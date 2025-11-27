#pragma once
#include "Axion/Common/Math.h"
#include "Axion/Graphics/RHI/Common.h"
#include "Axion/Graphics/RHI/Descriptor.h"

AXION_NAMESPACE_BEGIN

namespace Graphics::RHI {

// Shader stage container (binary)
struct ShaderModule {
    ShaderType  type;
    const void* code       = nullptr;
    size_t      codeSize   = 0;
    std::string entryPoint = "main";
    // For DX12 this should be a contiguous DXIL blob (VS/PS)
};

DEFINE_COM_PTR_FOR_TYPE( IPipelineLayout, PipelineLayout )

class IPipelineLayout : public IResource
{
public:
    struct PushConstantDesc {
        uint        size      = 0;
        ShaderStage stageMask = ShaderStage::Vertex | ShaderStage::Pixel;
    };
    struct Description {
        std::vector<DescriptorLayoutDesc> sets;
        PushConstantDesc                  pushConstant;
        std::string                       debugName = "";
    };
    virtual ~IPipelineLayout()                        = default;
    virtual const Description& getDescription() const = 0;
};

typedef IPipelineLayout::Description PipelineLayoutDesc;

DEFINE_COM_PTR_FOR_TYPE( IGraphicPipeline, GraphicPipeline )

struct VertexAttribute {
    std::string semanticName; // "POSITION", "TEXCOORD", etc.
    uint        semanticIndex     = 0;
    Format      format            = Format::RGBA32_FLOAT;
    uint        inputSlot         = 0;
    uint        alignedByteOffset = AUTO_VAL;
    uint        instanceStepRate  = 0;
};

struct VertexBinding {
    uint stride      = 0;
    uint inputSlot   = 0;
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
    bool                         alphaToCoverage = false;
    std::vector<BlendAttachment> attachments; // one per RTV slot
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

class IGraphicPipeline : public IResource
{
public:
    struct Description {

        std::vector<ShaderModule> shaderModules;
        IPipelineLayout*          layout = nullptr;

        std::vector<VertexBinding>   bindings;
        std::vector<VertexAttribute> attributes;

        PrimitiveTopology topology    = PrimitiveTopology::TriangleList;
        uint              sampleCount = 1;

        std::vector<Format> renderTargetFormats; // empty -> no color outputs
        Format              depthStencilFormat = Format::UNKNOWN;

        BlendState        blendState;
        RasterizerState   rasterizerState;
        DepthStencilState depthStencilState;

        uint        sampleMask = 0xFFFFFFFF;
        std::string debugName  = "";
    };
    virtual ~IGraphicPipeline()                       = default;
    virtual const Description& getDescription() const = 0;
};

typedef IGraphicPipeline::Description GraphicPipelineDesc;

DEFINE_COM_PTR_FOR_TYPE( IComputePipeline, ComputePipeline )

class IComputePipeline : public IResource
{
public:
    struct Description {
        ShaderModule     shaderModule;        ///< Only one: compute shader
        IPipelineLayout* layout    = nullptr; ///< Root signature
        std::string      debugName = "";

        // Optional metadata (for reflection or validation)
        Math::iVec3 threadGroupSize = { 0, 0, 0 }; // (x, y, z) group size from shader
    };

    virtual ~IComputePipeline()                       = default;
    virtual const Description& getDescription() const = 0;
};

typedef IComputePipeline::Description ComputePipelineDesc;

} // namespace Graphics::RHI

AXION_NAMESPACE_END
