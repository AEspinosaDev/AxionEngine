#pragma once
#include "Axion/Common/Math.h"
#include "Axion/Graphics/RHI/Common.h"
#include "Axion/Graphics/RHI/IDescriptor.h"

AXION_NAMESPACE_BEGIN

namespace Graphics::RHI {

// Shader stage container (binary)
struct ShaderModule {
    ShaderType  type;
    const void* code       = nullptr;
    size_t      codeSize   = 0;
    String32    entryPoint = "main";
    // For DX12 this should be a contiguous DXIL blob (VS/PS)
};

#pragma region Layout
DEFINE_OWNER_PTR_FOR_TYPE( IPipelineLayout, PipelineLayout )

class IPipelineLayout : public IDeviceObject
{
public:
    struct PushConstantDesc {
        u32        size           = 0;
        u32        customSpace    = 1;
        u32        customRegister = 0;
        ShaderStage stageMask      = ShaderStage::Vertex | ShaderStage::Pixel;
    };

    struct Description {
        STLW::Vector<DescriptorLayoutDesc> sets;
        PushConstantDesc                   pushConstant;
        String64                           debugName = "";

        bool enableIndirectRendering = false;
    };
    virtual ~IPipelineLayout()                                        = default;
    virtual const Description& getDescription() const                 = 0;
    virtual u32               getViewCount( u32 setIndex ) const    = 0;
    virtual u32               getSamplerCount( u32 setIndex ) const = 0;
    virtual u32               getAccelCount( u32 setIndex ) const   = 0;
};

typedef IPipelineLayout::Description PipelineLayoutDesc;

#pragma endregion
#pragma region Graphic
DEFINE_OWNER_PTR_FOR_TYPE( IGraphicPipeline, GraphicPipeline )

struct VertexAttribute {
    String32 semanticName      = ""; // "POSITION", "TEXCOORD", etc.
    u32     semanticIndex     = 0;
    Format   format            = Format::RGBA32_FLOAT;
    u32     inputSlot         = 0;
    u32     alignedByteOffset = AUTO_VAL;
    u32     instanceStepRate  = 0;
};

struct VertexBinding {
    u32 stride      = 0;
    u32 inputSlot   = 0;
    bool perInstance = false;
};

// Blend and rasterizer / depth states
struct BlendAttachment {
    bool        blendEnable = false;
    BlendFactor srcColor    = BlendFactor::One;
    BlendFactor dstColor    = BlendFactor::Zero;
    BlendOp     colorOp     = BlendOp::Add;
    BlendFactor srcAlpha    = BlendFactor::One;
    BlendFactor dstAlpha    = BlendFactor::Zero;
    BlendOp     alphaOp     = BlendOp::Add;
    uint8_t     writeMask   = 0xF; // RGBA
};

struct BlendState {
    bool                          alphaToCoverage = false;
    STLW::Vector<BlendAttachment> attachments; // one per RTV slot
};

struct RasterizerState {
    FillMode fillMode              = FillMode::Solid;
    CullMode cullMode              = CullMode::Back;
    bool     frontCounterClockwise = false;
    int      depthBias             = 0;
    float    depthBiasClamp        = 0.0f;
    float    slopeScaledDepthBias  = 0.0f;
    bool     depthClipEnable       = true;
    bool     multisampleEnable     = false;
    bool     antialiasedLineEnable = false;
};

struct DepthStencilState {
    bool      depthEnable    = true;
    bool      depthWriteMask = true;
    CompareOp depthFunc      = CompareOp::LessEqual;
    bool      stencilEnable  = false;
    // stencil ops omitted for brevity (add if needed)
};

class IGraphicPipeline : public IDeviceObject
{
public:
    struct Description {

        STLW::Vector<ShaderModule> shaderModules;
        IPipelineLayout*           layout = nullptr;

        STLW::Vector<VertexBinding>   bindings;
        STLW::Vector<VertexAttribute> attributes;

