// #pragma once
// #include "../DrawIndirect.h"
// #include "../PassSystem.h"
// #include "Axion/Graphics/Subsystems/RenderGraph.h"

// AXION_NAMESPACE_BEGIN
// namespace Core::Render {

// class DepthPrePass : public IRenderPass
// {
// public:
//     struct Config {
//         // Recursos Globales (Solo geometría y frame)
//         Graphics::RHI::BufferView inFrameView;
//         Graphics::RHI::BufferView inMeshesView;
//         Graphics::RHI::BufferView inInstancesView;
        
//         // Culling & Indirect
//         Graphics::RGResourceHandle inCulledRedirectBufferHandle;
//         Graphics::RHI::BufferView  inCulledRedirectView; // Offset 0
        
//         IndirectCommandData        indirectData;
//         Graphics::RGResourceHandle inIndirectBufferHandle;

//         Graphics::RGResourceHandle outDepthHandle;
//     };

//     void registerShaders( Graphics::IShaderRegistry& shaders ) override { 
//         // Registramos solo el Vertex Shader universal
//         shaders.registerShader( "DepthOnly", "Shaders/DepthOnly.slang", "vertexMain", Graphics::ShaderType::Vertex );
//     }

//     void createPipelines( Graphics::IPipelineRegistry& pipelines ) override {
        
//         // 1. LAYOUT PRIVADO (A MEDIDA) 🔒
//         // -----------------------------------------------------------
//         Graphics::PipelineLayoutDesc layoutDesc;
        
//         // Space 0: Frame Data (b0)
//         layoutDesc.addRange( Graphics::RHI::DescriptorRangeType::CBV, 1, 0, 0 ); 
        
//         // Space 1: Geometry Data (t0=Meshes, t1=Instances, t2=Redirect)
//         layoutDesc.addRange( Graphics::RHI::DescriptorRangeType::SRV, 3, 0, 1 );

//         _depthLayout = pipelines.createPipelineLayout( layoutDesc );

//         // 2. CREACIÓN DE LOS 3 PSOs 🎨
//         // -----------------------------------------------------------
//         // Reutilizamos la misma descripción base para todos
//         Graphics::GraphicsPipelineDesc psoDesc;
//         psoDesc.layout           = _depthLayout;
//         psoDesc.vertexShader     = pipelines.getShader("DepthOnly"); // El mismo shader para todos
//         psoDesc.pixelShader      = nullptr; // ¡DOBLE VELOCIDAD Z! ⚡
        
//         psoDesc.depthStencilState.depthTestEnable  = true;
//         psoDesc.depthStencilState.depthWriteEnable = true;
//         psoDesc.depthStencilState.depthFunc        = Graphics::RHI::CompareFunc::Less; // O LessEqual
        
//         psoDesc.renderTargetCount = 0; // Solo Depth
//         psoDesc.dsvFormat         = Graphics::RHI::Format::D32_FLOAT;

//         // --- PSO 1: TRIÁNGULOS ---
//         psoDesc.inputAssembly.topology = Graphics::RHI::TopologyType::Triangles;
//         _psoTriangle = pipelines.createGraphicsPipeline( psoDesc );

//         // --- PSO 2: LÍNEAS ---
//         psoDesc.inputAssembly.topology = Graphics::RHI::TopologyType::Lines;
//         _psoLine = pipelines.createGraphicsPipeline( psoDesc );

//         // --- PSO 3: PUNTOS ---
//         psoDesc.inputAssembly.topology = Graphics::RHI::TopologyType::Points;
//         _psoPoint = pipelines.createGraphicsPipeline( psoDesc );
//     }

//     void addToGraph( Graphics::RenderGraphBuilder& builder, Config& seedData ) {
//         builder.addPass<Config>( "DepthPrePass", seedData,
//             []( Graphics::RenderPassBuilder& pb, Config& data ) {
//                 // Solo escribimos Depth, y leemos geometría
//                 data.outDepthHandle = pb.write( data.outDepthHandle, Graphics::RHI::ResourceState::DepthWrite );
//                 data.inIndirectBufferHandle = pb.read( data.inIndirectBufferHandle, Graphics::RHI::ResourceState::IndirectArgument );
//                 data.inCulledRedirectBufferHandle = pb.read( data.inCulledRedirectBufferHandle, Graphics::RHI::ResourceState::ShaderResource );
//             },
//             [this]( const Config& data, Graphics::RenderPassContext& ctx ) { this->execute( data, ctx ); } );
//     }

// private:
//     Graphics::PipelineLayoutHandle   _depthLayout;
//     Graphics::PipelineHandle         _psoTriangle;
//     Graphics::PipelineHandle         _psoLine;
//     Graphics::PipelineHandle         _psoPoint;

