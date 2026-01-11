#pragma once
#include "Axion/Graphics/Passes/Utilitary.hpp"
#include "GPUScene.h"
#include "MaterialSystem.h"
#include "PassSystem.h"
#include <Axion/Core/Render/Rasterizer.h>
#include <Axion/Graphics/Renderer.h>
// High Level Passes
#include "Passes/TonemappingPass.hpp"

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
    void registerMaterials();
    void registerPasses();
    void createResources();

    Platform::Window*  _window = nullptr;
    RasterizerSettings _settings;

    // Passes
    PassManager _passes;

    // Material Library
    MaterialLibrary _matLib;

    // Graphics & GPU Resources Logic and Handles
    GPUScene _gpuScene;

    struct GPUResources {
        Graphics::BufferHandle           vertexBufferHandle;
        Graphics::BufferHandle           indexBufferHandle;
        Graphics::RHI::FreeListAllocator vertexAllocator;
        Graphics::RHI::FreeListAllocator indexAllocator;
        Graphics::BufferHandle           matBufferHandle;
        Graphics::RHI::FreeListAllocator matAllocator;

        std::vector<Graphics::BufferHandle>         uboBufferHandles;    // Per-frame
        std::vector<Graphics::RHI::LinearAllocator> uboBufferAllocators; // Per-frame

        // Graphics::AccelHandle staticTLASHandle;
        // Graphics::AccelHandle dynamicTLASHandle;
    };
    GPUResources _res;

    Graphics::RendererPtr _rnd = nullptr;

    uint _framesInFlight;
};

} // namespace Core::Render

AXION_NAMESPACE_END
