
#pragma once
#include <Axion/Graphics/RHI/IDevice.h>
#include <Axion/Graphics/Subsystems/IGPUResourcePool.h>
#include <Axion/Graphics/Subsystems/IPipelineRegistry.h>
#include <Axion/Graphics/Subsystems/IShaderRegistry.h>

AXION_NAMESPACE_BEGIN

namespace Graphics {

struct SubsystemInitContext {
    RHI::IDevice*      device    = nullptr;
    IGPUResourcePool*  pool      = nullptr;
    IPipelineRegistry* pipelines = nullptr;
    IShaderRegistry*   shaderReg = nullptr;
    //Add slab allocator here
};

class RendererSubsystem
{
public:
    virtual void initialize( const SubsystemInitContext& ctx ) {
        _device = ctx.device;
    }

    RHI::IDevice* getDevice() const {
        AXION_LOG_ASSERT( _device != nullptr, Logger::Module::GFX, "Null Device in RendererSubsystem, did you forget to call initialize?" );
        return _device;
    }

protected:
    RendererSubsystem()
        : _device( nullptr ) {}
    virtual ~RendererSubsystem() = default;

    RHI::IDevice*      _device = nullptr;
    mutable std::mutex _mutex;
};

} // namespace Graphics
AXION_NAMESPACE_END