        PrimitiveTopology topology    = PrimitiveTopology::TriangleList;
        u32              sampleCount = 1;

        STLW::Vector<Format> renderTargetFormats;
        Format               depthStencilFormat = Format::UNKNOWN;

        BlendState        blendState;
        RasterizerState   rasterizerState;
        DepthStencilState depthStencilState;

        u32     sampleMask = 0xFFFFFFFF;
        String64 debugName  = "";
    };
    virtual ~IGraphicPipeline()                       = default;
    virtual const Description& getDescription() const = 0;
};

typedef IGraphicPipeline::Description GraphicPipelineDesc;

#pragma endregion
#pragma region Mesh
DEFINE_OWNER_PTR_FOR_TYPE( IMeshPipeline, MeshPipeline )

class IMeshPipeline : public IDeviceObject
{
public:
    struct Description {

        STLW::Vector<ShaderModule> shaderModules;
        IPipelineLayout*           layout = nullptr;

        PrimitiveTopology topology    = PrimitiveTopology::TriangleList;
        u32              sampleCount = 1;

        STLW::Vector<Format> renderTargetFormats;
        Format               depthStencilFormat = Format::UNKNOWN;

        BlendState        blendState;
        RasterizerState   rasterizerState;
        DepthStencilState depthStencilState;

        u32     sampleMask = 0xFFFFFFFF;
        String64 debugName  = "";
    };
    virtual ~IMeshPipeline()                          = default;
    virtual const Description& getDescription() const = 0;
};

typedef IMeshPipeline::Description MeshPipelineDesc;

#pragma endregion
#pragma region Compute
DEFINE_OWNER_PTR_FOR_TYPE( IComputePipeline, ComputePipeline )

class IComputePipeline : public IDeviceObject
{
public:
    struct Description {
        ShaderModule     shaderModule;        ///< Only one: compute shader
        IPipelineLayout* layout    = nullptr; ///< Root signature
        String64         debugName = "";

        // Optional metadata (for reflection or validation)
        Math::iVec3 threadGroupSize = { 0, 0, 0 }; // (x, y, z) group size from shader
    };

    virtual ~IComputePipeline()                       = default;
    virtual const Description& getDescription() const = 0;
};

typedef IComputePipeline::Description ComputePipelineDesc;

DEFINE_OWNER_PTR_FOR_TYPE( IRayTracingPipeline, RayTracingPipeline )

struct HitGroupDesc {
    String64 name;               // The name to use in the Shader Binding Table
    String64 closestHitShader;   // Entry point name (export) for Closest Hit
    String64 anyHitShader;       // Entry point name (export) for Any Hit (optional)
    String64 intersectionShader; // Entry point name (export) for Intersection (optional)
    String64 callableShader;     // Entry point name (export) for Callables (optional)

    bool isProcedural() const { return !intersectionShader.empty(); }
};

#pragma endregion
#pragma region RTX
class IRayTracingPipeline : public IDeviceObject
{
public:
    struct Description {
        STLW::Vector<ShaderModule> shaderModules;
        IPipelineLayout*           layout = nullptr;
        STLW::Vector<HitGroupDesc> hitGroups;
        // Configuration
        u32 maxDepth         = 1; // How many times rays can bounce (TraceRay calls)
        u32 maxPayloadSize   = 0; // sizeof(RayPayload)
        u32 maxAttributeSize = 8; // sizeof(BuiltInTriangleIntersectionAttributes) is 8 (float2)

        String64 debugName = "";
    };

    virtual ~IRayTracingPipeline()                                                            = default;
    virtual const Description& getDescription() const                                         = 0;
    virtual void*              getShaderIdentifier( const std::string_view exportName ) const = 0;
};

typedef IRayTracingPipeline::Description RayTracingPipelineDesc;

} // namespace Graphics::RHI

AXION_NAMESPACE_END
