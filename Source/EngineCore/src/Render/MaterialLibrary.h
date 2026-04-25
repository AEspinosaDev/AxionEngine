#pragma once
#include "Axion/Common/Containers/STLWrapper/Array.h"
#include "Axion/Common/Containers/STLWrapper/Maps.h"
#include "Axion/Core/Render/Common.h"
#include "Axion/Graphics/Subsystems/IPipelineRegistry.h"
#include "Axion/Graphics/Subsystems/IShaderRegistry.h"

AXION_NAMESPACE_BEGIN

namespace Core::Render {

// /**
//  * Struct that defines a material archetype given custom  pass and topology permutations
//  */
template <u32 PassCount>
struct MaterialArchetype {
    using ShaderArray   = FixedArray<Graphics::ShaderHandle, PassCount>;
    using TopologyArray = FixedArray<Graphics::PipelineHandle, (u64)Graphics::PrimitiveTopology::Count>;

    // Material archetype desc gives the shader path for the Material BRDF to inherit in the Profile shader
    MaterialArchetypeDesc desc {};

    // Compiled shader handles for each pass type combination, indexed by [pass]
    ShaderArray shaderHandles;
    // Compiled pipeline handles for each pass and topology type combination, indexed by [pass][topology]
    FixedArray<TopologyArray, PassCount> psoHandles;

    MaterialArchetype() {}
    MaterialArchetype( MaterialArchetypeDesc desc )
        : desc( desc ) {}

    Graphics::PipelineHandle getPipeline( u32 passType, u32 topoType ) const {
        return psoHandles[passType][topoType];
    }
};

/**
 * Description of a Material Pass.
 * Defines the render target formats, depth format, and pipeline layout to be used for a specific pass type.
 * This allows for flexible configuration of different passes (e.g., opaque, transparent, shadow) without hardcoding these details in the material archetypes.
 */
struct MaterialPassProfile {

    // Pass profile stores the global layout used by all Materials in this Pass, which defines the expected resources and their bindings.
    Graphics::PipelineLayoutHandle layoutHandle;

    // Pass profile stores the shader used by all Materials in this Pass
    STLW::String                               shaderPath;
    STLW::String                               shaderIncludePath;
    STLW::Vector<Graphics::Shader::EntryPoint> entryPoints;
    bool                                       needsPerMaterialSpecialization = true;

    // Render state configuration for this pass type
    STLW::Vector<Graphics::Format> renderTargetFormats;
    Graphics::Format               depthTargetFormat = Graphics::Format::D32;

    // Additional pass pso shared states configuration
    Graphics::FillMode  fillMode   = Graphics::FillMode::Solid;
    Graphics::CullMode  cullMode   = Graphics::CullMode::Front;
    Graphics::BlendOp   blendOp    = Graphics::BlendOp::Add;
    Graphics::CompareOp depthOp    = Graphics::CompareOp::LessEqual;
    bool                depthWrite = true;
    bool                depthTest  = true;
};

template <u32 PassCount>
class MaterialLibrary
{
public:
    class ArchetypeBuilder;

    ArchetypeBuilder beginMaterial( StringView name );
    void             registerArchetype( const MaterialArchetypeDesc& desc );

    void initialize( Graphics::API api );

    // Material Profiles Api
    void                                              setPassProfile( u32 passSlot, const MaterialPassProfile& profile );
    const FixedArray<MaterialPassProfile, PassCount>& getPassProfiles() const { return _passProfiles; }

    // Resources
    void registerShaders( Graphics::IShaderRegistry& shaders );
    void createPipelines( Graphics::IPipelineRegistry& pipelines );

    AXION_FORCE_INLINE const STLW::Vector<MaterialArchetype<PassCount>>& getArchetypesRaw() const { return _archetypes; }
    AXION_FORCE_INLINE u64                                               getArchetypesCount() const { return _archetypes.size(); }
    AXION_FORCE_INLINE bool                                              isInitialized() const { return _initialized; }

    u32 getArchetypeID( StringView name ) const;

private:
    STLW::Vector<MaterialArchetype<PassCount>> _archetypes;
    STLW::UnorderedMap<String64, u32>          _archetypeLookup;

    FixedArray<MaterialPassProfile, PassCount> _passProfiles;

    Graphics::API _api;
    bool          _initialized = false;

    friend class ArchetypeBuilder;
};

#include "MaterialLibrary.inl"

// El archetype builder cogera ahora la descripcion del arquetipe,
// y creara unos PSOs usadno el shader del profile + el inherit shader del arquetipo,
// y el resto de configuracion del profile (RT formats, Depth formats, etc).
// El shader del profile hara un include del shader del arquetipo,
// y este ultimo definira la BRDF a usar y demas configuracion especifica del material.
// De esta forma separamos la configuracion de render (profiles) de la configuracion de material (archetypes),
//  y permitimos que multiples materiales compartan la misma configuracion de render pero tengan diferentes BRDFs
//   y comportamientos.
// class MaterialLibrary::ArchetypeBuilder
// {
// public:
//     ArchetypeBuilder( MaterialLibrary& l, StringView name )
//         : _lib( l ) {
//         _archDesc.name                = name;
//         _archDesc.topologiesSupported = MaterialTopologyTriangles;
//     }

//     ArchetypeBuilder& addPass( MaterialPassType                                  type,
//                                const STLW::String&                               path,
//                                const STLW::Vector<Graphics::Shader::EntryPoint>& entryPoints,
//                                const STLW::String&                               customIncludePath = "" ) {
//         MaterialArchetypePassConfig p;
//         p.shaderPath        = path;
//         p.passType          = type;
//         p.entryPoints       = entryPoints;
//         p.customIncludePath = customIncludePath;
//         _archDesc.passConfigs.push_back( p );
//         return *this;
//     }

//     void finish() {
//         _lib.registerArchetype( _archDesc );
//     }

// private:
//     MaterialLibrary&      _lib;
//     MaterialArchetypeDesc _archDesc;
// };

} // namespace Core::Render
AXION_NAMESPACE_END