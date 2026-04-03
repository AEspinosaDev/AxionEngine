
#pragma once
#include <Axion/Common/Memory/Pointers/OwnerPtr.h>
#include <Axion/Core/Scene/Entity.h>
#include <Axion/Core/Scene/Scene.h>

AXION_NAMESPACE_BEGIN

namespace Core::Render {

DEFINE_OWNER_PTR_FOR_TYPE( IRenderer, Renderer )

struct CommonSettings {
    String64                name             = "Renderer";
    Graphics::API           gfxApi           = Graphics::API::DirectX12;
    Graphics::BufferingType bufferingType    = Graphics::BufferingType::Double;
    Graphics::Format        backbufferFormat = Graphics::Format::RGBA8_UNORM;
    Graphics::GCMode        GCMode           = Graphics::GCMode::AvgMemory;
    u32                     selectedDeviceID = UINT32_MAX;

    u32 maxMtlTextures = 8192;
    u32 maxMtlSamplers = 128;

    RendererFlags flags = RendererEnableDebug;
    // #ifdef AXION_DEBUG
    //     RendererFlags flags = RendererEnableDebug;
    // #else
    //     RendererFlags flags = RendererNone;
    // #endif
};

struct MemoryBudget {
    u64 geometryBufferSize    = 512 * 1024 * 1024; ///< Initial memory reservation persistent static geometry buffer -Vertex/Index- (512MB default)
    u64 materialBufferSize    = 16 * 1024 * 1024;  ///< Initial memory reservation persistent static material buffer (16MB default)
    u64 volatileBufferSize    = 16 * 1024 * 1024;  ///< Initial memory reservation for per-frame volatile buffer -Enough for UBOs, Transforms, Lights, GUI, etc- (16MB default)
    u64 uploadBufferSize      = 128 * 1024 * 1024; ///< Initial memory reservation for per-frame transient upload buffer -for texture/accel/data streaming- (128MB default)
    u64 GPUCommandBuffersSize = 1024 * 1024;       ///< Initial memory reservation for per-frame Shader Binding Tables and Indirect Commands data (1MB default).
    u64 RGAllocSize           = 1024 * 1024;       ///< Initial memory reservation for per-frame RenderGraph data (1MB default).
    u32 RGDescriptorsPerFrame = 2048;              ///< Initial memory reservation for per-frame DescriptorSet data.
    u32 RGMaxViewsPerFrame    = 8192 + 256;        ///< Initial view count reservation for per-frame Descriptor Pools.
    u32 RGMaxSamplersPerFrame = 128 + 4;           ///< Initial sampler count reservation for per-frame Descriptor Pools.
};

class IRenderer
{
public:
    virtual ~IRenderer() = default;

    virtual void compileShaders( u32 threadCount = 1 )                                                    = 0;
    virtual void render( const Scene::Scene& scene, Scene::Entity& cameraEntity, float deltaTime = 0.0f ) = 0;
    virtual void shutdown()                                                                               = 0;

    virtual void newGuiFrame() const = 0;

    virtual CommonSettings getCommonSettings() const = 0;
    virtual MemoryBudget   getMemoryBudget() const   = 0;

    virtual u64       getCurrentFrameIndex() const   = 0;
    virtual u64       getTotalFrameNumber() const    = 0;
    virtual const u32 getTotalFramesInFlight() const = 0;

    virtual STLW::String toString() const = 0;
};

} // namespace Core::Render

AXION_NAMESPACE_END