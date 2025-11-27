#pragma once
#include "Axion/Graphics/RHI/Device.h"
#include "Axion/Graphics/Subsystems/PipelineRegistry.h"
#include <variant>

AXION_NAMESPACE_BEGIN

namespace Graphics {

DEFINE_UNIQUE_PTR_FOR_TYPE( PipelineRegistry, PipelineRegistry )

class PipelineRegistry final : public IPipelineRegistry
{
public:
    explicit PipelineRegistry( RHI::IDevice* device, IShaderRegistry& shaderReg );
    ~PipelineRegistry() override;

    GraphicBuilder graphic( const std::string& name ) override { return GraphicBuilder( *this, name ); }
    ComputeBuilder compute( const std::string& name ) override { return ComputeBuilder( *this, name ); }

    RHI::IGraphicPipeline*        getGraphicPipeline( PipelineHandle handle ) override;
    RHI::IComputePipeline*        getComputePipeline( PipelineHandle handle ) override;
    std::optional<PipelineHandle> findPipeline( const std::string& name ) const override;
    void                          destroyPipeline( PipelineHandle handle ) override;
    uint                          size() const override { return (uint)_pipelines.size(); };

    // (Hot-Reloading)
    void reloadAll() override;

private:
    PipelineHandle createGraphic( RHI::GraphicPipelineDesc& desc, const std::string& shaderName ) override;
    PipelineHandle createCompute( RHI::ComputePipelineDesc& desc, const std::string& shaderName ) override;

    RHI::IDevice*    _device = nullptr;
    IShaderRegistry& _shaderReg;
    std::mutex       _mutex;

    struct PipelineRecord {
        std::string                                                                    name;
        bool                                                                           alive = false;
        std::variant<std::monostate, RHI::GraphicPipelinePtr, RHI::ComputePipelinePtr> pipeline;
        RHI::PipelineLayoutPtr                                                         layoutOwner = nullptr;
    };

    std::vector<PipelineRecord>                     _pipelines;
    std::unordered_map<std::string, PipelineHandle> _nameToHandle;
};

AXION_NAMESPACE_END
} // namespace Graphics