//     void execute( const Config& data, Graphics::RenderPassContext& ctx ) {
//         auto* cmd = ctx.cmd;

//         // 1. SETUP RENDER TARGETS (Solo Depth, Clear)
//         auto* dsv = ctx.getTexture( data.outDepthHandle );
//         Graphics::RHI::RenderingDesc info;
//         info.renderArea = dsv->getDescription().size.to2D();
//         info.depthStencilAttachment = { 
//             .texture = dsv, 
//             .depthLoadOp = LoadOp::Clear, 
//             .depthStoreOp = StoreOp::Store 
//         };
//         cmd->beginRendering( info );

//         // 2. BIND LAYOUT PRIVADO
//         auto* layout = ctx.pipelines.getLayout( _depthLayout );
        
//         // Space 0: Frame
//         auto* set0 = ctx.allocateSet( layout, 0 );
//         set0->attachBufferView( 0, data.inFrameView, Graphics::RHI::ResourceState::ConstantBuffer );
//         cmd->bindDescriptorSet( 0, set0, layout );

//         // Space 1: Geometry
//         auto* set1 = ctx.allocateSet( layout, 1 );
//         set1->attachBufferView( 0, data.inMeshesView, Graphics::RHI::ResourceState::ShaderResource );
//         set1->attachBufferView( 1, data.inInstancesView, Graphics::RHI::ResourceState::ShaderResource );
        
//         // Redirect Buffer (Correcto con Offset 0)
//         Graphics::RHI::BufferView culledView = data.inCulledRedirectView;
//         culledView.buffer = ctx.getBuffer( data.inCulledRedirectBufferHandle );
//         culledView.offset = 0; 
//         set1->attachBufferView( 2, culledView, Graphics::RHI::ResourceState::ShaderResource );
        
//         cmd->bindDescriptorSet( 1, set1, layout );
        
//         // Index Buffer Global (Asumo que lo tienes en data o ctx)
//         // cmd->bindIndexBuffer( ... ); 

//         // 3. DRAW LOOP OPTIMIZADO
//         auto* indirectBuffer = ctx.getBuffer( data.inIndirectBufferHandle );
        
//         // Cachear PSOs para no buscarlos cada vez
//         auto* psoTri   = ctx.pipelines.getGraphicPipeline( _psoTriangle );
//         auto* psoLine  = ctx.pipelines.getGraphicPipeline( _psoLine );
//         auto* psoPoint = ctx.pipelines.getGraphicPipeline( _psoPoint );

//         Graphics::RHI::TopologyType currentTopoType = Graphics::RHI::TopologyType::Undefined;

//         for ( const auto& batch : data.indirectData.batches )
//         {
//             if ( batch.drawCount == 0 ) continue;

//             // Determinar tipo de topología del batch
//             // (Asumo que tienes una función helper o que puedes deducirlo del ID)
//             auto topoType = getTopologyTypeFromID( batch.topologyID ); 

//             // Solo cambiamos PSO si cambia el TIPO de primitiva (Tri vs Line vs Point)
//             if ( topoType != currentTopoType )
//             {
//                 switch ( topoType )
//                 {
//                     case Graphics::RHI::TopologyType::Triangles: cmd->bindGraphicPipeline( psoTri ); break;
//                     case Graphics::RHI::TopologyType::Lines:     cmd->bindGraphicPipeline( psoLine ); break;
//                     case Graphics::RHI::TopologyType::Points:    cmd->bindGraphicPipeline( psoPoint ); break;
//                 }
//                 currentTopoType = topoType;
//             }
            
//             // IMPORTANTE: Aunque el PSO define el "Type" (Triángulo), 
//             // la IA necesita saber si es TriangleList o Strip.
//             // Si tus batches mezclan List y Strip, necesitas llamar a esto:
//             cmd->setPrimitiveTopology( (Graphics::RHI::PrimitiveTopology)batch.topologyID );

//             cmd->drawIndexedIndirect(
//                 indirectBuffer,
//                 batch.bufferOffset,
//                 batch.drawCount );
//         }

//         cmd->endRendering();
//     }

//     // Helper sencillo (depende de cómo definas tus IDs en el motor)
//     Graphics::RHI::TopologyType getTopologyTypeFromID( uint topologyID ) {
//         // Ejemplo:
//         if (topologyID == D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST) return Graphics::RHI::TopologyType::Triangles;
//         if (topologyID == D3D_PRIMITIVE_TOPOLOGY_LINELIST)     return Graphics::RHI::TopologyType::Lines;
//         if (topologyID == D3D_PRIMITIVE_TOPOLOGY_POINTLIST)    return Graphics::RHI::TopologyType::Points;
//         return Graphics::RHI::TopologyType::Triangles; // Default
//     }
// };

// } // namespace
// AXION_NAMESPACE_END