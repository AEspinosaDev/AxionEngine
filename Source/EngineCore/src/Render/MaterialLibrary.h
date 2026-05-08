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
    String64 name; // Eg: "Opaque", "Transparent", "Shadow", used for shader naming and logging
    u32      slot; // Slot index for this pass type, used for indexing into archetype shader handles and pipeline handles

    // Pass profile stores the global layout used by all Materials in this Pass, which defines the expected resources and their bindings.
    Graphics::PipelineLayoutHandle layoutHandle;
    // Bind point type shared for all PSOs created with this pass profile, used to determine the type of resources expected in the layout (eg: Compute vs Graphics)
    Graphics::RHI::PipelineBindPoint bindPointType = Graphics::RHI::PipelineBindPoint::Graphic;

    // Pass profile stores the shader used by all Materials in this Pass
    STLW::String                               shaderPath;
    STLW::String                               shaderIncludePath;
    STLW::Vector<Graphics::Shader::EntryPoint> entryPoints;
    // Shader needs especialization with material archetype data (eg: BRDF type, texture count, etc) to be functional
    bool needsSpecialization = true;

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

class IMaterialLibrary
{
public:
    virtual ~IMaterialLibrary() = default;

    virtual u32  getArchetypeID( StringView name ) const = 0;
    virtual u64  getArchetypesCount() const              = 0;
    virtual bool isInitialized() const                   = 0;
};

template <u32 PassCount>
class MaterialLibrary : public IMaterialLibrary
{
    struct Description {
        Graphics::API                    gfxApi;
        SmallVector<MaterialPassProfile> passProfiles;
        Vector<MaterialArchetypeDesc>    archetypeDescs;
    };

public:
    void initialize( const Description& desc );

    // Resources
    void registerShaders( Graphics::IShaderRegistry& shaders );
    void createPipelines( Graphics::IPipelineRegistry& pipelines );

    const FixedArray<MaterialPassProfile, PassCount>& getPassProfiles() const { return _passProfiles; }
    AXION_FORCE_INLINE const STLW::Vector<MaterialArchetype<PassCount>>& getArchetypesRaw() const { return _archetypes; }

    u32                     getArchetypeID( StringView name ) const override;
    AXION_FORCE_INLINE u64  getArchetypesCount() const override { return _archetypes.size(); }
    AXION_FORCE_INLINE bool isInitialized() const override { return _initialized; }

private:
    STLW::Vector<MaterialArchetype<PassCount>> _archetypes;
    STLW::UnorderedMap<String64, u32>          _archetypeLookup;

    FixedArray<MaterialPassProfile, PassCount> _passProfiles;

    Graphics::API _api;
    bool          _initialized = false;
};

} // namespace Core::Render
AXION_NAMESPACE_END

#include "MaterialLibrary.inl"