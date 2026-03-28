
#pragma once
#include <Axion/Common/Memory/Pointers/OwnerPtr.h>
#include <Axion/Core/Scene/Entity.h>
#include <Axion/Core/Scene/Scene.h>

AXION_NAMESPACE_BEGIN

namespace Core::Render {

DEFINE_OWNER_PTR_FOR_TYPE( IRenderer, Renderer )

struct CommonSettings {
    std::string             name             = "";
    Graphics::API           gfxApi           = Graphics::API::DirectX12;
    Graphics::BufferingType bufferingType    = Graphics::BufferingType::Double;
    Graphics::Format        backbufferFormat = Graphics::Format::RGBA8_UNORM;
    Graphics::GCMode        GCMode           = Graphics::GCMode::AvgMemory;
    uint                    selectedDeviceID = UINT32_MAX;

    uint maxMtlTextures = 8192;
    uint maxMtlSamplers = 128;

    RendererFlags flags = RendererEnableDebug;
    // #ifdef AXION_DEBUG
    //     RendererFlags flags = RendererEnableDebug;
    // #else
    //     RendererFlags flags = RendererNone;
    // #endif
};

struct MemoryBudget {
    ulong geometryBufferSize    = 512 * 1024 * 1024; ///< Initial memory reservation persistent static geometry buffer -Vertex/Index- (512MB default)
    ulong materialBufferSize    = 16 * 1024 * 1024;  ///< Initial memory reservation persistent static material buffer (16MB default)
    ulong volatileBufferSize    = 16 * 1024 * 1024;  ///< Initial memory reservation for per-frame volatile buffer -Enough for UBOs, Transforms, Lights, GUI, etc- (16MB default)
    ulong uploadBufferSize      = 128 * 1024 * 1024; ///< Initial memory reservation for per-frame transient upload buffer -for texture/accel/data streaming- (128MB default)
    ulong GPUCommandBuffersSize = 1024 * 1024;       ///< Initial memory reservation for per-frame Shader Binding Tables and Indirect Commands data (1MB default).
    ulong RGAllocSize           = 1024 * 1024;       ///< Initial memory reservation for per-frame RenderGraph data (1MB default).
    uint  RGDescriptorsPerFrame = 2048;              ///< Initial memory reservation for per-frame DescriptorSet data.
    uint  RGMaxViewsPerFrame    = 8192 + 256;        ///< Initial view count reservation for per-frame Descriptor Pools.
    uint  RGMaxSamplersPerFrame = 128 + 4;           ///< Initial sampler count reservation for per-frame Descriptor Pools.
};

class IRenderer
{
public:
    virtual ~IRenderer() = default;

    virtual void compileShaders( uint threadCount = 1 )                                                   = 0;
    virtual void render( const Scene::Scene& scene, Scene::Entity& cameraEntity, float deltaTime = 0.0f ) = 0;
    virtual void shutdown()                                                                               = 0;

    virtual void newGuiFrame() const = 0;

    virtual CommonSettings getCommonSettings() const = 0;
    virtual MemoryBudget   getMemoryBudget() const   = 0;

    virtual ulong      getCurrentFrameIndex() const   = 0;
    virtual ulong      getTotalFrameNumber() const    = 0;
    virtual const uint getTotalFramesInFlight() const = 0;

    virtual std::string toString() const = 0;
};

} // namespace Core::Render

AXION_NAMESPACE_END