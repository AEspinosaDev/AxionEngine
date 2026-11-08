#include "GPUScene.h"
#include <cmath>

AXION_NAMESPACE_BEGIN

namespace Core::Render {

void GPUScene::update( const Scene::Scene&       cpuScene,
                       Scene::Entity&            cameraEntity,
                       IMaterialLibrary&         mtlLib,
                       const Extent2D&           resolution,
                       float                     deltaTime,
                       GPUSceneShadowMapSettings smSettings,
                       GPUSceneUpdateFlags       flags ) {

    bool transposeMatrices = ( flags & GPUSceneTransposeMatrices ) != GPUSceneNone;
    bool forceRaytrace     = ( flags & GPUSceneForceRaytrace ) != GPUSceneNone;
    bool sortInstances     = ( flags & GPUSceneSortInstances ) != GPUSceneNone;
    bool jitterCamera      = ( flags & GPUSceneEnableCameraJitter ) != GPUSceneNone;
    bool processShadows    = ( flags & GPUSceneProcessShadowMaps ) != GPUSceneNone;

    reset( deltaTime );

    processInstances( cpuScene, mtlLib, sortInstances, transposeMatrices, forceRaytrace );
    processParticles( cpuScene, mtlLib, sortInstances, transposeMatrices );
    processLights( cpuScene, cameraEntity, resolution, smSettings, transposeMatrices, processShadows );
    processEnvironments( cpuScene );
    processFrame( cameraEntity, resolution, transposeMatrices, jitterCamera );

    runGC();
}

void GPUScene::reset( float dt ) {
    _currentFrameIndex++;
    _accumulatedTime += dt;
    _instances.clear();
    _lights.clear();
    _environments.clear();
    _particleSystems.clear();
    _shadows.clear();
    _shadowViews.clear();
    _sortedKeys.clear();
}

#pragma region Instances
#pragma endregion

void GPUScene::processInstances( const Scene::Scene& cpuScene, IMaterialLibrary& mtlLib, bool sort, bool transpose, bool forceRaytrace ) {

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

        // An invalid handle can't index the asset LUTs
        if ( !meshComp.getMesh().isValid() || !meshComp.getMaterial().isValid() )
        {
            AXION_LOG_WARN_ONCE( Logger::Module::Core, "GPUScene: instance without mesh or material skipped" );
            continue;
        }

        u32 gpuMeshID     = processMesh( assets, meshComp.getMesh() );
        u32 gpuMaterialID = processMaterial( assets, mtlLib, mtlDirtyLUT, meshComp.getMaterial() );

        // --- INSTANCE DATA ---
        GPUInstance instance;
        instance.modelMatrix     = transComp.getMatrix();
        instance.prevModelMatrix = exchangePrevWorldMatrix( entity, instance.modelMatrix );

        if ( transpose )
        {
            instance.modelMatrix     = Math::MTX::transpose( instance.modelMatrix );
            instance.prevModelMatrix = Math::MTX::transpose( instance.prevModelMatrix );
        }

        instance.meshID     = gpuMeshID;
        instance.materialID = gpuMaterialID;
        instance.active     = meshComp.isVisible();
        instance.raytraced  = meshComp.isRaytraced() || forceRaytrace ? 1 : 0;

        _instances.pushBack( instance );

        if ( sort )
        {
            u32 psoBundleID = _materialCache.cache[gpuMaterialID].psoBundleID;

            // Unregistered archetype: instance data exists but it is never drawn
            if ( psoBundleID == AXION_INVALID_U32 )
            {
                AXION_LOG_WARN_ONCE( Logger::Module::Core, "GPUScene: instance with invalid material bundle skipped" );
                continue;
            }

            SortKey key;
            key.key                 = makeSortKey( psoBundleID, gpuMeshID );
            key.originalInstanceIdx = (u32)_instances.size() - 1;

            _sortedKeys.pushBack( key );
        }
    }

    // Quick sort
    if ( sort )
    {
        std::sort( _sortedKeys.begin(), _sortedKeys.end(), []( const SortKey& a, const SortKey& b ) { return a.key < b.key; } );
    }
}

