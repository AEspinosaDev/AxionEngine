
#pragma once
#include <Axion/Common/Graphics/Defines.h>

AXION_NAMESPACE_BEGIN

namespace Core::Render {

// Renderer feature flags
enum RendererFlags : uint
{
    RendererNone         = 0,
    RendererEnableGUI    = 1 << 0,
    RendererEnableDebug  = 1 << 1,
    RendererEnableFXAA   = 1 << 2,
    RendererEnableMSAAx2 = 1 << 3,
    RendererEnableMSAAx4 = 1 << 4,
    RendererEnableMSAAx8 = 1 << 5,
    RendererEnableTAA    = 1 << 6,
    RendererEnableHDR    = 1 << 7,
    RendererEnableDLSS   = 1 << 8,
};

AXION_ENUM_CLASS_FLAG_OPERATORS( RendererFlags )

enum class TopologyType : uchar
{
    Triangles = 0,
    Lines     = 1,
    Points    = 2,
    Meshlets  = 3,
    Count
};

static constexpr Graphics::PrimitiveTopology toGFXTopology( TopologyType type ) {
    switch ( type )
    {
        case TopologyType::Triangles:
            return Graphics::PrimitiveTopology::TriangleList;
        case TopologyType::Lines:
            return Graphics::PrimitiveTopology::LineList;
        case TopologyType::Points:
            return Graphics::PrimitiveTopology::PointList;
        case TopologyType::Meshlets:
            return Graphics::PrimitiveTopology::TriangleList;
        default:
            return Graphics::PrimitiveTopology::TriangleList;
    }
}

enum class MaterialPassType : uchar
{
    Opaque      = 0,
    Blend       = 1,
    Geometry    = 2,
    Composition = 3,
    Depth       = 4,
    Shadow      = 5,
    Raytracing  = 6,
    Wireframe   = 7,

    Count
};

enum MaterialTopologyFlags : uchar
{
    MaterialTopologyNone      = 0,
    MaterialTopologyTriangles = 1 << 0,
    MaterialTopologyLines     = 1 << 1,
    MaterialTopologyPoints    = 1 << 2,
    MaterialTopologyMeshlets  = 1 << 3,
};

AXION_ENUM_CLASS_FLAG_OPERATORS( MaterialTopologyFlags )

struct MaterialArchetypePassConfig {
    MaterialPassType                          passType;
    std::string                               shaderPath;
    std::vector<Graphics::Shader::EntryPoint> entryPoints;
    std::string                               customIncludePath;

    Graphics::FillMode  fillMode   = Graphics::FillMode::Solid;
    Graphics::CullMode  cullMode   = Graphics::CullMode::Front;
    Graphics::BlendOp   blendOp    = Graphics::BlendOp::Add;
    Graphics::CompareOp depthOp    = Graphics::CompareOp::LessEqual;
    bool                depthWrite = true;
    bool                depthTest  = true;

    std::string customPassAlias = "";
};

struct MaterialArchetypeDesc {
    std::string                              name;
    std::vector<MaterialArchetypePassConfig> passConfigs;
    MaterialTopologyFlags                    topologiesSupported = MaterialTopologyTriangles;
    uint                                     payloadSize         = 0;
};

} // namespace Core::Render

AXION_NAMESPACE_END