#include "GPUScene.h"

AXION_NAMESPACE_BEGIN

namespace Core::Render {

void GPUScene::update( const Scene::Scene&    cpuScene,
                       Scene::Entity&         cameraEntity,
                       const MaterialLibrary& mtlLib,
                       const Extent2D&        resolution,
                       float                  deltaTime,
                       GPUSceneUpdateFlags    flags ) {

    bool transposeMatrices = ( flags & GPUSceneTransposeMatrices ) != GPUSceneNone;
    bool forceRaytrace     = ( flags & GPUSceneForceRaytrace ) != GPUSceneNone;
    bool sortInstances     = ( flags & GPUSceneSortInstances ) != GPUSceneNone;

    reset( deltaTime );

    processInstances( cpuScene, mtlLib, sortInstances, transposeMatrices, forceRaytrace );
    processLights( cpuScene );
    processEnvironments( cpuScene );
    processFrame( cameraEntity, resolution, transposeMatrices );

    runGC();
}

void GPUScene::reset( float dt ) {
    _currentFrameIndex++;
    _accumulatedTime += dt;
    _instances.clear();
    _lights.clear();
    _environments.clear();
    _sortedKeys.clear();
}

#pragma region Instances
#pragma endregion

void GPUScene::processInstances( const Scene::Scene& cpuScene, const MaterialLibrary& mtlLib, bool sort, bool transpose, bool forceRaytrace ) {

    u64 instanceCount = cpuScene.getRegistry().view<Scene::MeshComponent>().size();
    _instances.reserve( instanceCount );
    if ( sort )
        _sortedKeys.reserve( instanceCount );

    auto* assets = cpuScene.assets();

    // Ensure LUT is big enough for all current assets.
    if ( _meshCache.assetToCacheLUT.size() < assets->getMeshCount() )
        _meshCache.assetToCacheLUT.resize( assets->getMeshCount(), -1 );

    auto meshesView  = cpuScene.getRegistry().multiView<const Scene::MeshComponent, const Scene::TransformComponent>();
    auto mtlDirtyLUT = assets->getMaterialDirtyLUT();

    // Process Meshes & Materials
    for ( ECS::EntityID entity : meshesView )
    {
        const auto& meshComp  = meshesView.get<Scene::MeshComponent>( entity );
        const auto& transComp = meshesView.get<Scene::TransformComponent>( entity );

        u32 gpuMeshID     = processMesh( assets, meshComp.getMesh() );
        u32 gpuMaterialID = processMaterial( assets, mtlLib, mtlDirtyLUT, meshComp.getMaterial() );

        // --- INSTANCE DATA ---
        GPUInstance instance;
        instance.modelMatrix = transComp.getMatrix();

        if ( transpose )
            instance.modelMatrix = Math::MTX::transpose( instance.modelMatrix );

        instance.meshID     = gpuMeshID;
        instance.materialID = gpuMaterialID;
        instance.active     = meshComp.isVisible();
        instance.raytraced  = meshComp.isRaytraced() || forceRaytrace ? 1 : 0;

        _instances.pushBack( instance );

        if ( sort )
        {
            u32 topology = (u32)_meshCache.cache[gpuMeshID].aabbMax_Topology.w;
            u32 archID   = _materialCache.cache[gpuMaterialID].archetypeID;

            SortKey key;
            key.key                 = makeSortKey( archID, topology, gpuMeshID );
            key.originalInstanceIdx = (u32)_instances.size() - 1;

            _sortedKeys.pushBack( key );
        }
    }

    if ( sort )
    {
        std::sort( _sortedKeys.begin(), _sortedKeys.end(), []( const SortKey& a, const SortKey& b ) { return a.key < b.key; } );
    }
}

#pragma region Mesh
#pragma endregion

u32 GPUScene::processMesh( const Axion::Core::Assets::AssetManager* assets, const Axion::Core::Assets::MeshHandle& cpuMeshHandle ) {
    u32 cpuAssetID    = cpuMeshHandle.id;
    u32 gpuCacheIndex = 0;

    // Safety check: if asset ID is larger than current LUT size (rare race condition), grow
    if ( cpuAssetID >= _meshCache.assetToCacheLUT.size() )
        _meshCache.assetToCacheLUT.resize( cpuAssetID + 1, -1 );

    int cachedIndex = _meshCache.assetToCacheLUT[cpuAssetID];

    if ( cachedIndex != -1 )
    {
        // A. ALREADY IN CACHE
        gpuCacheIndex = (u32)cachedIndex;
    } else
    {
        // B. NEW ALLOCATION NEEDED
        if ( !_meshCache.freeIndexQueue.empty() )
        {
            // Recycle slot
            gpuCacheIndex = (u32)_meshCache.freeIndexQueue.front();
            _meshCache.freeIndexQueue.pop();
        } else
        {
            // Grow vector
            gpuCacheIndex = (u32)_meshCache.cache.size();
            _meshCache.cache.emplace_back();
        }

        // Register in LUT immediately
        _meshCache.assetToCacheLUT[cpuAssetID] = (int)gpuCacheIndex;

        _meshCache.cache[gpuCacheIndex].valid           = false;
        _meshCache.cache[gpuCacheIndex].originalAssetID = cpuAssetID;
    }

    // --- UPLOAD LOGIC ---
    auto& gpuMesh = _meshCache.cache[gpuCacheIndex];

    if ( !gpuMesh.valid )
    {
        // Slow indirection
        auto* cpuMesh = assets->getMesh( cpuMeshHandle );
        if ( cpuMesh )
        {
            gpuMesh.vertexCount      = cpuMesh->getVertexCount();
            gpuMesh.indexCount       = cpuMesh->getIndexCount();
            gpuMesh.aabbMin          = Math::Vec4( cpuMesh->getAABB().min, 0.0f );
            gpuMesh.aabbMax_Topology = Math::Vec4( cpuMesh->getAABB().max, (float)cpuMesh->getTopology() );
            gpuMesh.bsphere          = Math::Vec4( cpuMesh->getBoundingSphere().center, cpuMesh->getBoundingSphere().radius );
            gpuMesh.needsAS          = 1;

            _pendingMeshUploads.push( { gpuCacheIndex, cpuMesh->getGeometryDataRef() } );

            gpuMesh.valid = true;
        }
    }

    if ( gpuMesh.valid )
        gpuMesh.lastFrameUsed = _currentFrameIndex;

    return gpuCacheIndex;
}

#pragma region Material
#pragma endregion

u32 GPUScene::processMaterial( const Axion::Core::Assets::AssetManager*   assets,
                               const MaterialLibrary&                     mtlLib,
                               std::pair<const byte*, size_t>&            dirtyLUT,
                               const Axion::Core::Assets::MaterialHandle& cpuMtlHandle ) {
    u32 cpuAssetID    = cpuMtlHandle.id;
    u32 gpuCacheIndex = 0;

    if ( cpuAssetID >= _materialCache.assetToCacheLUT.size() )
        _materialCache.assetToCacheLUT.resize( cpuAssetID + 1, -1 );

    int cachedIndex = _materialCache.assetToCacheLUT[cpuAssetID];

    if ( cachedIndex != -1 )
    {
        // A. ALREADY IN CACHE
        gpuCacheIndex = (u32)cachedIndex;
    } else
    {
        // B. NEW ALLOCATION NEEDED
        if ( !_materialCache.freeIndexQueue.empty() )
        {
            // Recycle slot
            gpuCacheIndex = (u32)_materialCache.freeIndexQueue.front();
            _materialCache.freeIndexQueue.pop();
        } else
        {
            // Grow vector
            gpuCacheIndex = (u32)_materialCache.cache.size();
            _materialCache.cache.emplace_back();
        }

        // Register in LUT immediately
        _materialCache.assetToCacheLUT[cpuAssetID] = (int)gpuCacheIndex;

        _materialCache.cache[gpuCacheIndex].originalAssetID = cpuAssetID;
        _materialCache.cache[gpuCacheIndex].valid           = true;
    }

    // --- UPDATE LOGIC ---
    bool isDirty = false;
    if ( cpuAssetID < dirtyLUT.second )
    {
        isDirty = ( dirtyLUT.first[cpuAssetID] != 0 );
    }

    if ( isDirty )
    {
        // Slow indirection
        auto* cpuMaterial = assets->getMaterialBase( cpuMtlHandle );
        auto& gpuMtl      = _materialCache.cache[gpuCacheIndex];

        gpuMtl.payloadSize = cpuMaterial->getPayloadSize();
        gpuMtl.archetypeID = mtlLib.getArchetypeID( cpuMaterial->getArchetypeName() );

        Assets::Material::TextureResolver resolver = [&]( const Axion::Core::Assets::TextureHandle& h ) -> u32 {
            return processTexture( assets, h );
        };

        PendingMaterialUpload entry;
        entry.GPUMaterialID = gpuCacheIndex;

        entry.payload.resize( gpuMtl.payloadSize );
        cpuMaterial->writePayload( entry.payload.data(), resolver );

        _pendingMtlUploads.push( std::move( entry ) );

        cpuMaterial->clearDirty();
    }

    _materialCache.cache[gpuCacheIndex].lastFrameUsed = _currentFrameIndex;

    return gpuCacheIndex;
}

u32 GPUScene::processTexture( const Axion::Core::Assets::AssetManager* assets, const Axion::Core::Assets::TextureHandle& cpuHandle ) {
    if ( !cpuHandle.isValid() )
        return 0xFFFFFFFF;

    u32 cpuAssetID    = cpuHandle.id;
    u32 gpuCacheIndex = 0;

    if ( cpuAssetID >= _textureCache.assetToCacheLUT.size() )
        _textureCache.assetToCacheLUT.resize( cpuAssetID + 1, -1 );

    int cachedIndex = _textureCache.assetToCacheLUT[cpuAssetID];

    if ( cachedIndex != -1 )
    {
        // A. ALREADY IN CACHE
        gpuCacheIndex = (u32)cachedIndex;
    } else
    {
        // B. NEW ALLOCATION NEEDED
        if ( !_textureCache.freeIndexQueue.empty() )
        {
            gpuCacheIndex = (u32)_textureCache.freeIndexQueue.front();
            _textureCache.freeIndexQueue.pop();
        } else
        {
            gpuCacheIndex = (u32)_textureCache.cache.size();
            _textureCache.cache.emplace_back();
        }

        _textureCache.assetToCacheLUT[cpuAssetID] = (int)gpuCacheIndex;

        _textureCache.cache[gpuCacheIndex].valid           = false;
        _textureCache.cache[gpuCacheIndex].originalAssetID = cpuAssetID;
        // _textureCache.cache[gpuCacheIndex].slot            = gpuCacheIndex;
    }

    auto& gpuTex         = _textureCache.cache[gpuCacheIndex];
    gpuTex.lastFrameUsed = _currentFrameIndex;

    if ( !gpuTex.valid )
    {
        auto* tex = assets->getTexture( cpuHandle );

        PendingTextureUpload entry;

        entry.slot = gpuCacheIndex;

        entry.pixels = tex->getPixelsRef();
        entry.format = tex->getGPUFormat();
        entry.extent = tex->getSize();
        entry.name   = tex->getName();

        _pendingTextureUploads.push( std::move( entry ) );

        gpuTex.valid = true;
    }

    return gpuCacheIndex;
}

#pragma endregion
#pragma region Lights

void GPUScene::processLights( const Scene::Scene& cpuScene ) {
    _lights.reserve( cpuScene.getRegistry().view<Scene::LightComponent>().size() );
    auto lightView = cpuScene.getRegistry().multiView<const Scene::LightComponent, const Scene::TransformComponent>();

    for ( ECS::EntityID entity : lightView )
    {

        const auto& lightComp = lightView.get<Scene::LightComponent>( entity );
        const auto& transComp = lightView.get<Scene::TransformComponent>( entity );

        GPULight light;
        if ( !lightComp.isActive() )
            continue;

        light.pos_Intensity = Math::Vec4( transComp.getTranslation(), lightComp.getIntensity() );
        light.dir_Type      = Math::Vec4( transComp.forward(), (float)lightComp.getType() );

        light.settings = { Math::radians( lightComp.getInnerAngle() * 0.5f ),
                           Math::radians( lightComp.getOuterAngle() * 0.5f ),
                           0.0f,
                           lightComp.isActive() ? 1.0f : 0.0f };

        Math::Vec3 finalColor = lightComp.getColor();

        if ( lightComp.usesTemperature() )
        {
            Math::Vec3 kelvinColor = Axion::Math::kelvinToRGB( lightComp.getTemperature() );
            finalColor             = finalColor * kelvinColor;
        }

        light.col_Radius = {
            finalColor.x,
            finalColor.y,
            finalColor.z,
            lightComp.getRange() };

        _lights.pushBack( light );
    }
}

#pragma region Frame
#pragma endregion

void GPUScene::processFrame( Scene::Entity& cameraEntity, const Extent2D& resolution, bool transpose ) {
    if ( cameraEntity && cameraEntity.hasComponent<Scene::CameraComponent>() &&
         cameraEntity.hasComponent<Scene::TransformComponent>() )
    {
        // Frame General Information

        const auto& camComp   = cameraEntity.getComponent<Scene::CameraComponent>();
        const auto& transComp = cameraEntity.getComponent<Scene::TransformComponent>();

        auto proj  = camComp.getProjection( resolution );
        auto model = transComp.getMatrix();

        auto viewMat  = Math::MTX::inverse( model );
        auto viewProj = proj * viewMat;

        if ( transpose )
        {
            _frame.viewProj = Math::MTX::transpose( viewProj );
            _frame.invView  = Math::MTX::transpose( viewMat );
            _frame.invProj  = Math::MTX::transpose( Math::MTX::inverse( proj ) );
        } else
        {
            _frame.viewProj = viewProj;
            _frame.invView  = viewMat;
            _frame.invProj  = Math::MTX::inverse( proj );
        }

        _frame.camPos_Time = Math::Vec4(
            transComp.getTranslation().x,
            transComp.getTranslation().y,
            transComp.getTranslation().z,
            _accumulatedTime );

        float nearPlane = camComp.getNearPlane();
        float farPlane  = camComp.getFarPlane();
        _frame.res_Clip = Math::Vec4(
            (float)resolution.width,
            (float)resolution.height,
            nearPlane,
            farPlane );

        _frame.sceneParams = Math::Vec4(
            (float)_lights.size(),
            (float)_instances.size(),
            (float)_environments.size(),
            0.0f );

        _frame.sceneAABBMin = Math::Vec4( 0.0f );
        _frame.sceneAABBMax = Math::Vec4( 0.0f );

        Math::Frustum f = Math::createFrustumFromMatrix( viewProj );
        memcpy( _frame.frustrumPlanes, f.planes, sizeof( glm::vec4 ) * 6 );
    }
}

#pragma region Envs
#pragma endregion
void GPUScene::processEnvironments( const Scene::Scene& cpuScene ) {

    _environments.reserve( cpuScene.getRegistry().view<Scene::EnvironmentComponent>().size() );
    auto envView = cpuScene.getRegistry().multiView<const Scene::EnvironmentComponent, const Scene::TransformComponent>();

    for ( ECS::EntityID entity : envView )
    {
        GPUEnvironment env;

        const auto& envComp   = envView.get<Scene::EnvironmentComponent>( entity );
        const auto& transComp = envView.get<Scene::TransformComponent>( entity );

        if ( !envComp.isActive() )
            continue;

        env.groundColor_Type   = Math::Vec4( envComp.getGroundColor(), (float)envComp.getType() );
        env.skyColor_Intensity = Math::Vec4( envComp.getSkyColor(), envComp.getIntensity() );
        env.rotation_BlendDist = Math::Vec4( transComp.getRotation().x, transComp.getRotation().y, transComp.getRotation().z, envComp.getBlendDistance() );

        _environments.pushBack( env );
    }
}

#pragma region GC
#pragma endregion

void GPUScene::runGC() {
    for ( u64 i = 0; i < _meshCache.cache.size(); ++i )
    {
        auto& gpuMesh = _meshCache.cache[i];

        if ( gpuMesh.valid )
        {
            if ( _currentFrameIndex - gpuMesh.lastFrameUsed > _resourceTTL )
            {
                static const u32 VERTEX_STRIDE = sizeof( Assets::Vertex );
                static const u32 INDEX_STRIDE  = sizeof( u32 );

                _pendingMeshReleases.push( {
                    gpuMesh.vertexOffset,
                    gpuMesh.vertexCount * VERTEX_STRIDE,
                    gpuMesh.indexOffset,
                    gpuMesh.indexCount * INDEX_STRIDE,
                    (u32)i // Cache Slot Index
                } );

                if ( gpuMesh.originalAssetID < _meshCache.assetToCacheLUT.size() )
                {
                    _meshCache.assetToCacheLUT[gpuMesh.originalAssetID] = -1;
                }

                _meshCache.freeIndexQueue.push( (u32)i );

                gpuMesh.valid        = false;
                gpuMesh.vertexOffset = 0;
                gpuMesh.indexOffset  = 0;
                gpuMesh.vertexCount  = 0;

                AXION_LOG_INFO( Logger::Module::Core, "GC: Recycled mesh slot {}", i );
            }
        }
    }
    for ( u64 i = 0; i < _materialCache.cache.size(); ++i )
    {
        auto& gpuMat = _materialCache.cache[i];

        if ( gpuMat.valid )
        {
            if ( _currentFrameIndex - gpuMat.lastFrameUsed > _resourceTTL )
            {
                _pendingMtlReleases.push( {
                    gpuMat.bufferOffset,
                    gpuMat.payloadSize,
                    (u32)i // Cache Slot Index
                } );

                if ( gpuMat.originalAssetID < _materialCache.assetToCacheLUT.size() )
                {
                    _materialCache.assetToCacheLUT[gpuMat.originalAssetID] = -1;
                }

                _materialCache.freeIndexQueue.push( (u32)i );

                gpuMat.valid        = false;
                gpuMat.bufferOffset = 0;
                gpuMat.payloadSize  = 0;
                gpuMat.archetypeID  = 0;

                AXION_LOG_INFO( Logger::Module::Core, "GC: Recycled material slot {}", i );
            }
        }
    }
}

bool GPUScene::hasPendingUploads() const {
    return !_pendingMeshUploads.empty() || !_pendingMtlUploads.empty() || !_pendingTextureUploads.empty();
}

bool GPUScene::hasPendingReleases() const {
    return !_pendingMeshReleases.empty() || !_pendingMtlReleases.empty() || !_pendingTextureReleases.empty();
}

} // namespace Core::Render
AXION_NAMESPACE_END