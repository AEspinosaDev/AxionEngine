#pragma once
#include "Axion/Graphics/Subsystems/IRenderGraph.h"
#include <Render/GPUScene.h>
#include <Render/PassManager.h>
#include <Render/Rasterizer/RasterizerConfig.h>

AXION_NAMESPACE_BEGIN
namespace Core::Render {

/// @brief Simulates every particle system and culls its particles, in the same dispatch.
class UpdateParticleSystemPass : public IRenderPass
{
public:
    static constexpr u32 GROUP_SIZE = 256; ///< Must match PARTICLE_GROUP_SIZE in UpdateParticleSystem.slang

    struct Config {
        Graphics::RGResourceHandle               outVisibleCountsHandle;   ///< Visible particles per system
        STLW::Vector<Graphics::RGResourceHandle> outParticleBufferHandles; ///< Data + visible list of every system. Written through the bindless heap: invisible to the graph

        Graphics::BufferSlice            inResetCountsSlice;               ///< Zeros (CPU upload)
        Graphics::RHI::IDescriptorSet*   transientDescriptorSet = nullptr; ///< Frame set: gFrame, gMeshes, gParticleSystems
        const Vector<GPUParticleSystem>* systems                = nullptr;
        float                            deltaTime              = 0.0f;
    };

    void registerShaders( Graphics::IShaderRegistry& shaders ) override {
        _shHandle = shaders.shader( "UpdateParticleSystemShader" )
                        .asDXIL()
                        .path( AXION_SHADER_DIR "/Slang/Preprocess/UpdateParticleSystem.slang" )
                        .include( AXION_SHADER_DIR "/Slang/Common" )
                        .cs( "csUpdateParticleSystem" )
                        .autoReflect( false ) // Global layout
                        .load();
    }
    void createPipelines( Graphics::IPipelineRegistry& pipelines ) override {
        _pipHandle = pipelines.compute( "UpdateParticleSystemPipeline" ).shader( _shHandle ).setLayout( _globalLayout ).create();
    }

    void addToGraph( Graphics::RenderGraphBuilder& builder, const Config& seedData ) {

        builder.addPass<Config>( "UpdateParticleSystemPass", seedData,

                                 []( Graphics::RenderPassBuilder& pb, Config& data ) {
        data.outVisibleCountsHandle = pb.write( data.outVisibleCountsHandle, Graphics::RHI::ResourceState::UnorderedAccess );
        for ( auto& handle : data.outParticleBufferHandles )
            handle = pb.write( handle, Graphics::RHI::ResourceState::UnorderedAccess ); },
                                 [this]( const Config& data, Graphics::RenderPassContext& ctx ) { this->execute( data, ctx ); } );
    }

private:
    void execute( const Config& data, Graphics::RenderPassContext& ctx ) {

        auto* cmd    = ctx.cmd;
        auto* pso    = ctx.pipelines.getComputePipeline( _pipHandle );
        auto* layout = pso->getDescription().layout;

        // The visible counters start every frame at 0
        auto*       counts = ctx.getBuffer( data.outVisibleCountsHandle );
        const auto& zeros  = data.inResetCountsSlice;
        cmd->barrier( counts, Graphics::RHI::ResourceState::CopyDest );
        cmd->copyBuffer( counts, zeros.container, zeros.size, 0, zeros.offset, Graphics::RHI::BarrierPolicy::None );
        cmd->barrier( counts, Graphics::RHI::ResourceState::UnorderedAccess );

        cmd->bindComputePipeline( pso );

        // SPACE 0: Frame data (systems, meshes, frustum)
        cmd->bindDescriptorSet( (u32)Rasterizer::Config::LayoutSetType::FrameBound, data.transientDescriptorSet, layout, Graphics::RHI::PipelineBindPoint::Compute );

        // SPACE 1: Visible counters out
        auto* ioSet = ctx.allocateSet( layout, Graphics::PassIO::SET_INDEX );
        ioSet->attach( Graphics::PassIO::rwBuffer( 0 ), Graphics::RHI::DescriptorType::UAV_Buffer, counts );
        cmd->bindDescriptorSet( Graphics::PassIO::SET_INDEX, ioSet, layout, Graphics::RHI::PipelineBindPoint::Compute );

        struct UpdateConstants {
            u32   systemIndex;
            float deltaTime;
        };

        for ( u32 i = 0; i < (u32)data.systems->size(); ++i )
        {
            const GPUParticleSystem& system = ( *data.systems )[i];

            // Its buffers don't exist yet, or there is nothing to do this frame
            if ( system.bufferUAVHeapID == AXION_INVALID_U32 || ( !system.visible && !system.update && !system.reset ) )
                continue;

            cmd->pushConstants( UpdateConstants { i, data.deltaTime } );
            cmd->dispatch( { ( system.count + GROUP_SIZE - 1 ) / GROUP_SIZE, 1, 1 } );
        }
    }

    Graphics::PipelineHandle _pipHandle;
    Graphics::ShaderHandle   _shHandle;
};

} // namespace Core::Render
AXION_NAMESPACE_END
