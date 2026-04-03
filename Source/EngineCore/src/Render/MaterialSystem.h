#pragma once
#include "Axion/Core/Render/Common.h"
#include "Axion/Graphics/Subsystems/IPipelineRegistry.h"
#include "Axion/Graphics/Subsystems/IShaderRegistry.h"
#include "Axion/Common/Containers/STLWrapper/Maps.h"
#include "Axion/Common/Containers/STLWrapper/Array.h"

AXION_NAMESPACE_BEGIN

namespace Core::Render {

struct MaterialArchetype {

    MaterialArchetypeDesc desc {};

    //  Resources
    STLW::Array<Graphics::ShaderHandle, (size_t)MaterialPassType::Count> shaderHandles;
    using TopologyArray = STLW::Array<Graphics::PipelineHandle, (size_t)TopologyType::Count>;
    STLW::Array<TopologyArray, (size_t)MaterialPassType::Count> pipelines;

    MaterialArchetype() {
    }

    Graphics::PipelineHandle getPipeline( MaterialPassType pass, TopologyType topo ) const {
        return pipelines[(size_t)pass][(size_t)topo];
    }
};

struct MaterialPassProfile {
    STLW::Vector<Graphics::Format> renderTargetFormats;
    Graphics::Format               depthTargetFormat = Graphics::Format::D32;
};

enum MaterialPassSupportFlags : byte
{
    MaterialPassSupportNone        = 1 << 0,
    MaterialPassSupportOpaque      = 1 << 1,
    MaterialPassSupportTranslucent = 1 << 2,
    MaterialPassSupportDepth       = 1 << 3,
    MaterialPassSupportShadow      = 1 << 4,
    MaterialPassSupportVoxel       = 1 << 5,
    MaterialPassSupportWireframe   = 1 << 6,
    MaterialPassSupportRaytracing  = 1 << 7,
    MaterialPassSupportCount       = 1 << 8,
};

AXION_ENUM_CLASS_FLAG_OPERATORS( MaterialPassSupportFlags )

class MaterialLibrary
{
public:
    class ArchetypeBuilder;

    ArchetypeBuilder beginMaterial( StringView name );
    void             registerArchetype( const MaterialArchetypeDesc& desc );

    void init( Graphics::API api, MaterialPassSupportFlags defaultPassSupportFlags = MaterialPassSupportNone );

    void setTargetLayout( Graphics::PipelineLayoutHandle globalLayout );

    void setPassFormats( MaterialPassType passType, const MaterialPassProfile& profile );

    void registerShaders( Graphics::IShaderRegistry& shaders );

    void createPipelines( Graphics::IPipelineRegistry& pipelines );

    AXION_FORCE_INLINE const STLW::Vector<MaterialArchetype>& getArchetypesRaw() const { return _archetypes; }
    AXION_FORCE_INLINE u64                                  getArchetypesCount() const { return _archetypes.size(); }
    AXION_FORCE_INLINE bool                                   isInitialized() const { return _initialized; }

    u32 getArchetypeID( StringView name ) const;

private:
    void enforceDefaultPasses( MaterialArchetypeDesc& desc );

    std::string           toString( MaterialPassType type );
    std::string           toString( TopologyType type );
    MaterialTopologyFlags topologyToFlags( TopologyType type );

    STLW::Vector<MaterialArchetype>       _archetypes;
    STLW::UnorderedMap<String64, u32> _archetypeLookup;

    STLW::Array<MaterialPassProfile, (size_t)MaterialPassType::Count> _passProfiles;

    Graphics::PipelineLayoutHandle _globalLayoutHandle; // Global Shader Contract

    Graphics::API            _api;
    bool                     _initialized = false;
    MaterialPassSupportFlags _defaultPassSupportFlags;

    friend class ArchetypeBuilder;
};

class MaterialLibrary::ArchetypeBuilder
{
public:
    ArchetypeBuilder( MaterialLibrary& l, StringView name )
        : _lib( l ) {
        _archDesc.name                = name;
        _archDesc.topologiesSupported = MaterialTopologyTriangles;
    }

    ArchetypeBuilder& addPass( MaterialPassType                                  type,
                               const STLW::String&                               path,
                               const STLW::Vector<Graphics::Shader::EntryPoint>& entryPoints,
                               const STLW::String&                               customIncludePath = "" ) {
        MaterialArchetypePassConfig p;
        p.shaderPath        = path;
        p.passType          = type;
        p.entryPoints       = entryPoints;
        p.customIncludePath = customIncludePath;
        _archDesc.passConfigs.push_back( p );
        return *this;
    }

    // MaterialBuilder& addForwardStandard( StringView path ) {
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