#pragma once
#include "Axion/Graphics/Subsystems/IRenderGraph.h"
#include <Render/PassManager.h>
#include <Render/Rasterizer/RasterizerConfig.h>

AXION_NAMESPACE_BEGIN
namespace Core::Render {

/// @brief Writes the mesh indirect command of every particle system from its visible count.
class WriteParticleCommandsPass : public IRenderPass
{
public:
    static constexpr u32 GROUP_SIZE = 64; ///< Must match WRITE_COMMANDS_GROUP_SIZE in WriteParticleCommands.slang

    struct Config {
        Graphics::RGResourceHandle inVisibleCountsHandle;   ///< Visible particles per system
        Graphics::RGResourceHandle outIndirectBufferHandle; ///< One command per system, after the instance ones

        Graphics::RHI::IDescriptorSet* transientDescriptorSet = nullptr; ///< Frame set: gMeshes, gParticleSystems
        u32                            systemCount            = 0;
        u32                            firstCommand           = 0; ///< Instance commands in the buffer
    };

    void registerShaders( Graphics::IShaderRegistry& shaders ) override {
        _shHandle = shaders.shader( "WriteParticleCommandsShader" )
                        .asDXIL()
                        .path( AXION_SHADER_DIR "/Slang/Preprocess/WriteParticleCommands.slang" )
                        .include( AXION_SHADER_DIR "/Slang/Common" )
                        .cs( "csWriteParticleCommands" )
                        .autoReflect( false ) // Global layout
                        .load();
    }
    void createPipelines( Graphics::IPipelineRegistry& pipelines ) override {
        _pipHandle = pipelines.compute( "WriteParticleCommandsPipeline" ).shader( _shHandle ).setLayout( _globalLayout ).create();
    }

    void addToGraph( Graphics::RenderGraphBuilder& builder, const Config& seedData ) {

        builder.addPass<Config>( "WriteParticleCommandsPass", seedData,

                                 []( Graphics::RenderPassBuilder& pb, Config& data ) {
        // Reading as SRV waits for the update: a dispatch in the same UAV state wouldn't (the RHI has no UAV barriers)
        data.inVisibleCountsHandle   = pb.read( data.inVisibleCountsHandle, Graphics::RHI::ResourceState::ShaderResource );
        data.outIndirectBufferHandle = pb.write( data.outIndirectBufferHandle, Graphics::RHI::ResourceState::UnorderedAccess ); },
                                 [this]( const Config& data, Graphics::RenderPassContext& ctx ) { this->execute( data, ctx ); } );
    }

private:
    void execute( const Config& data, Graphics::RenderPassContext& ctx ) {

        auto* cmd    = ctx.cmd;
        auto* pso    = ctx.pipelines.getComputePipeline( _pipHandle );
        auto* layout = pso->getDescription().layout;

        cmd->bindComputePipeline( pso );

        // SPACE 0: Frame data (systems, meshes)
        cmd->bindDescriptorSet( (u32)Rasterizer::Config::LayoutSetType::FrameBound, data.transientDescriptorSet, layout, Graphics::RHI::PipelineBindPoint::Compute );

        // SPACE 1: Visible counters in, commands out
        auto* ioSet = ctx.allocateSet( layout, Graphics::PassIO::SET_INDEX );
        ioSet->attach( Graphics::PassIO::buffer( 0 ), Graphics::RHI::DescriptorType::SRV_Buffer, ctx.getBuffer( data.inVisibleCountsHandle ) );
        ioSet->attach( Graphics::PassIO::rwBuffer( 0 ), Graphics::RHI::DescriptorType::UAV_Buffer, ctx.getBuffer( data.outIndirectBufferHandle ) );
        cmd->bindDescriptorSet( Graphics::PassIO::SET_INDEX, ioSet, layout, Graphics::RHI::PipelineBindPoint::Compute );

        struct WriteCommandsConstants {
            u32 systemCount;
            u32 firstCommand;
        };

        cmd->pushConstants( WriteCommandsConstants { data.systemCount, data.firstCommand } );
        cmd->dispatch( { ( data.systemCount + GROUP_SIZE - 1 ) / GROUP_SIZE, 1, 1 } );
    }

    Graphics::PipelineHandle _pipHandle;
    Graphics::ShaderHandle   _shHandle;
};

} // namespace Core::Render
AXION_NAMESPACE_END
