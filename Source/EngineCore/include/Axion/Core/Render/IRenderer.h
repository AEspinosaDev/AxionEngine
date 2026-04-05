
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

    RendererFlags flags = RendererEnableDebug | RendererEnableGPUCulling;
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