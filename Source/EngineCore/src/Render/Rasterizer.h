#pragma once
#include "Axion/Graphics/Passes/Utilitary.hpp"
#include "Axion/Graphics/RHI/Memory.hpp"
#include "DrawIndirect.h"
#include "GPUScene.h"
#include "MaterialSystem.h"
#include "PassSystem.h"
#include <Axion/Core/Assets/Material.h>
#include <Axion/Core/Render/Rasterizer.h>
#include <Axion/Graphics/Renderer.h>

// High Level Passes
#include "Passes/CullingPass.hpp"
#include "Passes/DepthPass.hpp"
#include "Passes/ForwardPass.hpp"
#include "Passes/IndirectUploadPass.hpp"
#include "Passes/TonemappingPass.hpp"
#include "Passes/UploadPass.hpp"

AXION_NAMESPACE_BEGIN

namespace Core::Render {

class Rasterizer final : public IRasterizer
{
public:
    Rasterizer( Platform::Window* wnd, const RasterizerSettings& settings );
    ~Rasterizer();

    void compileShaders( uint threadCount = 1 ) override;
    void render( const Scene::Scene& scene, Scene::Entity& cameraEntity, float deltaTime = 0.0f ) override;
    void shutdown() override;

    Settings getSettings() const override { return _settings; };
    void     setSettings( const Settings& settings ) override { _settings = settings; };

    MemoryBudget   getMemoryBudget() const override { return _settings.memory; };
    CommonSettings getCommonSettings() const override { return _settings.common; };

    ulong getCurrentFrameIndex() const override;
    ulong getTotalFrameNumber() const override;

    const uint getTotalFramesInFlight() const override { return _framesInFlight; };

    std::string toString() const override;

private:
    void setupMaterialLibrary();
    void registerMaterials();
    void registerPasses();
    void createResources();

    struct TransientViews {
        Graphics::RHI::BufferView frameView;
        Graphics::RHI::BufferView meshesView;
        Graphics::RHI::BufferView mtlView;
        Graphics::RHI::BufferView instancesView;
        Graphics::RHI::BufferView lightsView;
        Graphics::RHI::BufferView envsView;
        Graphics::RHI::BufferView redirectView;
    };

    TransientViews      uploadTransientData( Graphics::RHI::LinearAllocator& currentUBOAlloc,
                                             Graphics::RHI::LinearAllocator& currentSSBOAlloc );
    IndirectCommandData uploadIndirectCommandData( Graphics::RHI::LinearAllocator& currentSSBOAlloc,
                                                   Graphics::RHI::LinearAllocator& currentIndirectAlloc );

    Platform::Window*  _window = nullptr;
    RasterizerSettings _settings;

    // Passes
    PassManager                               _passes;
    Axion::Graphics::Passes::BlitToBackBuffer cpypass {};

    // Material Library & Global Shader Contract
    MaterialLibrary                _mtlLib;
    Graphics::PipelineLayoutHandle _globalMtlLayoutHandle;

    // Graphics & GPU Resources Logic and Handles
    GPUScene _gpuScene;

    struct FrameResources {
        Graphics::BufferHandle         uboBufferHandle;
        Graphics::RHI::LinearAllocator uboAllocator;

        Graphics::BufferHandle         ssboBufferHandle;
        Graphics::RHI::LinearAllocator ssboAllocator;

        // Indirect Rendering
        Graphics::BufferHandle         indirectStagingBufferHandle;
        Graphics::RHI::LinearAllocator indirectAllocator;
        Graphics::BufferHandle         indirectTemplateBufferHandle;
        Graphics::BufferHandle         indirectBufferHandle;
        Graphics::BufferHandle         culledInstanceBufferHandle;

        // Persistent Descriptor Set
        Graphics::RHI::IDescriptorSet* persistentDescriptorSetPtr = nullptr;
    };
    struct GPUResources {
        // Resource Handles
        Graphics::BufferHandle           vertexBufferHandle;
        Graphics::RHI::FreeListAllocator vertexAllocator;

        Graphics::BufferHandle           indexBufferHandle;
        Graphics::RHI::FreeListAllocator indexAllocator;

        Graphics::BufferHandle           mtlBufferHandle;
        Graphics::RHI::FreeListAllocator mtlAllocator;

        std::vector<Graphics::TextureHandle> texture2DHandles;
        Graphics::TextureHandle              fallbackTexture2DHandle;
        std::vector<Graphics::SamplerHandle> samplerHandles;
        Graphics::SamplerHandle              fallbackSamplerHandle;

        std::vector<FrameResources> frame;
    };
    GPUResources _res;

    Graphics::RendererPtr _rnd = nullptr;

    IndirectCommandData::Cache _indirectCommandDataCache;

    uint _framesInFlight;
};

} // namespace Core::Render

AXION_NAMESPACE_END
