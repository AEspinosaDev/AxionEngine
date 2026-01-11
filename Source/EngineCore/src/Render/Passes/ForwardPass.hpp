// // Dentro de la lambda del RenderPass
// const uint passTypeIndex = (uint)MaterialPassType::ForwardTriOpaque;
// const auto& archetypes   = _matLib.getArchetypesRaw(); // Devuelve const vector&

// for (const auto& instance : gpuScene.instances()) 
// {
//     // 1. Acceso Directo al Arquetipo (Array Access)
//     const auto& arch = archetypes[ instance.materialArchetypeID ];

//     // 2. Acceso Directo al Pipeline de este pase (Array Access)
//     auto pipeline = arch.pipelines[ passTypeIndex ];

//     // 3. Validación (Por si este material no soporta este pase)
//     if ( !pipeline.isValid() ) continue;

//     // 4. Bind & Draw
//     if (currentPipeline != pipeline) {
//         ctx.bindPipeline(pipeline);
//         currentPipeline = pipeline;
//     }
    
//     // Draw(instance)...
// }