Math::Mat4 GPUScene::exchangePrevWorldMatrix( ECS::EntityID entity, const Math::Mat4& world ) {
    if ( entity >= _prevWorldMatrices.size() )
        _prevWorldMatrices.resize( (size_t)entity + 1 );

    PrevWorldMatrix& prev = _prevWorldMatrices[entity];

    // Only a matrix stored in the previous frame is a valid origin for the motion
    const bool       seenLastFrame = prev.frame != AXION_INVALID_U32 && prev.frame + 1 == _currentFrameIndex;
    const Math::Mat4 result        = seenLastFrame ? prev.matrix : world;

    prev.matrix = world;
    prev.frame  = _currentFrameIndex;
    return result;
}

#pragma region Particles
#pragma endregion

void GPUScene::processParticles( const Scene::Scene& cpuScene, IMaterialLibrary& mtlLib, bool sort, bool transpose ) {

    _particleSystems.reserve( cpuScene.getRegistry().view<Scene::ParticleSystemComponent>().size() );

    auto* assets      = cpuScene.assets();
    auto  mtlDirtyLUT = assets->getMaterialDirtyLUT();
    auto  systemsView = cpuScene.getRegistry().multiView<const Scene::ParticleSystemComponent, const Scene::TransformComponent>();

    for ( ECS::EntityID entity : systemsView )
    {
        const auto& systemComp = systemsView.get<Scene::ParticleSystemComponent>( entity );
        const auto& transComp  = systemsView.get<Scene::TransformComponent>( entity );

        if ( !systemComp.getMesh().isValid() || !systemComp.getMaterial().isValid() || systemComp.getCount() == 0 )
            continue;

        if ( _particleSystems.size() >= MAX_GPU_PARTICLE_SYSTEMS || systemComp.getCount() > MAX_PARTICLES_PER_SYSTEM )
        {
            AXION_LOG_WARN_ONCE( Logger::Module::Core, "GPUScene: particle system skipped, limits are {} systems and {} particles per system", MAX_GPU_PARTICLE_SYSTEMS, MAX_PARTICLES_PER_SYSTEM );
            continue;
        }

        u32 gpuMeshID     = processMesh( assets, systemComp.getMesh() );
        u32 gpuMaterialID = processMaterial( assets, mtlLib, mtlDirtyLUT, systemComp.getMaterial() );

        // Unregistered archetype: it could never be drawn
        if ( _materialCache.cache[gpuMaterialID].psoBundleID == AXION_INVALID_U32 )
        {
            AXION_LOG_WARN_ONCE( Logger::Module::Core, "GPUScene: particle system with invalid material bundle skipped" );
            continue;
        }

        GPUParticleSystem gpuSystem;
        gpuSystem.modelMatrix = transComp.getMatrix();

        if ( transpose )
            gpuSystem.modelMatrix = Math::MTX::transpose( gpuSystem.modelMatrix );

        gpuSystem.meshID      = gpuMeshID;
        gpuSystem.materialID  = gpuMaterialID;
        gpuSystem.count       = systemComp.getCount();
        gpuSystem.extent_Size = Math::Vec4( systemComp.getExtent(), systemComp.getSize() );
        gpuSystem.speed       = systemComp.getSpeed();
        gpuSystem.atomicity   = systemComp.getAtomicity();
        gpuSystem.behavior    = (u32)systemComp.getBehavior();
        gpuSystem.seed        = systemComp.getSeed();

        gpuSystem.sizeVariation = systemComp.getSizeVariation();

        gpuSystem.visible    = systemComp.isVisible();
        gpuSystem.update     = systemComp.isUpdating();
        gpuSystem.faceCamera = systemComp.facesCamera();

        // Atomicity is not supported yet: every particle has its own state
        const u32 bufferSlot = processParticleBuffer( entity, systemComp.getCount() );
        auto&     buffer     = _particleBufferCache.cache[bufferSlot];

        gpuSystem.bufferSlot           = bufferSlot;
        gpuSystem.bufferSRVHeapID      = buffer.srvHeapID;
        gpuSystem.bufferUAVHeapID      = buffer.uavHeapID;
        gpuSystem.visibleListSRVHeapID = buffer.visibleListSRVHeapID;
        gpuSystem.visibleListUAVHeapID = buffer.visibleListUAVHeapID;

        if ( buffer.seedingVersion != systemComp.getSeedingVersion() )
        {
            buffer.seedingVersion = systemComp.getSeedingVersion();
            buffer.needsReset     = true;
        }
        gpuSystem.reset   = buffer.needsReset;
        buffer.needsReset = false;

        _particleSystems.pushBack( gpuSystem );
    }

    // Quick sort. The systems themselves, not keys: their index (commands, visible counts, VisBuffer) only lives one frame
    if ( sort )
    {
        std::sort( _particleSystems.begin(), _particleSystems.end(), [this]( const GPUParticleSystem& a, const GPUParticleSystem& b ) {
            return _materialCache.cache[a.materialID].psoBundleID < _materialCache.cache[b.materialID].psoBundleID;
        } );
    }
}

