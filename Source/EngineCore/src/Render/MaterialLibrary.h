#pragma once
#include <Axion/Common/Containers/STLWrapper/Array.h>
#include <Axion/Common/Containers/STLWrapper/Maps.h>
#include <Axion/Common/Containers/STLWrapper/Lists.h>
#include <Axion/Core/Render/Common.h>
#include <Axion/Graphics/Subsystems/IPipelineRegistry.h>
#include <Axion/Graphics/Subsystems/IShaderRegistry.h>

AXION_NAMESPACE_BEGIN

namespace Core::Render {

// /**
//  * Struct that defines a material archetype.
//  */
template <u32 PassCount>
struct MaterialArchetype {

    const String64 name;
    const String64 shaderModule;
    const String64 shaderSpecializationType;

    // Compiled shader handles for each pass type combination, indexed by [pass]
    using ShaderArray = FixedArray<Graphics::ShaderHandle, PassCount>;
    ShaderArray shaderHandles;

    MaterialArchetype( StringView name, StringView shaderModule, StringView shaderSpecializationType )
        : name( name )
        , shaderModule( shaderModule )
        , shaderSpecializationType( shaderSpecializationType ) {}
};

enum class StateOverrideFlags : u32
{
    None       = 0,
    Topology   = 1 << 0,
    FillMode   = 1 << 1,
    CullMode   = 1 << 2,
    BlendOp    = 1 << 3,
    DepthOp    = 1 << 4,
    DepthWrite = 1 << 5,
    DepthTest  = 1 << 6,
    All        = 0xFFFFFFFF
};
AXION_ENUM_CLASS_FLAG_OPERATORS( StateOverrideFlags );

/**
 * Description of a Material Pass.
 * Defines the render target formats, depth format, and pipeline layout to be used for a specific pass type.
 * This allows for flexible configuration of different passes (e.g., opaque, transparent, shadow) without hardcoding these details in the material archetypes.
 */
struct MaterialPassProfile {
    String64                                   name;         // Eg: "Opaque", "Transparent", "Shadow", used for shader naming and logging
    u32                                        slot;         // Slot index for this pass type, used for indexing into archetype shader handles and pipeline handles
    Graphics::PipelineLayoutHandle             layoutHandle; // Pass profile stores the global layout used by all Materials in this Pass, which defines the expected resources and their bindings.
    Graphics::RHI::PipelineBindPoint           bindPointType = Graphics::RHI::PipelineBindPoint::Graphic;
    STLW::String                               shaderPath; // Pass profile stores the shader used by all Materials in this Pass
    STLW::String                               shaderIncludePath;
    STLW::Vector<Graphics::Shader::EntryPoint> entryPoints;
    bool                                       needsSpecialization = true; // Shader needs especialization with material archetype data (eg: BRDF type, texture count, etc) to be functional

    // Render Target configuration for this pass type
    STLW::Vector<Graphics::Format> renderTargetFormats;
    Graphics::Format               depthTargetFormat = Graphics::Format::D32;

    Graphics::RenderState defaultState;                           // Can be overridden by each material instance, but defines the default render state for this pass type.
    StateOverrideFlags    overrideMask = StateOverrideFlags::All; // Defaults to all, meaning that all render state properties can be overridden by the material instance.
};

template <u32 PassCount>
struct PipelineBundle {
    FixedArray<Graphics::PipelineHandle, PassCount> handles;

    Graphics::PipelineHandle getHandleForPass( u32 passSlot ) const {
        return handles[passSlot];
    }
};

class IMaterialLibrary
{
public:
    virtual ~IMaterialLibrary() = default;

    virtual u32  getArchetypeID( StringView name ) const = 0;
    virtual u64  getArchetypesCount() const              = 0;
    virtual u64  getPipelineBundleCount() const          = 0;
    virtual bool isInitialized() const                   = 0;

    virtual Graphics::PipelineHandle getPipelineHandle( u64 bundleHash, u32 passSlot ) const = 0;
};

template <u32 PassCount>
class MaterialLibrary : public IMaterialLibrary
{
    struct Description {
        Graphics::API                    gfxApi;
        SmallVector<MaterialPassProfile> passProfiles;
    };

public:
    void initialize( const Description& desc );
    void registerArchetype( StringView name, StringView shaderModule, StringView shaderSpecializationType );
    u64  updateArchetypeState( u32 archetypeID, const Graphics::RenderState& state );

    void registerShaders( Graphics::IShaderRegistry& shaders );
    void updatePipelines( Graphics::IPipelineRegistry& pipelines );

    const FixedArray<MaterialPassProfile, PassCount>& getPassProfiles() const { return _passProfiles; }
    AXION_FORCE_INLINE const STLW::Vector<MaterialArchetype<PassCount>>& getArchetypesRaw() const { return _archetypes; }

    u32                      getArchetypeID( StringView name ) const override;
    AXION_FORCE_INLINE u64   getArchetypesCount() const override { return _archetypes.size(); }
    AXION_FORCE_INLINE u64   getPipelineBundleCount() const override { return _pipelineCache.size(); }
    AXION_FORCE_INLINE bool  isInitialized() const override { return _initialized; }
    Graphics::PipelineHandle getPipelineHandle( u64 bundleHash, u32 passSlot ) const override;

private:
    STLW::Vector<MaterialArchetype<PassCount>> _archetypes;
    STLW::UnorderedMap<String64, u32>          _archetypeLookup;
    FixedArray<MaterialPassProfile, PassCount> _passProfiles;

    struct ArchetypeStateEntry {
        u32                   archetypeID;
        Graphics::RenderState state;

        u64 hash() const {
            u64 h = archetypeID;
            h ^= (u64)state.topology << 24;
            h ^= (u64)state.fillMode << 28;
            h ^= (u64)state.cullMode << 32;
            h ^= (u64)state.blendOp << 36;
            h ^= (u64)state.depthOp << 40;
            h ^= (u64)state.depthTest << 44;
            h ^= (u64)state.depthWrite << 45;
            return h;
        }
    };

    STLW::Queue<ArchetypeStateEntry> _pendingArchetypeStates;
    // Change this map for another better structure
    STLW::UnorderedMap<u64, PipelineBundle<PassCount>> _pipelineCache;

    Graphics::API _api;
    bool          _initialized = false;
};

} // namespace Core::Render
AXION_NAMESPACE_END

#include "MaterialLibrary.inl"