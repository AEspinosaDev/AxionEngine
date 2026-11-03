#pragma once
#include <Render/DrawIndirect.h>
#include <Render/PassManager.h>
#include <Render/Rasterizer/RasterizerConfig.h>
#include "Axion/Graphics/Subsystems/IRenderGraph.h"

AXION_NAMESPACE_BEGIN
namespace Core::Render {

class CullingPass : public IRenderPass
{
public:
    static constexpr u32 GROUP_SIZE = 64; ///< Must match CULLING_GROUP_SIZE in Culling.slang

    struct Config {
        Graphics::RGResourceHandle outIndirectBufferHandle;       ///< DispatchMesh commands (instance counts filled here)
        Graphics::RGResourceHandle outCulledRedirectBufferHandle; ///< Visible instances, packed per command

        Graphics::BufferSlice          inCommandMapSlice;                ///< Sorted instance -> command (CPU upload)
        Graphics::RHI::IDescriptorSet* transientDescriptorSet = nullptr; ///< Frame set: gFrame, gMeshes, gInstances, sorted gInstanceRedirection
        u32                            instanceCount          = 0;       ///< Sorted (drawable) instances
    };

    void registerShaders( Graphics::IShaderRegistry& shaders ) override {
        _shHandle = shaders.shader( "CullingShader" )
                        .asDXIL()
                        .path( AXION_SHADER_DIR "/Slang/Preprocess/Culling.slang" )
                        .include( AXION_SHADER_DIR "/Slang/Common" )
                        .cs( "csCulling" )
                        .autoReflect( false ) // Global layout
                        .load();
    }
    void createPipelines( Graphics::IPipelineRegistry& pipelines ) override {
        _pipHandle = pipelines.compute( "CullingPipeline" ).shader( _shHandle ).setLayout( _globalLayout ).create();
    }

    void addToGraph( Graphics::RenderGraphBuilder& builder, const Config& seedData ) {

        builder.addPass<Config>( "CullingPass", seedData,

                                 []( Graphics::RenderPassBuilder& pb, Config& data ) {
        data.outIndirectBufferHandle       = pb.write( data.outIndirectBufferHandle, Graphics::RHI::ResourceState::UnorderedAccess );
        data.outCulledRedirectBufferHandle = pb.write( data.outCulledRedirectBufferHandle, Graphics::RHI::ResourceState::UnorderedAccess ); },
                                 [this]( const Config& data, Graphics::RenderPassContext& ctx ) { this->execute( data, ctx ); } );
    }

private:
    void execute( const Config& data, Graphics::RenderPassContext& ctx ) {

        auto* cmd    = ctx.cmd;
        auto* pso    = ctx.pipelines.getComputePipeline( _pipHandle );
        auto* layout = pso->getDescription().layout;

        cmd->bindComputePipeline( pso );

        // SPACE 0: Frame data (sorted instances, meshes, frustum)
        cmd->bindDescriptorSet( (u32)Rasterizer::Config::LayoutSetType::FrameBound, data.transientDescriptorSet, layout, Graphics::RHI::PipelineBindPoint::Compute );

        // SPACE 1: Command map in, commands + visible instances out
        auto* ioSet = ctx.allocateSet( layout, Graphics::PassIO::SET_INDEX );
        ioSet->attachBufferSlice( Graphics::PassIO::buffer( 0 ), Graphics::RHI::DescriptorType::SRV_Buffer, data.inCommandMapSlice );
        ioSet->attach( Graphics::PassIO::rwBuffer( 0 ), Graphics::RHI::DescriptorType::UAV_Buffer, ctx.getBuffer( data.outIndirectBufferHandle ) );
        ioSet->attach( Graphics::PassIO::rwBuffer( 1 ), Graphics::RHI::DescriptorType::UAV_Buffer, ctx.getBuffer( data.outCulledRedirectBufferHandle ) );
        cmd->bindDescriptorSet( Graphics::PassIO::SET_INDEX, ioSet, layout, Graphics::RHI::PipelineBindPoint::Compute );

        cmd->pushConstants( data.instanceCount );
        cmd->dispatch( { ( data.instanceCount + GROUP_SIZE - 1 ) / GROUP_SIZE, 1, 1 } );
    }

    Graphics::PipelineHandle _pipHandle;
    Graphics::ShaderHandle   _shHandle;
};

} // namespace Core::Render
AXION_NAMESPACE_END