u32 GPUScene::processParticleBuffer( ECS::EntityID entity, u32 particleCount ) {

    if ( entity >= _particleBufferCache.assetToCacheLUT.size() )
        _particleBufferCache.assetToCacheLUT.resize( entity + 1, -1 );

    int cachedIndex = _particleBufferCache.assetToCacheLUT[entity];

    // Another count needs another buffer. The old one may be in use by frames in flight:
    // nobody refreshes it anymore, so the GC frees it once it is safe
    if ( cachedIndex != -1 && _particleBufferCache.cache[cachedIndex].particleCount != particleCount )
        cachedIndex = -1;

    u32 slot = (u32)cachedIndex;
    if ( cachedIndex == -1 )
    {
        if ( !_particleBufferCache.freeIndexQueue.empty() )
        {
            slot = (u32)_particleBufferCache.freeIndexQueue.front();
            _particleBufferCache.freeIndexQueue.pop();
        } else
        {
            slot = (u32)_particleBufferCache.cache.size();
            _particleBufferCache.cache.emplace_back();
        }

        _particleBufferCache.assetToCacheLUT[entity] = (int)slot;

        GPUParticleBuffer& buffer = _particleBufferCache.cache[slot];
        buffer                    = {};
        buffer.ownerEntity        = entity;
        buffer.particleCount      = particleCount;
        buffer.valid              = true;

        _pendingParticleBufferUploads.push( { slot, particleCount } );
    }

    _particleBufferCache.cache[slot].lastFrameUsed = _currentFrameIndex;
    return slot;
}

#pragma endregion
#pragma region Shadow

