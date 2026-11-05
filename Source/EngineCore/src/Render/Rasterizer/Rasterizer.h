#pragma once
#include <Axion/Common/Containers/STLWrapper/String.h>
#include <Axion/Common/Memory/VMemoryArena.h>
#include <Axion/Core/Assets/Material.h>
#include <Axion/Core/Render/IRasterizer.h>
#include <Axion/Graphics/IRenderer.h>
#include <Axion/Graphics/Passes/Utilitary.hpp>

// Private
// -------------------
#include <Render/DrawIndirect.h>
#include <Render/Rasterizer/RasterizerConfig.h>
#include <Render/Rasterizer/RasterizerResources.h>

// Modules
#include <Render/GPUScene.h>
#include <Render/MaterialLibrary.h>
#include <Render/PassManager.h>

// Generic Passes
#include <Render/GenericPasses/FXAAPass.hpp>
#include <Render/GenericPasses/IndirectUploadPass.hpp>
#include <Render/GenericPasses/ToneMappingPass.hpp>
// Rasterizer Passes
#include <Render/Rasterizer/Passes/CullingPass.hpp>
#include <Render/Rasterizer/Passes/UploadPass.hpp>
#include <Render/Rasterizer/Passes/VisPass.hpp>
#include <Render/Rasterizer/Passes/VisResolveNaivePass.hpp>

AXION_NAMESPACE_BEGIN

namespace Core::Render::Rasterizer {

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

    const STLW::Vector<Graphics::GPUPassTiming>& getGPUPassTimings() const override { return _rnd->getPassTimings(); }

    const u32 getTotalFramesInFlight() const override { return _FRAMES_IN_FLIGHT; };

    STLW::String toString() const override;

private:
    void createResources();

    struct TransientPayload {
        Graphics::BufferSlice frameSlice;
        Graphics::BufferSlice meshesSlice;
        Graphics::BufferSlice mtlSlice;
        Graphics::BufferSlice instancesSlice;
        Graphics::BufferSlice lightsSlice;
        Graphics::BufferSlice envsSlice;
        Graphics::BufferSlice redirectSlice;
        Graphics::BufferSlice texturesSlice;
    };

    TransientPayload       uploadTransientData( Graphics::BufferLinearAllocator<>& currentUBOAlloc,
                                                Graphics::BufferLinearAllocator<>& currentSSBOAlloc );
    IndirectCommandPayload uploadIndirectCommandData( Graphics::BufferLinearAllocator<>& currentSSBOAlloc,
                                                      Graphics::BufferLinearAllocator<>& currentIndirectAlloc );

    Platform::Window*  _window = nullptr;
    RasterizerSettings _settings;

    // CPU Memory Management
    // Memory::VMemoryArena _memoryArena;

    // Passes
    PassManager _passes;
    // Backend Passes
    Graphics::Passes::BlitToBackBuffer _cpypass {};
    Graphics::Passes::GUIPass          _guipass {};
    Graphics::Passes::PresentPass      _presentpass {};

    // Material Library & Global Shader Contract
    MaterialLibrary<(u32)Config::MaterialPassType::Count> _mtlLib;
    Graphics::PipelineLayoutHandle                        _globalMtlLayoutHandle;

    // Graphics & GPU Resources Logic and Handles
    GPUScene                _gpuScene;
    Resources::GPUResources _res;

    Graphics::RendererOwnerPtr _rnd = nullptr;

    const u32 _FRAMES_IN_FLIGHT;
};

} // namespace Core::Render::Rasterizer

AXION_NAMESPACE_END
