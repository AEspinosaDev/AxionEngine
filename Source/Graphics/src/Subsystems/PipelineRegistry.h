#pragma once
#include "RendererSubsystem.h"
#include <variant>

AXION_NAMESPACE_BEGIN

namespace Graphics {

class PipelineRegistry final : public IPipelineRegistry, public RendererSubsystem
{
public:
    explicit PipelineRegistry();
    ~PipelineRegistry() override;

    void initialize( const SubsystemInitContext& ctx ) override;

    GraphicBuilder    graphic( StringView name ) override { return GraphicBuilder( *this, name ); }
    ComputeBuilder    compute( StringView name ) override { return ComputeBuilder( *this, name ); }
    RayTracingBuilder raytracing( StringView name ) override { return RayTracingBuilder( *this, name ); }
    MeshBuilder       mesh( StringView name ) override { return MeshBuilder( *this, name ); }
    LayoutBuilder     layout( StringView name ) override { return LayoutBuilder( *this, name ); }

    RHI::IGraphicPipeline*    getGraphicPipeline( PipelineHandle handle ) override;
    RHI::IComputePipeline*    getComputePipeline( PipelineHandle handle ) override;
    RHI::IRayTracingPipeline* getRaytracingPipeline( PipelineHandle handle ) override;
    RHI::IMeshPipeline*       getMeshPipeline( PipelineHandle handle ) override;
    RHI::IPipelineLayout*     getLayout( PipelineLayoutHandle handle ) override;

    std::optional<PipelineHandle>       findPipeline( StringView name ) const override;
    std::optional<PipelineLayoutHandle> findLayout( StringView name ) const override;

    void destroyLayout( PipelineLayoutHandle handle ) override;
    void destroyPipeline( PipelineHandle handle ) override;

    u32 size() const override { return (u32)_pipelines.size(); };

    // (Hot-Reloading)
    void reloadAll() override;

private:
    PipelineHandle createGraphic( RHI::GraphicPipelineDesc& desc, ShaderHandle shaderHandle ) override;
    PipelineHandle createCompute( RHI::ComputePipelineDesc& desc, ShaderHandle shaderHandle ) override;
    PipelineHandle createRaytracing( RHI::RayTracingPipelineDesc& desc, ShaderHandle shaderHandle ) override;
    PipelineHandle createMesh( RHI::MeshPipelineDesc& desc, ShaderHandle shaderHandle ) override;
    PipelineHandle createGraphic( RHI::GraphicPipelineDesc& desc, StringView shaderName ) override;
    PipelineHandle createCompute( RHI::ComputePipelineDesc& desc, StringView shaderName ) override;
    PipelineHandle createRaytracing( RHI::RayTracingPipelineDesc& desc, StringView shaderName ) override;
    PipelineHandle createMesh( RHI::MeshPipelineDesc& desc, StringView shaderName ) override;

    PipelineLayoutHandle createLayout( const RHI::PipelineLayoutDesc& desc ) override;

    IShaderRegistry* _shaderReg = nullptr;

    struct PipelineRecord {
        String64 name;
        bool     alive = false;
        std::variant<std::monostate,
                     RHI::GraphicPipelineOwnerPtr,
                     RHI::ComputePipelineOwnerPtr,
                     RHI::RayTracingPipelineOwnerPtr,
                     RHI::MeshPipelineOwnerPtr>
                                    pipeline;
        RHI::PipelineLayoutOwnerPtr layoutOwner = nullptr;
    };
    struct LayoutRecord {
        String64                    name;
        bool                        alive = true;
        RHI::PipelineLayoutOwnerPtr layout;
    };

    STLW::Vector<PipelineRecord>                 _pipelines;
    STLW::UnorderedMap<String64, PipelineHandle> _nameToHandle;

    STLW::Vector<LayoutRecord>                         _layouts;
    STLW::UnorderedMap<String64, PipelineLayoutHandle> _nameToLayoutHandle;
};

AXION_NAMESPACE_END
} // namespace Graphics