u32 GPUScene::processCSM( ECS::EntityID entity, u32 shadowRes ) {

    if ( entity >= _csmCache.assetToCacheLUT.size() )
        _csmCache.assetToCacheLUT.resize( entity + 1, -1 );

    int cachedIndex = _csmCache.assetToCacheLUT[entity];

    // Another res needs another buffer. The old one may be in use by frames in flight:
    // nobody refreshes it anymore, so the GC frees it once it is safe
    if ( cachedIndex != -1 && _csmCache.cache[cachedIndex].res != shadowRes )
        cachedIndex = -1;

    u32 slot = (u32)cachedIndex;
    if ( cachedIndex == -1 )
    {
        if ( !_csmCache.freeIndexQueue.empty() )
        {
            slot = (u32)_csmCache.freeIndexQueue.front();
            _csmCache.freeIndexQueue.pop();
        } else
        {
            slot = (u32)_csmCache.cache.size();
            _csmCache.cache.emplace_back();
        }

        _csmCache.assetToCacheLUT[entity] = (int)slot;

        GPUCascadedSM& csm = _csmCache.cache[slot];
        csm                = {};
        csm.ownerEntity    = entity;
        csm.res            = shadowRes;
        csm.valid          = true;

        _pendingCSMUploads.push( { slot, shadowRes } );
    }

    _csmCache.cache[slot].lastFrameUsed = _currentFrameIndex;
    return slot;
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

            const auto* meshletData = cpuMesh->getMeshletData();
            if ( !meshletData )
                AXION_LOG_ERROR( Logger::Module::Core, "Mesh [{}] has no meshlets (disableMeshlets / non indexed triangle list): mesh shading passes will skip it", cpuMesh->getName() );
            gpuMesh.meshletCount         = meshletData ? (u32)meshletData->meshlets.size() : 0;
            gpuMesh.meshletVertexCount   = meshletData ? (u32)meshletData->vertexIndices.size() : 0;
            gpuMesh.meshletPrimitiveSize = meshletData ? (u32)meshletData->primitiveIndices.size() : 0;

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
                               IMaterialLibrary&                          mtlLib,
                               std::pair<const byte*, size_t>&            dirtyLUT,
                               const Axion::Core::Assets::MaterialHandle& cpuMtlHandle ) {
    u32 cpuAssetID    = cpuMtlHandle.id;
    u32 gpuCacheIndex = 0;

    if ( cpuAssetID >= _materialCache.assetToCacheLUT.size() )
        _materialCache.assetToCacheLUT.resize( cpuAssetID + 1, -1 );

    int cachedIndex = _materialCache.assetToCacheLUT[cpuAssetID];

    // --- UPDATE LOGIC ---
    bool isDirty = false;
    if ( cpuAssetID < dirtyLUT.second )
    {
        isDirty = ( dirtyLUT.first[cpuAssetID] != 0 );
    }

    if ( cachedIndex != -1 )
    {
        // A. ALREADY IN CACHE
        gpuCacheIndex = (u32)cachedIndex;
    } else
    {
        // B. NEW ALLOCATION NEEDED
        isDirty = true;
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
            _materialToTextureMap.emplace_back();
        }

        // Register in LUT immediately
        _materialCache.assetToCacheLUT[cpuAssetID] = (int)gpuCacheIndex;

        _materialCache.cache[gpuCacheIndex].originalAssetID = cpuAssetID;
        _materialCache.cache[gpuCacheIndex].valid           = true;
        _materialCache.cache[gpuCacheIndex].bufferOffset    = AXION_INVALID_U32;
    }

    if ( isDirty )
    {
        // Slow indirection
        auto* cpuMaterial = assets->getMaterialBase( cpuMtlHandle );
        auto& gpuMtl      = _materialCache.cache[gpuCacheIndex];

        // Reset textures in this material
        for ( u32 oldTexGpuHandle : _materialToTextureMap[gpuCacheIndex] )
            if ( _textureCache.cache[oldTexGpuHandle].refCount > 0 )
                _textureCache.cache[oldTexGpuHandle].refCount--;
        _materialToTextureMap[gpuCacheIndex].clear();

        gpuMtl.payloadSize   = cpuMaterial->getPayloadSize();
        const char* archName = cpuMaterial->getArchetypeInfo().name; // Subscribe bundle to matlib given archetype and renderstate
        gpuMtl.psoBundleID   = mtlLib.updateArchetypeState( mtlLib.getArchetypeID( archName ), cpuMaterial->getRenderState() );

        Assets::Material::TextureResolver resolver = [&]( const Axion::Core::Assets::TextureHandle& h ) -> u32 {
            return processTexture( assets, h, gpuCacheIndex );
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

u32 GPUScene::processTexture( const Axion::Core::Assets::AssetManager*  assets,
                              const Axion::Core::Assets::TextureHandle& cpuHandle,
                              u32                                       materialGpuCacheIndex ) {
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
        _textureCache.cache[gpuCacheIndex].heapID          = AXION_INVALID_U32; // Set LATER
    }

    auto& gpuTex = _textureCache.cache[gpuCacheIndex];

    gpuTex.refCount++;
    _materialToTextureMap[materialGpuCacheIndex].pushBack( gpuCacheIndex );

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

void GPUScene::processLights( const Scene::Scene&              cpuScene,
                              Scene::Entity&                   cameraEntity,
                              const Extent2D&                  resolution,
                              const GPUSceneShadowMapSettings& smSettings,
                              bool                             transpose,
                              bool                             processShadows ) {
    auto lightCount = cpuScene.getRegistry().view<Scene::LightComponent>().size();
    _lights.reserve( lightCount );
    _shadows.reserve( lightCount );
    _shadowViews.reserve( lightCount * 6 );

    // The cascades are fitted to the camera frustum: without a camera, no light gets a CSM
    const bool hasCamera = cameraEntity && cameraEntity.hasComponent<Scene::CameraComponent>() &&
                           cameraEntity.hasComponent<Scene::TransformComponent>();

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

        u32 shadowIndex = AXION_INVALID_U32;
        if ( processShadows )
        {
            // Process shadow (CSM: directional lights only, other types will go to the shadow atlas)
            //----------------------------------------------------------------------------------------
            if ( lightComp.castsShadows() && lightComp.getType() == Scene::LightComponent::Type::Directional && hasCamera )
            {
                // Process persistent CSM data
                const u32 csmSlot = processCSM( entity, smSettings.cascadedSMRes ); 
                const u32 csmRes  = _csmCache.cache[csmSlot].res;

                const auto& camComp     = cameraEntity.getComponent<Scene::CameraComponent>();
                const auto& camTrans    = cameraEntity.getComponent<Scene::TransformComponent>();
                const float aspect      = resolution.height > 0 ? (float)resolution.width / (float)resolution.height : 1.0f;
                const bool  perspective = camComp.getProjectionType() == Scene::CameraComponent::ProjectionType::Perspective;

                // Unjittered camera: the TAA jitter must not move the cascades.
                // The log splits need a near plane > 0 (an ortho camera may have 0)
                const float shadowNear = Math::max( camComp.getNearPlane(), 1e-3f );
                const float shadowFar  = Math::max( Math::min( camComp.getFarPlane(), lightComp.getShadowDistance() ), shadowNear * 2.0f );
                const auto  lightDir   = Math::normalize( transComp.forward() );

                auto createCascadeViewProj = []( const Math::Vec3& dir, const Math::BoundingSphere& sphere, u32 mapRes ) {
                    // Rotation only: it changes only when the light rotates, so the snapping grid stays fixed in the world
                    const Math::Vec3 up        = std::abs( dir.y ) > 0.99f ? Math::Vec3( 0.0f, 0.0f, 1.0f ) : Math::Vec3( 0.0f, 1.0f, 0.0f );
                    const Math::Mat4 lightView = Math::MTX::lookAt( Math::Vec3( 0.0f ), dir, up );

                    Math::Vec3 center = Math::Vec3( lightView * Math::Vec4( sphere.center, 1.0f ) );

                    const float texelSize = 2.0f * sphere.radius / (float)mapRes;
                    center.x              = std::floor( center.x / texelSize ) * texelSize;
                    center.y              = std::floor( center.y / texelSize ) * texelSize;

                    // The light view looks down -Z: the sphere covers the view distances [-center.z - r, -center.z + r].
                    // Casters between the light and the box fall in front of the near plane: the shadow pass must disable depth clip (pancaking) to keep them
                    const Math::Mat4 proj = Math::MTX::ortho( center.x - sphere.radius, center.x + sphere.radius, center.y - sphere.radius, center.y + sphere.radius, -center.z - sphere.radius, -center.z + sphere.radius );
                    return proj * lightView;
                };

                GPUShadow shadow {};
                shadow.mapSRVHeapID = _csmCache.cache[csmSlot].srvHeapID; // INVALID until its texture exists (next frame): skip the shadow then
                shadow.firstView    = (u32)_shadowViews.size();
                shadow.viewCount    = smSettings.maxCascades;
                shadow.splitsFar    = Math::Vec4( 0.0f );

                float sliceNear = shadowNear;
                for ( u32 cascade = 0; cascade < smSettings.maxCascades; ++cascade )
                {
                    const float sliceFar = Math::computeCascadeSplit( cascade, smSettings.maxCascades, shadowNear, shadowFar, lightComp.getCascadeSplitLambda() );

                    const Math::BoundingSphere sphere =
                        perspective ? Math::computePerspectiveSliceSphere( camTrans.getTranslation(), camTrans.forward(), Math::radians( camComp.getFOV() ), aspect, sliceNear, sliceFar )
                                    : Math::computeOrthoSliceSphere( camTrans.getTranslation(), camTrans.forward(), camComp.getOrthoSize(), aspect, sliceNear, sliceFar );

                    const Math::Mat4 viewProj = createCascadeViewProj( lightDir, sphere, csmRes );

                    GPUShadowView view;
                    view.viewProj = transpose ? Math::MTX::transpose( viewProj ) : viewProj;
                    view.params   = Math::Vec4( 2.0f * sphere.radius / (float)csmRes, // Texel world size
                                                (float)cascade,                       // Slice
                                                0.0f,
                                                0.0f );
                    _shadowViews.pushBack( view );

                    shadow.splitsFar[cascade] = sliceFar;
                    sliceNear                 = sliceFar;
                }

                shadowIndex = (u32)_shadows.size();
                _shadows.pushBack( shadow );
            }

            // TBD: Normal shadow map in shadow atlas
        }

        light.settings = { Math::radians( lightComp.getInnerAngle() * 0.5f ),
                           Math::radians( lightComp.getOuterAngle() * 0.5f ),
                           shadowIndex == AXION_INVALID_U32 ? -1.0f : (float)shadowIndex,
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

void GPUScene::processFrame( Scene::Entity& cameraEntity, const Extent2D& resolution, bool transpose, bool jitter ) {
    if ( cameraEntity && cameraEntity.hasComponent<Scene::CameraComponent>() &&
         cameraEntity.hasComponent<Scene::TransformComponent>() )
    {
        // Frame General Information

        const auto& camComp   = cameraEntity.getComponent<Scene::CameraComponent>();
        const auto& transComp = cameraEntity.getComponent<Scene::TransformComponent>();

        auto proj       = camComp.getProjection( resolution );
        auto jitterProj = proj;
        if ( jitter )
        {
            auto getProjectionJitter = []( u32 frameIndex, u32 screenWidth, u32 screenHeight ) {
                const int index = (int)( frameIndex % 16 ) + 1;

                const float jitterX = Math::halton( index, 2 ) - 0.5f;
                const float jitterY = Math::halton( index, 3 ) - 0.5f;

                return Math::Vec2( jitterX * 2.0f / (float)screenWidth, jitterY * 2.0f / (float)screenHeight );
            };

            const Math::Vec2 jitterNDC = getProjectionJitter( _currentFrameIndex, resolution.width, resolution.height );
            jitterProj                 = Math::MTX::translate( Math::Mat4( 1.0f ), Math::Vec3( jitterNDC.x, jitterNDC.y, 0.0f ) ) * proj;
        }

        // View
        auto model   = transComp.getMatrix();
        auto viewMat = Math::MTX::inverse( model );

        auto viewProj       = proj * viewMat;
        auto jitterViewProj = jitterProj * viewMat;
        auto prevViewProj   = _hasPrevViewProj ? _prevViewProj : viewProj;

        if ( transpose )
        {
            _frame.viewProj         = Math::MTX::transpose( viewProj );
            _frame.jitteredViewProj = Math::MTX::transpose( jitterViewProj );
            _frame.prevViewProj     = Math::MTX::transpose( prevViewProj );
            _frame.invView          = Math::MTX::transpose( viewMat );
            _frame.invProj          = Math::MTX::transpose( Math::MTX::inverse( proj ) );
        } else
        {
            _frame.viewProj         = viewProj;
            _frame.jitteredViewProj = jitterViewProj;
            _frame.prevViewProj     = prevViewProj;
            _frame.invView          = viewMat;
            _frame.invProj          = Math::MTX::inverse( proj );
        }

        // Previous of the next frame
        _prevViewProj    = viewProj;
        _hasPrevViewProj = true;

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
                static const u32 VERTEX_STRIDE  = sizeof( Assets::Vertex );
                static const u32 INDEX_STRIDE   = sizeof( u32 );
                static const u32 MESHLET_STRIDE = sizeof( Assets::Meshlet );

                _pendingMeshReleases.push( { .geometryPageID         = gpuMesh.geometryPageID,
                                             .vertexOffset           = gpuMesh.vertexOffset,
                                             .vertexSize             = gpuMesh.vertexCount * VERTEX_STRIDE,
                                             .indexOffset            = gpuMesh.indexOffset,
                                             .indexSize              = gpuMesh.indexCount * INDEX_STRIDE,
                                             .meshletOffset          = gpuMesh.meshletOffset,
                                             .meshletSize            = gpuMesh.meshletCount * MESHLET_STRIDE,
                                             .meshletVertexOffset    = gpuMesh.meshletVertexOffset,
                                             .meshletVertexSize      = gpuMesh.meshletVertexCount * INDEX_STRIDE,
                                             .meshletPrimitiveOffset = gpuMesh.meshletPrimitiveOffset,
                                             .meshletPrimitiveSize   = gpuMesh.meshletPrimitiveSize,
                                             .GPUMeshID              = (u32)i } ); // Cache Slot Index

                if ( gpuMesh.originalAssetID < _meshCache.assetToCacheLUT.size() )
                {
                    _meshCache.assetToCacheLUT[gpuMesh.originalAssetID] = -1;
                }

                _meshCache.freeIndexQueue.push( (u32)i );

                gpuMesh.valid        = false;
                gpuMesh.vertexOffset = 0;
                gpuMesh.indexOffset  = 0;
                gpuMesh.vertexCount  = 0;

                gpuMesh.meshletCount         = 0;
                gpuMesh.meshletVertexCount   = 0;
                gpuMesh.meshletPrimitiveSize = 0;

                // Not resident anymore: the slot is skipped by draws until its next upload is placed
                gpuMesh.geometryPageID         = AXION_INVALID_U32;
                gpuMesh.vertexHeapID           = AXION_INVALID_U32;
                gpuMesh.indexHeapID            = AXION_INVALID_U32;
                gpuMesh.meshletHeapID          = AXION_INVALID_U32;
                gpuMesh.meshletVertexHeapID    = AXION_INVALID_U32;
                gpuMesh.meshletPrimitiveHeapID = AXION_INVALID_U32;

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

                // Check textures inside material
                for ( u32 textureGpuHandle : _materialToTextureMap[i] )
                {
                    _textureCache.cache[textureGpuHandle].refCount--;
                    // If no more materials owning this texture, prepare to destroy it
                    if ( _textureCache.cache[textureGpuHandle].refCount == 0 )
                    {
                        _pendingTextureReleases.push( textureGpuHandle );

                        auto& gpuTex = _textureCache.cache[textureGpuHandle];

                        if ( gpuTex.originalAssetID < _textureCache.assetToCacheLUT.size() )
                        {
                            _textureCache.assetToCacheLUT[gpuTex.originalAssetID] = -1;
                        }

                        _textureCache.freeIndexQueue.push( (u32)textureGpuHandle );

                        gpuTex.valid           = false;
                        gpuTex.refCount        = 0;
                        gpuTex.originalAssetID = 0;
                        gpuTex.heapID          = AXION_INVALID_U32;

                        AXION_LOG_INFO( Logger::Module::Core, "GC: Recycled texture slot {}", textureGpuHandle );
                    }
                }

                _materialToTextureMap[i].clear();

                if ( gpuMat.originalAssetID < _materialCache.assetToCacheLUT.size() )
                {
                    _materialCache.assetToCacheLUT[gpuMat.originalAssetID] = -1;
                }

                _materialCache.freeIndexQueue.push( (u32)i );

                gpuMat.valid        = false;
                gpuMat.bufferOffset = AXION_INVALID_U32;
                gpuMat.payloadSize  = 0;
                gpuMat.psoBundleID  = AXION_INVALID_U32;

                AXION_LOG_INFO( Logger::Module::Core, "GC: Recycled material slot {}", i );
            }
        }
    }

    for ( u64 i = 0; i < _particleBufferCache.cache.size(); ++i )
    {
        auto& buffer = _particleBufferCache.cache[i];
        if ( !buffer.valid || _currentFrameIndex - buffer.lastFrameUsed <= _resourceTTL )
            continue;

        _pendingParticleBufferReleases.push( (u32)i );

        // The entity may own a newer buffer already (its particle count changed)
        auto& lut = _particleBufferCache.assetToCacheLUT;
        if ( buffer.ownerEntity < lut.size() && lut[buffer.ownerEntity] == (int)i )
            lut[buffer.ownerEntity] = -1;

        _particleBufferCache.freeIndexQueue.push( i );
        buffer.valid = false;
    }

    for ( u64 i = 0; i < _csmCache.cache.size(); ++i )
    {
        auto& csm = _csmCache.cache[i];
        if ( !csm.valid || _currentFrameIndex - csm.lastFrameUsed <= _resourceTTL )
            continue;

        _pendingCSMReleases.push( (u32)i );

        // The entity may own a newer CSM already (its resolution changed)
        auto& lut = _csmCache.assetToCacheLUT;
        if ( csm.ownerEntity < lut.size() && lut[csm.ownerEntity] == (int)i )
            lut[csm.ownerEntity] = -1;

        _csmCache.freeIndexQueue.push( i );
        csm.valid     = false;
        csm.srvHeapID = AXION_INVALID_U32;
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