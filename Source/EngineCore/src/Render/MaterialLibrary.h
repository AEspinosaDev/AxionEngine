#pragma once
#include <Axion/Common/Containers/STLWrapper/Array.h>
#include <Axion/Common/Containers/STLWrapper/Lists.h>
#include <Axion/Common/Containers/STLWrapper/Maps.h>
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
    /**
     * @brief Initialization descriptor for the MaterialLibrary.
     */
    struct Description {
        Graphics::API                    gfxApi;       ///< The graphics API currently in use (e.g., DirectX 12, Vulkan).
        SmallVector<MaterialPassProfile> passProfiles; ///< Configuration profiles for all render passes.
    };
    virtual ~IMaterialLibrary() = default;

    virtual u32 updateArchetypeState( u32 archetypeID, const Graphics::RenderState& state ) = 0;

    virtual const MaterialPassProfile& getPassProfile( u32 passSlot ) const                 = 0;
    virtual Graphics::PipelineHandle   getPipelineHandle( u32 bundleID, u32 passSlot ) const = 0;

    virtual u32  getArchetypeID( StringView name ) const = 0;
    virtual u64  getArchetypesCount() const              = 0;
    virtual u64  getPipelineBundleCount() const          = 0;
    virtual bool isInitialized() const                   = 0;

};

using MaterialLibraryDesc = IMaterialLibrary::Description;

/**
 * @brief Core system responsible for managing material archetypes, pass profiles, and their associated pipeline states.
 * * This library acts as a centralized lazy-evaluation cache for Pipeline State Objects (PSOs). It handles the compilation
 * and retrieval of pipeline bundles based on archetype data and dynamic render states.
 * * @tparam PassCount The total number of render passes supported by the renderer.
 */
template <u32 PassCount>
class MaterialLibrary : public IMaterialLibrary
{

public:
    /**
     * @brief Initializes the material library with the provided description.
     * @param desc The configuration descriptor containing API info and pass profiles.
     */
    void initialize( const MaterialLibraryDesc& desc );

    /**
     * @brief Registers a new material archetype into the library.
     * @param name The unique name of the material archetype.
     * @param shaderModule The base shader module file or name.
     * @param shaderSpecializationType The entry point or specialization type in the shader.
     */
    void registerArchetype( StringView name, StringView shaderModule, StringView shaderSpecializationType );

    /**
     * @brief Queues a material state update to generate or fetch a pipeline bundle.
     * @param archetypeID The internal ID of the archetype being used.
     * @param state The dynamic render state requested by the material instance.
     * @return u64 The 64-bit deterministic hash representing the pipeline bundle.
     */
    u32 updateArchetypeState( u32 archetypeID, const Graphics::RenderState& state ) override;

    /**
     * @brief Registers and compiles all shaders for all known archetypes and passes.
     * @param shaders The global shader registry.
     */
    void registerShaders( Graphics::IShaderRegistry& shaders );

    /**
     * @brief Processes all pending archetype state updates and compiles missing PSOs.
     * * This function iterates through the queued states, checks the pipeline cache,
     * and synchronously builds new pipeline bundles if they do not exist.
     * * @param pipelines The global pipeline registry used to create new PSOs.
     */
    void updatePipelines( Graphics::IPipelineRegistry& pipelines );

    
    const MaterialPassProfile& getPassProfile( u32 passSlot ) const override;

    /**
     * @brief Retrieves the array of configured pass profiles.
     * @return const FixedArray<MaterialPassProfile, PassCount>&
     */
    const FixedArray<MaterialPassProfile, PassCount>& getPassProfiles() const { return _passProfiles; }

    /**
     * @brief Retrieves the raw array of registered material archetypes.
     * @return const STLW::Vector<MaterialArchetype<PassCount>>&
     */
    AXION_FORCE_INLINE const STLW::Vector<MaterialArchetype<PassCount>>& getArchetypesRaw() const { return _archetypes; }

    /**
     * @brief Retrieves the internal ID of a registered archetype by name.
     * @param name The name of the archetype.
     * @return u32 The ID of the archetype, or 0 if not found (fallback).
     */
    u32 getArchetypeID( StringView name ) const override;

    /**
     * @brief Retrieves the total number of registered archetypes.
     */
    AXION_FORCE_INLINE u64 getArchetypesCount() const override { return _archetypes.size(); }

    /**
     * @brief Retrieves the total number of cached pipeline bundles.
     */
    AXION_FORCE_INLINE u64 getPipelineBundleCount() const override { return _pipelineCache.size(); }

    /**
     * @brief Checks if the material library has been successfully initialized.
     */
    AXION_FORCE_INLINE bool isInitialized() const override { return _initialized; }

    /**
     * @brief Fetches a specific pipeline handle from the cache without throwing exceptions.
     * @param bundleID The ID of the pipeline bundle.
     * @param passSlot The specific pass slot index to retrieve the PSO for.
     * @return Graphics::PipelineHandle The pipeline handle, or Invalid if not found.
     */
    Graphics::PipelineHandle getPipelineHandle( u32 bundleID, u32 passSlot ) const override;

private:
    STLW::UnorderedMap<String64, u32>          _archetypeLookup; ///< Fast name-to-ID mapping for archetypes.
    STLW::Vector<MaterialArchetype<PassCount>> _archetypes;      ///< Contiguous array of registered archetypes.

    FixedArray<MaterialPassProfile, PassCount> _passProfiles; ///< Array containing the static profile definitions for each pass.

    /**
     * @brief Internal structure used to hash and identify unique pipeline requests.
     */
    struct ArchetypeStateEntry {
        u32                   archetypeID;
        Graphics::RenderState state;

        /**
         * @brief Generates a 64-bit deterministic hash for the archetype and state combination.
         */
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

    STLW::Queue<ArchetypeStateEntry> _pendingArchetypeStates; ///< Queue of material states waiting for pipeline creation.
    // TODO: Change this map for another faster structure in the future
    STLW::UnorderedMap<u64, u32>            _pipelineLookup; ///< Global cache mapping hashes to fully built pipeline bundles lookup.
    STLW::Vector<PipelineBundle<PassCount>> _pipelineCache;  ///< Global PSO cache.

    Graphics::API _api; ///< Active graphics API backend.
    bool          _initialized = false;
};

} // namespace Core::Render
AXION_NAMESPACE_END

#include "MaterialLibrary.inl"