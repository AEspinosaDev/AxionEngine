#include "MaterialSystem.h"

AXION_NAMESPACE_BEGIN

namespace Core::Render {
MaterialLibrary::ArchetypeBuilder MaterialLibrary::beginMaterial( StringView name ) {
    return MaterialLibrary::ArchetypeBuilder( *this, name );
}

void MaterialLibrary::init( Graphics::API api ) {
    _api                     = api;

    _initialized = true;
}

void MaterialLibrary::setTargetLayout( Graphics::PipelineLayoutHandle globalLayout ) {
    _globalLayoutHandle = globalLayout;
}

void MaterialLibrary::setPassProfile( MaterialPassType passType, const MaterialPassProfile& profile ) {
    _passProfiles[(size_t)passType] = profile;
    _defaultPassSupportFlags |= passTypeToFlags( passType );
}


u32 MaterialLibrary::getArchetypeID( StringView name ) const {
    if ( auto it = _archetypeLookup.find( name ); it != _archetypeLookup.end() )
        return it->second;

    // AXION_LOG_WARN( "Material '{}' not found. Fallback to Error Material.", name );
    return 0;
}

void MaterialLibrary::registerArchetype( const MaterialArchetypeDesc& desc ) {

    if ( _archetypeLookup.contains( desc.name ) )
        return;

    MaterialArchetype arch;
    arch.desc = desc;

    enforceDefaultPasses( arch.desc );

    _archetypes.push_back( arch );
    _archetypeLookup[desc.name] = (u32)_archetypes.size() - 1;

    AXION_LOG_INFO( Logger::Module::Core, "Registered Material [{}]", desc.name );
}

void MaterialLibrary::registerShaders( Graphics::IShaderRegistry& shaders ) {

    for ( auto& arch : _archetypes )
    {
        for ( auto& pass : arch.desc.passConfigs )
        {
            std::string baseName;
            if ( !pass.customPassAlias.empty() )
                baseName = pass.customPassAlias;
            else
                baseName = arch.desc.name;

            std::string shaderName = baseName + "_shader_" + toString( pass.passType );

            auto builder = shaders.shader( shaderName )
                               .path( pass.shaderPath )
                               .entryPoints( pass.entryPoints )
                               .include( AXION_SHADER_DIR "/Slang/Common" )
                               .autoReflect( false );

            if ( !pass.customIncludePath.empty() )
                builder.include( pass.customIncludePath );

            _api == Graphics::API::DirectX12 ? builder.asDXIL() : builder.asSPIRV();

            arch.shaderHandles[(size_t)pass.passType] = builder.load();
        }
    }
}
void MaterialLibrary::createPipelines( Graphics::IPipelineRegistry& pipelines ) {
    for ( auto& arch : _archetypes )
    {
        for ( auto& pass : arch.desc.passConfigs )
        {
            if ( pass.passType == MaterialPassType::Raytracing )
            {
                // TBD
                continue;
            }

            const size_t passTypeID = (size_t)pass.passType;

            const auto& profile = _passProfiles[passTypeID];

            for ( int t = 0; t < (int)TopologyType::Count; ++t )
            {
                auto topoType  = (TopologyType)t;
                auto rhiTopo   = toGFXTopology( topoType );
                auto topoFlags = topologyToFlags( topoType );

                if ( !( arch.desc.topologiesSupported & topoFlags ) )
                    continue;

                auto& shaderHandle = arch.shaderHandles[passTypeID];
                if ( !shaderHandle.isValid() )
                {
                    if ( !pass.shaderPath.empty() )
                    {
                        AXION_LOG_ERROR( Logger::Module::Core, "Skipping pipeline creation for Material '{}' Pass '{}': Shader compilation failed.", arch.desc.name, toString( pass.passType ) );
                    }
                    continue;
                }

                std::string baseName;

                if ( !pass.customPassAlias.empty() )
                    baseName = pass.customPassAlias;
                else
                    baseName = arch.desc.name;

                std::string pipName = baseName + "_pip_" + toString( topoType ) + "_" + toString( pass.passType );

                auto builder = pipelines.graphic( pipName ).shader( shaderHandle ).setLayout( _globalLayoutHandle );

                // Graphics::RHI::RasterizerState rasterizerState;
                // rasterizerState.fillMode              = pass.fillMode;
                // // rasterizerState.cullMode              = pass.cullMode;
                // rasterizerState.cullMode              = pass.cullMode;
                // rasterizerState.frontCounterClockwise = false;
                // rasterizerState.depthBias             = 0;
                // rasterizerState.depthBiasClamp        = 0.0f;
                // rasterizerState.slopeScaledDepthBias  = 0.0f;
                // rasterizerState.depthClipEnable       = true;
                // rasterizerState.multisampleEnable     = false;
                // rasterizerState.antialiasedLineEnable = false;
                // builder.setRasterizer( rasterizerState );

                builder.setTopology( rhiTopo );
                builder.cullMode( pass.cullMode );

                builder.setDepthStencilState( { .depthEnable    = pass.depthTest,
                                                .depthWriteMask = pass.depthWrite,
                                                .depthFunc      = pass.depthOp } );

                for ( int i = 0; i < _passProfiles[passTypeID].renderTargetFormats.size(); ++i )
                {
                    builder.addRenderTarget( _passProfiles[passTypeID].renderTargetFormats[i] );
                }
                builder.setDepthFormat( _passProfiles[passTypeID].depthTargetFormat );

                arch.pipelines[passTypeID][t] = builder.create();
            }
        }
    }
}

#pragma region Default Passes

void MaterialLibrary::enforceDefaultPasses( MaterialArchetypeDesc& desc ) {

    MaterialPassSupportFlags currentArchSupportedPasses = MaterialPassSupportNone;

    size_t opaquePassIndex = -1;

    for ( size_t i = 0; i < desc.passConfigs.size(); ++i )
    {
        const auto& pass = desc.passConfigs[i];

        switch ( pass.passType )
        {
            case MaterialPassType::Opaque:
                currentArchSupportedPasses |= MaterialPassSupportOpaque;
                opaquePassIndex = i;
                break;
            case MaterialPassType::Depth:
                currentArchSupportedPasses |= MaterialPassSupportDepth;
                break;
            case MaterialPassType::Shadow:
                currentArchSupportedPasses |= MaterialPassSupportShadow;
                break;
        }
    }

    // Subscribe DEPTH pass to this archetype
    setDefaultDepthPass( desc, currentArchSupportedPasses, opaquePassIndex );
    // Subscribe SHADOW pass to this archetype
    setDefaultShadowPass( desc, currentArchSupportedPasses );
    // Subscribe VIZ pass to this archetype
    setDefaultVisibilityPass( desc, currentArchSupportedPasses );
}

void MaterialLibrary::setDefaultDepthPass( MaterialArchetypeDesc& desc, MaterialPassSupportFlags currentArchSupportedPasses, size_t opaquePassIndex ) {

    bool globalDepthEnabled = ( _defaultPassSupportFlags & MaterialPassSupportDepth );
    bool isOpaque           = ( currentArchSupportedPasses & MaterialPassSupportOpaque );
    bool hasDepth           = ( currentArchSupportedPasses & MaterialPassSupportDepth );

    if ( globalDepthEnabled && isOpaque && !hasDepth )
    {
        MaterialArchetypePassConfig defaultDepthPass;
        defaultDepthPass.passType        = MaterialPassType::Depth;
        defaultDepthPass.customPassAlias = StringView( "Global_Depth" );

        defaultDepthPass.shaderPath  = AXION_SHADER_DIR "/Slang/Preprocess/DepthOnly.slang";
        defaultDepthPass.entryPoints = { { "vsDepth", Graphics::ShaderType::Vertex } };

        desc.passConfigs.push_back( defaultDepthPass );

        hasDepth = true;
    }

    // If has depth, deactivate depth writes from Opaque pass
    if ( hasDepth && isOpaque && opaquePassIndex != -1 )
    {
        auto& opaquePass = desc.passConfigs[opaquePassIndex];

        opaquePass.depthWrite = false;
        opaquePass.depthOp    = Graphics::CompareOp::LessEqual;
    }
}

void MaterialLibrary::setDefaultShadowPass( MaterialArchetypeDesc& desc, MaterialPassSupportFlags currentArchSupportedPasses ) {
    AXION_UNUSED_PARAMETER( desc );

    bool globalShadowEnabled = ( _defaultPassSupportFlags & MaterialPassSupportShadow );
    bool hasShadow           = ( currentArchSupportedPasses & MaterialPassSupportShadow );

    // TBD

}

void MaterialLibrary::setDefaultVisibilityPass( MaterialArchetypeDesc& desc, MaterialPassSupportFlags currentArchSupportedPasses ) {

    bool globalVisEnabled = ( _defaultPassSupportFlags & MaterialPassSupportVisibility );
    bool isOpaque         = ( currentArchSupportedPasses & MaterialPassSupportOpaque );

    if ( globalVisEnabled )
    {
        MaterialArchetypePassConfig defaultDepthPass;
        defaultDepthPass.passType        = MaterialPassType::Visibility;
        defaultDepthPass.customPassAlias = StringView( "Global_Vis" );

        defaultDepthPass.shaderPath  = AXION_SHADER_DIR "/Slang/Preprocess/Vis.slang";
        defaultDepthPass.entryPoints = { { "vsVis", Graphics::ShaderType::Vertex },
                                         { "psVis", Graphics::ShaderType::Pixel } };

        desc.passConfigs.push_back( defaultDepthPass );
    }
}
#pragma endregion

std::string MaterialLibrary::toString( MaterialPassType type ) {
    switch ( type )
    {
        case MaterialPassType::Opaque:
            return "Opaque";
        case MaterialPassType::Blend:
            return "Blend";
        case MaterialPassType::Geometry:
            return "Geometry";
        case MaterialPassType::Shadow:
            return "Shadow";
        case MaterialPassType::Depth:
            return "Depth";
        case MaterialPassType::Raytracing:
            return "RT";
        default:
            return "Unknown";
    }
}

std::string MaterialLibrary::toString( TopologyType type ) {
    switch ( type )
    {
        case TopologyType::Triangles:
            return "Tri";
        case TopologyType::Lines:
            return "Line";
        case TopologyType::Points:
            return "Pnt";
        case TopologyType::Meshlets:
            return "Mesh";
        default:
            return "Unknown";
    }
}

MaterialTopologyFlags MaterialLibrary::topologyToFlags( TopologyType type ) {
    switch ( type )
    {
        case TopologyType::Triangles:
            return MaterialTopologyTriangles;
        case TopologyType::Lines:
            return MaterialTopologyLines;
        case TopologyType::Points:
            return MaterialTopologyPoints;
        case TopologyType::Meshlets:
            return MaterialTopologyNone;
        default:
            return MaterialTopologyTriangles;
    }
}
MaterialPassSupportFlags MaterialLibrary::passTypeToFlags( MaterialPassType type ) {
    switch ( type )
    {
        case MaterialPassType::Opaque:
            return MaterialPassSupportOpaque;
            
        case MaterialPassType::Blend:
            return MaterialPassSupportTranslucent;
            
        case MaterialPassType::Depth:
            return MaterialPassSupportDepth;
            
        case MaterialPassType::Shadow:
            return MaterialPassSupportShadow;
            
        case MaterialPassType::Raytracing:
            return MaterialPassSupportRaytracing;
            
        case MaterialPassType::Wireframe:
            return MaterialPassSupportWireframe;
            
        case MaterialPassType::Visibility:
            return MaterialPassSupportVisibility;

        // Systemic or compute passes that do not require explicit material permutation flags
        case MaterialPassType::Geometry:
        case MaterialPassType::Composition:
        case MaterialPassType::VisibilityResolve:
            return MaterialPassSupportNone;

        default:
            return MaterialPassSupportNone;
    }
}

} // namespace Core::Render
AXION_NAMESPACE_END