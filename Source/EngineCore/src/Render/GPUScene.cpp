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

    ulong instanceCount = cpuScene.getRegistry().view<Scene::MeshComponent>().size();
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

        uint gpuMeshID     = processMesh( assets, meshComp.mesh );
        uint gpuMaterialID = processMaterial( assets, mtlLib, mtlDirtyLUT, meshComp.material );

        // --- INSTANCE DATA ---
        GPUInstance instance;
        instance.modelMatrix = transComp.getMatrix();

        if ( transpose )
            instance.modelMatrix = Math::MTX::transpose( instance.modelMatrix );

        instance.meshID     = gpuMeshID;
        instance.materialID = gpuMaterialID;
        instance.active     = meshComp.visible;
        instance.raytraced  = meshComp.raytraced || forceRaytrace ? 1 : 0;

        _instances.push_back( instance );

        if ( sort )
        {
            uint topology = (uint)_meshCache.cache[gpuMeshID].aabbMax_Topology.w;
            uint archID   = _materialCache.cache[gpuMaterialID].archetypeID;

            SortKey key;
            key.key                 = makeSortKey( archID, topology, gpuMeshID );
            key.originalInstanceIdx = (uint)_instances.size() - 1;

            _sortedKeys.push_back( key );
        }
    }

    if ( sort )
    {
        std::sort( _sortedKeys.begin(), _sortedKeys.end(), []( const SortKey& a, const SortKey& b ) { return a.key < b.key; } );
    }
}

#pragma region Mesh
#pragma endregion

