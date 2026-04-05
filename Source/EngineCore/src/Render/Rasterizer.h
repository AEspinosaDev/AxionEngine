#pragma once
#include "Axion/Graphics/Passes/Utilitary.hpp"
#include "DrawIndirect.h"
#include "GPUScene.h"
#include "MaterialSystem.h"
#include "PassSystem.h"
#include <Axion/Common/Containers/STLWrapper/String.h>
#include <Axion/Core/Assets/Material.h>
#include <Axion/Core/Render/IRasterizer.h>
#include <Axion/Graphics/IRenderer.h>

// High Level Passes
#include "Passes/CullingPass.hpp"
#include "Passes/DepthPass.hpp"
#include "Passes/FXAAPass.hpp"
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

    void compileShaders( u32 threadCount = 1 ) override;
    void render( const Scene::Scene& scene, Scene::Entity& cameraEntity, float deltaTime = 0.0f ) override;
    void shutdown() override;

    void newGuiFrame() const override;

    Settings getSettings() const override { return _settings; };
    void     setSettings( const Settings& settings ) override { _settings = settings; };

    MemoryBudget   getMemoryBudget() const override { return _settings.memory; };
    CommonSettings getCommonSettings() const override { return _settings.common; };

    u64 getCurrentFrameIndex() const override;
    u64 getTotalFrameNumber() const override;

    const u32 getTotalFramesInFlight() const override { return _framesInFlight; };

    STLW::String toString() const override;

private:
    void setupMaterialLibrary();
    void registerMaterials();
    void registerPasses();
    void createResources();

    struct TransientPayload {
        Graphics::BufferSlice frameSlice;
        Graphics::BufferSlice meshesSlice;
        Graphics::BufferSlice mtlSlice;
        Graphics::BufferSlice instancesSlice;
        Graphics::BufferSlice lightsSlice;
        Graphics::BufferSlice envsSlice;
        Graphics::BufferSlice redirectSlice;
    };

    TransientPayload       uploadTransientData( Graphics::BufferLinearAllocator<>& currentUBOAlloc,
                                                Graphics::BufferLinearAllocator<>& currentSSBOAlloc );
    IndirectCommandPayload uploadIndirectCommandData( Graphics::BufferLinearAllocator<>& currentSSBOAlloc,
                                                      Graphics::BufferLinearAllocator<>& currentIndirectAlloc );

    Platform::Window*  _window = nullptr;
    RasterizerSettings _settings;

    // Passes
    PassManager _passes;
    // Backend Passes
    Axion::Graphics::Passes::BlitToBackBuffer _cpypass {};
    Axion::Graphics::Passes::GUIPass          _guipass {};
    Axion::Graphics::Passes::PresentPass      _presentpass {};

    // Material Library & Global Shader Contract
    MaterialLibrary                _mtlLib;
    Graphics::PipelineLayoutHandle _globalMtlLayoutHandle;

    // Graphics & GPU Resources Logic and Handles
    GPUScene _gpuScene;

    struct FrameResources {
        Graphics::BufferHandle            uboBufferHandle;
        Graphics::BufferLinearAllocator<> uboAllocator;

        Graphics::BufferHandle            ssboBufferHandle;
        Graphics::BufferLinearAllocator<> ssboAllocator;

        // Indirect Rendering
        Graphics::BufferHandle            indirectStagingBufferHandle;
        Graphics::BufferLinearAllocator<> indirectAllocator;
        Graphics::BufferHandle            indirectTemplateBufferHandle;
        Graphics::BufferHandle            indirectBufferHandle;
        Graphics::BufferHandle            culledInstanceBufferHandle;

        // Persistent Descriptor Set
        Graphics::RHI::IDescriptorSet* persistentDescriptorSetPtr = nullptr;
    };
    struct GPUResources {
        // Resource Handles
        Graphics::BufferHandle               vertexBufferHandle;
        Graphics::BufferGPUFreeListAllocator vertexAllocator;

        Graphics::BufferHandle               indexBufferHandle;
        Graphics::BufferGPUFreeListAllocator indexAllocator;

        Graphics::BufferHandle               mtlBufferHandle;
        Graphics::BufferGPUFreeListAllocator mtlAllocator;

        Vector<Graphics::TextureHandle> textureHandles;
        Graphics::TextureHandle         fallbackTexture2DHandle;
        Vector<Graphics::SamplerHandle> samplerHandles;
        Graphics::SamplerHandle         fallbackSamplerHandle;

        SmallVector<FrameResources, 3> frame;
    };
    GPUResources _res;

    Graphics::RendererOwnerPtr _rnd = nullptr;

    IndirectCommandPayload::Cache _indirectCommandDataCache;

    u32 _framesInFlight;
};

} // namespace Core::Render

AXION_NAMESPACE_END
