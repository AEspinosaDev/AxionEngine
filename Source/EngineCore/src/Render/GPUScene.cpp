#include "GPUScene.h"

AXION_NAMESPACE_BEGIN

namespace Core::Render {

// Config for GC
static const uint MAX_UNUSED_FRAMES_TTL = 120;

void GPUScene::update( const Scene::Scene& cpuScene,
                       Scene::Entity&      cameraEntity,
                       const Extent2D&     resolution,
                       float               deltaTime,
                       GPUSceneUpdateFlags flags ) {

    bool transposeMatrices = ( flags & GPUSceneTransposeMatrices ) != GPUSceneNone;
    bool forceRaytrace     = ( flags & GPUSceneForceRaytrace ) != GPUSceneNone;

    reset( deltaTime );

    processMeshes( cpuScene, transposeMatrices, forceRaytrace );
    processLights( cpuScene );
    processFrame( cameraEntity, resolution, transposeMatrices );

    runGC();
}

void GPUScene::reset( float dt ) {
    _currentFrameIndex++;
    _accumulatedTime += dt;
    _instances.clear();
    _lights.clear();
}

#pragma region Meshes
#pragma endregion

void GPUScene::processMeshes( const Scene::Scene& cpuScene, bool transpose, bool forceRaytrace ) {

    _instances.reserve( cpuScene.getRegistry().view<Scene::MeshComponent>().size() );
    _lights.reserve( cpuScene.getRegistry().view<Scene::LightComponent>().size() );

    auto* assets = cpuScene.assets();

    // Ensure LUT is big enough for all current assets.
    if ( _assetToCacheLUT.size() < assets->getMeshCount() )
        _assetToCacheLUT.resize( assets->getMeshCount(), -1 );

    auto meshView = cpuScene.getRegistry().view<Scene::MeshComponent, Scene::TransformComponent>();

    for ( ECS::EntityID entity : meshView )
    {
        const auto& meshComp  = meshView.get<Scene::MeshComponent>( entity );
        const auto& transComp = meshView.get<Scene::TransformComponent>( entity );

        if ( !meshComp.visible )
            continue;

        uint cpuAssetID    = meshComp.mesh.id;
        uint gpuCacheIndex = 0;

        // --- CACHE LOOKUP (O(1) Array Access) ---

        // Safety check: if asset ID is larger than current LUT size (rare race condition), grow
        if ( cpuAssetID >= _assetToCacheLUT.size() )
            _assetToCacheLUT.resize( cpuAssetID + 1, -1 );

        int cachedIndex = _assetToCacheLUT[cpuAssetID];

        if ( cachedIndex != -1 )
        {
            // A. ALREADY IN CACHE
            gpuCacheIndex = (uint)cachedIndex;
        } else
        {
            // B. NEW ALLOCATION NEEDED
            if ( !_freeIndexQueue.empty() )
            {
                // Recycle slot
                gpuCacheIndex = _freeIndexQueue.front();
                _freeIndexQueue.pop();
            } else
            {
                // Grow vector
                gpuCacheIndex = (uint)_meshCache.size();
                _meshCache.emplace_back();
            }

            // Register in LUT immediately
            _assetToCacheLUT[cpuAssetID] = (int)gpuCacheIndex;

            _meshCache[gpuCacheIndex].valid           = false;
            _meshCache[gpuCacheIndex].originalAssetID = cpuAssetID;
        }

        // --- UPLOAD LOGIC ---
        auto& gpuMesh = _meshCache[gpuCacheIndex];

        if ( !gpuMesh.valid )
        {
            auto* cpuMesh = assets->getMesh( meshComp.mesh );
            if ( cpuMesh )
            {
                gpuMesh.vertexCount = cpuMesh->getVertexCount();
                gpuMesh.indexCount  = cpuMesh->getIndexCount();
                gpuMesh.aabb        = cpuMesh->getAABB();
                gpuMesh.bsphere     = cpuMesh->getBoundingSphere();
                gpuMesh.needsAS     = true;

                _pendingMeshUploads.push( { gpuCacheIndex, // Slot Index
                                            cpuMesh->getVertices(),
                                            cpuMesh->getIndices() } );

                gpuMesh.valid = true;
            }
        }

        if ( gpuMesh.valid )
            gpuMesh.lastFrameUsed = _currentFrameIndex;

        // --- INSTANCE DATA ---
        GPUInstance instance;
        instance.modelMatrix = transComp.getMatrix();

        if ( transpose )
            instance.modelMatrix = Math::MTX::transpose( instance.modelMatrix );

        instance.meshID     = gpuCacheIndex; // SLOT index
        instance.materialID = 0;             // For now
        instance.active     = 1;
        instance.raytraced  = meshComp.raytraced || forceRaytrace ? 1 : 0;

        _instances.push_back( instance );
    }
}

#pragma region Lights
#pragma endregion

void GPUScene::processLights( const Scene::Scene& cpuScene ) {
    auto lightView = cpuScene.getRegistry().view<Scene::LightComponent, Scene::TransformComponent>();

    for ( ECS::EntityID entity : lightView )
    {

        const auto& lightComp = lightView.get<Scene::LightComponent>( entity );
        const auto& transComp = lightView.get<Scene::TransformComponent>( entity );

        GPULight light;
        light.position = transComp.translation;
        // light.color     = lightComp.color;
        // light.intensity = lightComp.intensity;
        // light.radius    = lightComp.radius;
        light.active = 1;

        _lights.push_back( light );
    }
}

void GPUScene::processFrame( Scene::Entity& cameraEntity, const Extent2D& resolution, bool transpose ) {
    if ( cameraEntity && cameraEntity.hasComponent<Scene::CameraComponent>() &&
         cameraEntity.hasComponent<Scene::TransformComponent>() )
    {
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

        _frame.cameraPosition = transComp.translation;
        _frame.time           = _accumulatedTime;
        _frame.resolution     = resolution;
        _frame.lightCount     = (uint)_lights.size();
        _frame.instanceCount  = (uint)_instances.size();
    }
}

#pragma region GC
#pragma endregion

void GPUScene::runGC() {
    for ( ulong i = 0; i < _meshCache.size(); ++i )
    {
        auto& gpuMesh = _meshCache[i];

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

                if ( gpuMesh.originalAssetID < _assetToCacheLUT.size() )
                {
                    _assetToCacheLUT[gpuMesh.originalAssetID] = -1;
                }

                _freeIndexQueue.push( (uint)i );

                gpuMesh.valid        = false;
                gpuMesh.vertexOffset = 0;
                gpuMesh.indexOffset  = 0;
                gpuMesh.vertexCount  = 0;

                AXION_LOG_INFO( Logger::Module::Core, "GC: Recycled mesh slot {}", i );
            }
        }
    }
}

} // namespace Core::Render
AXION_NAMESPACE_END