uint GPUScene::processMesh( const Axion::Core::Assets::AssetManager* assets, const Axion::Core::Assets::MeshHandle& cpuMeshHandle ) {
    uint cpuAssetID    = cpuMeshHandle.id;
    uint gpuCacheIndex = 0;

    // Safety check: if asset ID is larger than current LUT size (rare race condition), grow
    if ( cpuAssetID >= _meshCache.assetToCacheLUT.size() )
        _meshCache.assetToCacheLUT.resize( cpuAssetID + 1, -1 );

    int cachedIndex = _meshCache.assetToCacheLUT[cpuAssetID];

    if ( cachedIndex != -1 )
    {
        // A. ALREADY IN CACHE
        gpuCacheIndex = (uint)cachedIndex;
    } else
    {
        // B. NEW ALLOCATION NEEDED
        if ( !_meshCache.freeIndexQueue.empty() )
        {
            // Recycle slot
            gpuCacheIndex = _meshCache.freeIndexQueue.front();
            _meshCache.freeIndexQueue.pop();
        } else
        {
            // Grow vector
            gpuCacheIndex = (uint)_meshCache.cache.size();
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

uint GPUScene::processMaterial( const Axion::Core::Assets::AssetManager*   assets,
                                const MaterialLibrary&                     mtlLib,
                                std::pair<const uchar*, size_t>&           dirtyLUT,
                                const Axion::Core::Assets::MaterialHandle& cpuMtlHandle ) {
    uint cpuAssetID    = cpuMtlHandle.id;
    uint gpuCacheIndex = 0;

    if ( cpuAssetID >= _materialCache.assetToCacheLUT.size() )
        _materialCache.assetToCacheLUT.resize( cpuAssetID + 1, -1 );

    int cachedIndex = _materialCache.assetToCacheLUT[cpuAssetID];

    if ( cachedIndex != -1 )
    {
        // A. ALREADY IN CACHE
        gpuCacheIndex = (uint)cachedIndex;
    } else
    {
        // B. NEW ALLOCATION NEEDED
        if ( !_materialCache.freeIndexQueue.empty() )
        {
            // Recycle slot
            gpuCacheIndex = _materialCache.freeIndexQueue.front();
            _materialCache.freeIndexQueue.pop();
        } else
        {
            // Grow vector
            gpuCacheIndex = (uint)_materialCache.cache.size();
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
        gpuMtl.archetypeID = mtlLib.getArchetypeID( std::string( cpuMaterial->getArchetypeName() ) );

        Assets::Material::TextureResolver resolver = [&]( const Axion::Core::Assets::TextureHandle& h ) -> uint {
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

uint GPUScene::processTexture( const Axion::Core::Assets::AssetManager* assets, const Axion::Core::Assets::TextureHandle& cpuHandle ) {
    if ( !cpuHandle.isValid() )
        return 0xFFFFFFFF;

    uint cpuAssetID    = cpuHandle.id;
    uint gpuCacheIndex = 0;

    if ( cpuAssetID >= _textureCache.assetToCacheLUT.size() )
        _textureCache.assetToCacheLUT.resize( cpuAssetID + 1, -1 );

    int cachedIndex = _textureCache.assetToCacheLUT[cpuAssetID];

    if ( cachedIndex != -1 )
    {
        // A. ALREADY IN CACHE
        gpuCacheIndex = (uint)cachedIndex;
    } else
    {
        // B. NEW ALLOCATION NEEDED
        if ( !_textureCache.freeIndexQueue.empty() )
        {
            gpuCacheIndex = _textureCache.freeIndexQueue.front();
            _textureCache.freeIndexQueue.pop();
        } else
        {
            gpuCacheIndex = (uint)_textureCache.cache.size();
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
        if ( !lightComp.active )
            continue;

        light.pos_Intensity = Math::Vec4( transComp.translation, lightComp.intensity );
        light.dir_Type      = Math::Vec4( transComp.forward(), (float)lightComp.type );

        light.settings = { Math::radians( lightComp.innerAngle * 0.5f ),
                           Math::radians( lightComp.outerAngle * 0.5f ),
                           0.0f,
                           lightComp.active ? 1.0f : 0.0f };

        Math::Vec3 finalColor = lightComp.color;

        if ( lightComp.useTemperature )
        {
            Math::Vec3 kelvinColor = Axion::Math::kelvinToRGB( lightComp.temperature );
            finalColor             = finalColor * kelvinColor;
        }

        light.col_Radius = {
            finalColor.x,
            finalColor.y,
            finalColor.z,
            lightComp.range };

        _lights.push_back( light );
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
            transComp.translation.x,
            transComp.translation.y,
            transComp.translation.z,
            _accumulatedTime );

        float nearPlane = camComp.nearPlane;
        float farPlane  = camComp.farPlane;
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

        if ( !envComp.active )
            continue;

        env.groundColor_Type   = Math::Vec4( envComp.groundColor, (float)envComp.type );
        env.skyColor_Intensity = Math::Vec4( envComp.skyColor, envComp.intensity );
        env.rotation_BlendDist = Math::Vec4( transComp.rotation.x, transComp.rotation.y, transComp.rotation.z, envComp.blendDistance );

        _environments.push_back( env );
    }
}

#pragma region GC
#pragma endregion

void GPUScene::runGC() {
    for ( ulong i = 0; i < _meshCache.cache.size(); ++i )
    {
        auto& gpuMesh = _meshCache.cache[i];

        if ( gpuMesh.valid )
        {
            if ( _currentFrameIndex - gpuMesh.lastFrameUsed > _resourceTTL )
            {
                static const uint VERTEX_STRIDE = sizeof( Assets::Vertex );
                static const uint INDEX_STRIDE  = sizeof( uint );

                _pendingMeshReleases.push( {
                    gpuMesh.vertexOffset,
                    gpuMesh.vertexCount * VERTEX_STRIDE,
                    gpuMesh.indexOffset,
                    gpuMesh.indexCount * INDEX_STRIDE,
                    (uint)i // Cache Slot Index
                } );

                if ( gpuMesh.originalAssetID < _meshCache.assetToCacheLUT.size() )
                {
                    _meshCache.assetToCacheLUT[gpuMesh.originalAssetID] = -1;
                }

                _meshCache.freeIndexQueue.push( (uint)i );

                gpuMesh.valid        = false;
                gpuMesh.vertexOffset = 0;
                gpuMesh.indexOffset  = 0;
                gpuMesh.vertexCount  = 0;

                AXION_LOG_INFO( Logger::Module::Core, "GC: Recycled mesh slot {}", i );
            }
        }
    }
    for ( ulong i = 0; i < _materialCache.cache.size(); ++i )
    {
        auto& gpuMat = _materialCache.cache[i];

        if ( _currentFrameIndex - gpuMat.lastFrameUsed > _resourceTTL )
        {
            _pendingMtlReleases.push( {
                gpuMat.bufferOffset,
                gpuMat.payloadSize,
                (uint)i // Cache Slot Index
            } );

            if ( gpuMat.originalAssetID < _materialCache.assetToCacheLUT.size() )
            {
                _materialCache.assetToCacheLUT[gpuMat.originalAssetID] = -1;
            }

            _materialCache.freeIndexQueue.push( (uint)i );

            gpuMat.valid        = false;
            gpuMat.bufferOffset = 0;
            gpuMat.payloadSize  = 0;
            gpuMat.archetypeID  = 0;

            AXION_LOG_INFO( Logger::Module::Core, "GC: Recycled material slot {}", i );
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