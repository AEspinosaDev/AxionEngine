#include "MaterialSystem.h"

AXION_NAMESPACE_BEGIN

namespace Core::Render {


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

        defaultDepthPass.shaderPath  = AXION_SHADER_DIR "/Slang/VisBuffer/VisGen.slang";
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