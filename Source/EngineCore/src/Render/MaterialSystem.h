enum class RenderPassType {
    Forward = 0,
    Shadow  = 1,
    Depth   = 2,
    Geometry = 3,
    Count
};

struct MaterialArchetype {
    Graphics::ShaderHandle shader;
    
    // Array of pipelines indexed by the Pass Type + Variant
    // E.g., pipelines[Forward][Opaque], pipelines[Forward][Transparent]
    // For simplicity, let's assume variants are handled by the pass request or separate indices
    Graphics::PipelineHandle pipelines[RenderPassType::Count]; 
};