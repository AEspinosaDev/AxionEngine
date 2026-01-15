#pragma once
#include "Axion/Graphics/Subsystems/PipelineRegistry.h"
#include "Axion/Graphics/Subsystems/ShaderRegistry.h"
#include <array>
#include <string>
#include <unordered_map>
#include <vector>

AXION_NAMESPACE_BEGIN

namespace Core::Render {

enum class MaterialPassType : uchar
{
    Opaque       = 0,
    Blend        = 1,
    Geometry     = 2,
    Shadow       = 3,
    Voxelization = 4,
    Raytracing   = 5,

    Count
};

enum class MaterialTopologyType : uchar
{
    Triangles = 0,
    Lines     = 1,
    Points    = 2,
    Meshlets  = 3,
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

struct MaterialArchetype {
    struct PassConfig {
        MaterialPassType                          passType;
        std::string                               shaderPath;
        std::vector<Graphics::Shader::EntryPoint> entryPoints;
        std::string                               customIncludePath;

        Graphics::FillMode fillMode = Graphics::FillMode::Solid;
        Graphics::CullMode cullMode = Graphics::CullMode::None;
        Graphics::BlendOp  blendOp  = Graphics::BlendOp::Add;
    };
    struct Description {
        std::string             name;
        std::vector<PassConfig> passConfigs;
        MaterialTopologyFlags   topologiesSupported = MaterialTopologyTriangles;
        uint                    payloadSize         = 0;
    };

    Description desc {};

    //  Resources
    std::array<Graphics::ShaderHandle, (size_t)MaterialPassType::Count> shaderHandles;
    using TopologyArray = std::array<Graphics::PipelineHandle, (size_t)MaterialTopologyType::Count>;
    std::array<TopologyArray, (size_t)MaterialPassType::Count> pipelines;

    MaterialArchetype() {
    }

    Graphics::PipelineHandle getPipeline( MaterialPassType pass, MaterialTopologyType topo ) const {
        return pipelines[(size_t)pass][(size_t)topo];
    }
};

typedef MaterialArchetype::Description MaterialArchetypeDesc;

struct MaterialPassProfile {
    std::vector<Graphics::Format> renderTargetFormats;
    Graphics::Format              depthTargetFormat = Graphics::Format::D32;
};

class MaterialLibrary
{
public:
    class ArchetypeBuilder;

    ArchetypeBuilder beginMaterial( const std::string& name );

    void init( Graphics::API api );

    void setTargetLayout( Graphics::PipelineLayoutHandle globalLayout );

    void setPassFormats( MaterialPassType passType, const MaterialPassProfile& profile );

    void registerShaders( Graphics::IShaderRegistry& shaders );

    void createPipelines( Graphics::IPipelineRegistry& pipelines );

    AXION_FORCE_INLINE const std::vector<MaterialArchetype>& getArchetypesRaw() const { return _archetypes; }
    AXION_FORCE_INLINE bool                                  isInitialized() const { return _initialized; }

    uint getArchetypeID( const std::string& name );

private:
    void registerArchetype( const MaterialArchetypeDesc& desc );

    std::string                 toString( MaterialPassType type );
    std::string                 toString( MaterialTopologyType type );
    Graphics::PrimitiveTopology toRHITopology( MaterialTopologyType type );
    MaterialTopologyFlags       topologyToFlags( MaterialTopologyType type );

    std::vector<MaterialArchetype>        _archetypes;
    std::unordered_map<std::string, uint> _archetypeLookup;

    std::array<MaterialPassProfile, (size_t)MaterialPassType::Count> _passProfiles;

    Graphics::PipelineLayoutHandle _globalLayoutHandle; //Global Shader Contract

    Graphics::API _api;
    bool          _initialized = false;

    friend class ArchetypeBuilder;
};

class MaterialLibrary::ArchetypeBuilder
{
public:
    ArchetypeBuilder( MaterialLibrary& l, const std::string& name )
        : _lib( l ) {
        _archDesc.name                = name;
        _archDesc.topologiesSupported = MaterialTopologyTriangles;
    }

    ArchetypeBuilder& addPass( MaterialPassType                                 type,
                               const std::string&                               path,
                               const std::vector<Graphics::Shader::EntryPoint>& entryPoints,
                               const std::string&                               customIncludePath = "" ) {
        MaterialArchetype::PassConfig p;
        p.shaderPath        = path;
        p.passType          = type;
        p.entryPoints       = entryPoints;
        p.customIncludePath = customIncludePath;
        _archDesc.passConfigs.push_back( p );
        return *this;
    }

    // MaterialBuilder& addForwardStandard( const std::string& path ) {
    //     addPass( MaterialPassPermutation::Opaque, path, "vsForward", "psForward" );
    //     addPass( MaterialPassPermutation::Blend, path, "vsForward", "psForward" );
    //     return *this;
    // }
    // ArchetypeBuilder& supportForTopology( MaterialTopologyType type ) {
    //     _archDesc.topologiesSupported |= MaterialLibrary::topologyToFlags( type );
    //     return *this;
    // }

    void finish() {
        _lib.registerArchetype( _archDesc );
    }

private:
    MaterialLibrary&      _lib;
    MaterialArchetypeDesc _archDesc;
};

} // namespace Core::Render
AXION_NAMESPACE_END