#pragma once
#include "Axion/Core/Render/Defines.h"
#include "Axion/Graphics/Subsystems/PipelineRegistry.h"
#include "Axion/Graphics/Subsystems/ShaderRegistry.h"
#include <array>
#include <string>
#include <unordered_map>
#include <vector>

AXION_NAMESPACE_BEGIN

namespace Core::Render {

struct MaterialArchetype {

    MaterialArchetypeDesc desc {};

    //  Resources
    std::array<Graphics::ShaderHandle, (size_t)MaterialPassType::Count> shaderHandles;
    using TopologyArray = std::array<Graphics::PipelineHandle, (size_t)TopologyType::Count>;
    std::array<TopologyArray, (size_t)MaterialPassType::Count> pipelines;

    MaterialArchetype() {
    }

    Graphics::PipelineHandle getPipeline( MaterialPassType pass, TopologyType topo ) const {
        return pipelines[(size_t)pass][(size_t)topo];
    }
};

struct MaterialPassProfile {
    std::vector<Graphics::Format> renderTargetFormats;
    Graphics::Format              depthTargetFormat = Graphics::Format::D32;
};

class MaterialLibrary
{
public:
    class ArchetypeBuilder;

    ArchetypeBuilder beginMaterial( const std::string& name );
    void             registerArchetype( const MaterialArchetypeDesc& desc );

    void init( Graphics::API api );

    void setTargetLayout( Graphics::PipelineLayoutHandle globalLayout );

    void setPassFormats( MaterialPassType passType, const MaterialPassProfile& profile );

    void registerShaders( Graphics::IShaderRegistry& shaders );

    void createPipelines( Graphics::IPipelineRegistry& pipelines );

    AXION_FORCE_INLINE const std::vector<MaterialArchetype>& getArchetypesRaw() const { return _archetypes; }
    AXION_FORCE_INLINE ulong                                 getArchetypesCount() const { return _archetypes.size(); }
    AXION_FORCE_INLINE bool                                  isInitialized() const { return _initialized; }

    uint getArchetypeID( const std::string& name ) const;

   

private:
    std::string           toString( MaterialPassType type );
    std::string           toString( TopologyType type );
    MaterialTopologyFlags topologyToFlags( TopologyType type );

    std::vector<MaterialArchetype>        _archetypes;
    std::unordered_map<std::string, uint> _archetypeLookup;

    std::array<MaterialPassProfile, (size_t)MaterialPassType::Count> _passProfiles;

    Graphics::PipelineLayoutHandle _globalLayoutHandle; // Global Shader Contract

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
        MaterialArchetypePassConfig p;
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
    // ArchetypeBuilder& supportForTopology( TopologyType type ) {
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