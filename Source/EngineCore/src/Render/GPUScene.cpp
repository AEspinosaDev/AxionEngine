#include "GPUScene.h"

AXION_NAMESPACE_BEGIN

namespace Core::Render {

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

GPUScene::TransientOffsets GPUScene::uploadTransientData( Graphics::RHI::IBuffer* destBuffer ) {
    TransientOffsets offsets;

    uint currentOffset = 0;
    // (D3D12/Vulkan)
    const uint CBV_ALIGNMENT = 256;

    // 1. FRAME DATA (Constant Buffer)
    // ---------------------------------------------------------
    // CBVs requieren alineación de 256 bytes al inicio. Como currentOffset es 0, OK.
    offsets.frameOffset = currentOffset;
    destBuffer->copyData( _frame, currentOffset );

    currentOffset += sizeof( GPUFrame );
    currentOffset = Helpers::alignu( currentOffset, CBV_ALIGNMENT );

    // 2. MESH METADATA (Structured Buffer)
    // ---------------------------------------------------------
    if ( sizeof( GPUMesh ) > 0 )
        currentOffset = Helpers::alignu( currentOffset, sizeof( GPUMesh ) );

    offsets.meshOffset = currentOffset;

    if ( !_meshCache.empty() )
    {
        destBuffer->copyData( _meshCache, currentOffset );
        currentOffset += _meshCache.size() * sizeof( GPUMesh );
        // Ya no alineamos a 256 al final, el siguiente bloque se encargará de su propia alineación
    }

    // 3. INSTANCE DATA (Structured Buffer)
    // ---------------------------------------------------------
    // CRÍTICO: Alinear al tamaño del struct
    if ( sizeof( GPUInstance ) > 0 )
        currentOffset = Helpers::alignu( currentOffset, sizeof( GPUInstance ) );

    offsets.instanceOffset = currentOffset;

    if ( !_instances.empty() )
    {
        destBuffer->copyData( _instances, currentOffset );
        currentOffset += _instances.size() * sizeof( GPUInstance );
    }

    // 4. LIGHT DATA (Structured Buffer)
    // ---------------------------------------------------------
    // CRÍTICO: Alinear al tamaño del struct
    if ( sizeof( GPULight ) > 0 )
        currentOffset = Helpers::alignu( currentOffset, sizeof( GPULight ) );

    offsets.lightOffset = currentOffset;

    if ( !_lights.empty() )
    {
        destBuffer->copyData( _lights, currentOffset );
        currentOffset += _lights.size() * sizeof( GPULight );
    }

    return offsets;
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

    auto meshView = cpuScene.getRegistry().multiView<const Scene::MeshComponent, const Scene::TransformComponent>();

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
    auto lightView = cpuScene.getRegistry().multiView<const Scene::LightComponent, const Scene::TransformComponent>();

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

#pragma region Frame
#pragma endregion

void GPUScene::processFrame( Scene::Entity& cameraEntity, const Extent2D& resolution, bool transpose ) {
    if ( cameraEntity && cameraEntity.hasComponent<Scene::CameraComponent>() &&
         cameraEntity.hasComponent<Scene::TransformComponent>() )
    {
        const auto& camComp   = cameraEntity.getComponent<Scene::CameraComponent>();
        const auto& transComp = cameraEntity.getComponent<Scene::TransformComponent>();

        auto proj  = camComp.getProjection( resolution );
        auto model = transComp.getMatrix();

        auto viewMat  = Math::MTX::inverse( model );
        // auto viewMat     = Axion::Math::MTX::lookAt( { 0, 0, -2.0 }, { 0, 0, 0 }, { 0, 1, 0 } );
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
            0.0f,
            0.0f );

        // _frame.sceneAABBMin = Math::Vec4( _sceneAABB.min.x, _sceneAABB.min.y, _sceneAABB.min.z, 0.0f );
        // _frame.sceneAABBMax = Math::Vec4( _sceneAABB.max.x, _sceneAABB.max.y, _sceneAABB.max.z, 0.0f );
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

bool GPUScene::hasPendingUploads() const {
    return !_pendingMeshUploads.empty();
}

bool GPUScene::hasPendingReleases() const {
    return !_pendingMeshReleases.empty();
}

} // namespace Core::Render
AXION_NAMESPACE_END