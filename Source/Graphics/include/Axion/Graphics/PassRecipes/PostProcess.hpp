

// struct ToneMappingPass {

//     Graphics::PipelineHandle   pipelineHandle;
//     Graphics::RGResourceHandle inputHandle;
//     Graphics::RGResourceHandle outputHandle;

//     struct Data {
//         Graphics::RGResourceHandle inputHDR;
//         Graphics::RGResourceHandle outputLDR;
//     };

//     void setup( Graphics::RenderPassBuilder& builder, Data& data ) {
//         data.inputHDR  = builder.read( inputHandle );
//         data.outputLDR = builder.write( outputHandle );
//     }

//     void execute( const Data& data, Graphics::RenderPassContext& ctx ) {
//         auto* pso = ctx.pipelines.getComputePipeline( pipelineHandle );

//         auto* texIn  = ctx.getTexture( data.inputHDR );
//         auto* texOut = ctx.getTexture( data.outputLDR );

//         auto* set0 = ctx.allocateSet( pso->getDescription().layout, 0 );
//         auto* set1 = ctx.allocateSet( pso->getDescription().layout, 1 );
//         set0->attach( 0, texIn, Graphics::RHI::ResourceState::ShaderResource );
//         set1->attach( 0, texOut, Graphics::RHI::ResourceState::UnorderedAccess );

//         ctx.cmd->bindComputePipeline( pso );
//         ctx.cmd->bindDescriptorSet( 0, set0 );
//         ctx.cmd->bindDescriptorSet( 1, set1 );

//         ctx.cmd->dispatch( texIn->getDescription().size );
//     }
// };