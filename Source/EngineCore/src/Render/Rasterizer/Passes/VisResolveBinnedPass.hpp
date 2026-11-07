#pragma once
#include "Axion/Graphics/Subsystems/IRenderGraph.h"
#include <Render/PassManager.h>
#include <Render/Rasterizer/RasterizerConfig.h>
#include <Render/Rasterizer/RasterizerResources.h>

AXION_NAMESPACE_BEGIN
namespace Core::Render {

/// @brief Material resolve over the binned pixel list: one indirect dispatch per bin, with the resolve PSO of the bin.
/// The args written by BinPrefixPass carry the bin ID into the push constant root slot.
class VisResolveBinnedPass : public IRenderPass
{
public:
    struct Config {
        Rasterizer::Resources::GlobalBufferHandles inGlobalBufferHandles;   ///< Geometry pages + materials (bindless)
        STLW::Vector<Graphics::RGResourceHandle>   inParticleBufferHandles; ///< Read through the bindless heap: declared for the transitions

        Graphics::RGResourceHandle inVisHandle;
        Graphics::RGResourceHandle inPixelListHandle;
        Graphics::RGResourceHandle inBinOffsetHandle;
        Graphics::RGResourceHandle inBinCountHandle; ///< Tail of the last group of each bin
        Graphics::RGResourceHandle inDispatchArgsHandle;
        Graphics::RGResourceHandle outColorHandle; ///< Background already written by BinClassifyPass

        SmallVector<u32, Rasterizer::Config::MAX_RESOLVE_BINS> binBundles; ///< bin -> a psoBundleID of it (its resolve PSO)

        Graphics::RHI::IDescriptorSet* transientDescriptorSet = nullptr;

        u32               materialPassSlot;
        IMaterialLibrary* matLib;
    };

    void registerShaders( Graphics::IShaderRegistry& /*shaders*/ ) override { /*NO OP*/ }
    void createPipelines( Graphics::IPipelineRegistry& /*pipelines*/ ) override { /*NO OP*/ }

    void addToGraph( Graphics::RenderGraphBuilder& builder, const Config& seedData ) {
        builder.addPass<Config>( "Vis Resolve Binned Pass", seedData,
                                 []( Graphics::RenderPassBuilder& pb, Config& data ) {
        data.inVisHandle = pb.read( data.inVisHandle, Graphics::RHI::ResourceState::ShaderResource );
        data.inGlobalBufferHandles.read( pb );
        for ( auto& handle : data.inParticleBufferHandles )
            handle = pb.read( handle, Graphics::RHI::ResourceState::ShaderResource );
        data.inPixelListHandle    = pb.read( data.inPixelListHandle, Graphics::RHI::ResourceState::ShaderResource );
        data.inBinOffsetHandle    = pb.read( data.inBinOffsetHandle, Graphics::RHI::ResourceState::ShaderResource );
        data.inBinCountHandle     = pb.read( data.inBinCountHandle, Graphics::RHI::ResourceState::ShaderResource );
        data.inDispatchArgsHandle = pb.read( data.inDispatchArgsHandle, Graphics::RHI::ResourceState::IndirectArgument );
        data.outColorHandle       = pb.write( data.outColorHandle, Graphics::RHI::ResourceState::UnorderedAccess ); },
                                 [this]( const Config& data, Graphics::RenderPassContext& ctx ) { this->execute( data, ctx ); } );
    }

private:
    void execute( const Config& data, Graphics::RenderPassContext& ctx ) {
        auto* cmd       = ctx.cmd;
        auto* matLayout = ctx.pipelines.getLayout( data.matLib->getPassProfile( data.materialPassSlot ).layoutHandle );

        // SPACE 0: Frame data. Geometry & materials are bindless
        cmd->bindDescriptorSet( (u32)Rasterizer::Config::LayoutSetType::FrameBound, data.transientDescriptorSet, matLayout, Graphics::RHI::PipelineBindPoint::Compute );

        // SPACE 1: Vis buffer + binned pixels in, color out
        auto* ioSet = ctx.allocateSet( matLayout, Graphics::PassIO::SET_INDEX );
        ioSet->attach( Graphics::PassIO::texture( 0 ), Graphics::RHI::DescriptorType::SRV_Image, ctx.getTexture( data.inVisHandle ) );
        ioSet->attach( Graphics::PassIO::buffer( 0 ), Graphics::RHI::DescriptorType::SRV_Buffer, ctx.getBuffer( data.inPixelListHandle ) );
        ioSet->attach( Graphics::PassIO::buffer( 1 ), Graphics::RHI::DescriptorType::SRV_Buffer, ctx.getBuffer( data.inBinOffsetHandle ) );
        ioSet->attach( Graphics::PassIO::buffer( 2 ), Graphics::RHI::DescriptorType::SRV_Buffer, ctx.getBuffer( data.inBinCountHandle ) );
        ioSet->attach( Graphics::PassIO::rwTexture( 0 ), Graphics::RHI::DescriptorType::UAV_Image, ctx.getTexture( data.outColorHandle ) );
        cmd->bindDescriptorSet( Graphics::PassIO::SET_INDEX, ioSet, matLayout, Graphics::RHI::PipelineBindPoint::Compute );

        auto* args = ctx.getBuffer( data.inDispatchArgsHandle );
        for ( u32 bin = 0; bin < (u32)data.binBundles.size(); ++bin )
        {
            Graphics::PipelineHandle psoHandle = data.matLib->getPipelineHandle( data.binBundles[bin], data.materialPassSlot );
            if ( !psoHandle.isValid() )
                continue;

            cmd->bindComputePipeline( ctx.pipelines.getComputePipeline( psoHandle ) );

            // Groups computed on the GPU (0 if the bin got no pixels). No pushConstants: the signature writes the bin ID
            cmd->dispatchIndirect( args, bin * sizeof( Graphics::RHI::DispatchIndirectCommand ), 1 );
        }
    }
};

} // namespace Core::Render
AXION_NAMESPACE_END
