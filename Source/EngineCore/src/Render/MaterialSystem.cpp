#include "MaterialSystem.h"

AXION_NAMESPACE_BEGIN

namespace Core::Render {
MaterialLibrary::ArchetypeBuilder MaterialLibrary::beginMaterial( const std::string& name ) {
    return MaterialLibrary::ArchetypeBuilder( *this, name );
}

void MaterialLibrary::init( Graphics::API api ) {
    _api = api;

    // beginMaterial( "ErrorMagenta" )
    //     .addPass( MaterialPassType::Opaque, "Assets/Shaders/Error.slang", ... )
    //     .finish();

    _initialized = true;
}

void MaterialLibrary::setPassFormats( MaterialPassType passType, const MaterialPassProfile& profile ) {
    _passProfiles[(size_t)passType] = profile;
}

uint MaterialLibrary::getArchetypeID( const std::string& name ) {
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

    _archetypes.push_back( arch );
    _archetypeLookup[desc.name] = (uint)_archetypes.size() - 1;

    AXION_LOG_INFO( Logger::Module::Core, "Registered Material [{}]", desc.name );
}

void MaterialLibrary::registerShaders( Graphics::IShaderRegistry& shaders ) {

    for ( auto& arch : _archetypes )
    {
        for ( auto& pass : arch.desc.passConfigs )
        {
            std::string shaderName = arch.desc.name + "_shader_" + toString( pass.passType );
            auto        builder    = shaders.shader( shaderName )
                               .path( pass.shaderPath )
                               .entryPoints( pass.entryPoints )
                               .include( AXION_SHADER_DIR "/Slang/Common" );

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

            const auto& profile = _passProfiles[(size_t)pass.passType];

            for ( int t = 0; t < (int)MaterialTopologyType::Count; ++t )
            {
                auto topoType  = (MaterialTopologyType)t;
                auto rhiTopo   = toRHITopology( topoType );
                auto topoFlags = topologyToFlags( topoType );

                if ( !( arch.desc.topologiesSupported & topoFlags ) )
                    continue;

                auto& shaderHandle = arch.shaderHandles[(size_t)pass.passType];
                if ( !shaderHandle.isValid() )
                {
                    if ( !pass.shaderPath.empty() )
                    {
                        AXION_LOG_ERROR( Logger::Module::Core, "Skipping pipeline creation for Material '{}' Pass '{}': Shader compilation failed.", arch.desc.name, toString( pass.passType ) );
                    }
                    continue;
                }

                std::string pipName = arch.desc.name + "_pip_" + toString( topoType ) + "_" + toString( pass.passType );

                auto builder = pipelines.graphic( pipName ).shader( shaderHandle );

                Graphics::RHI::RasterizerState rasterizerState;
                rasterizerState.fillMode              = pass.fillMode;
                rasterizerState.cullMode              = pass.cullMode;
                rasterizerState.frontCounterClockwise = false;
                rasterizerState.depthBias             = 0;
                rasterizerState.depthBiasClamp        = 0.0f;
                rasterizerState.slopeScaledDepthBias  = 0.0f;
                rasterizerState.depthClipEnable       = true;
                rasterizerState.multisampleEnable     = false;
                rasterizerState.antialiasedLineEnable = false;

                builder.setTopology( rhiTopo );
                builder.setRasterizer( rasterizerState );

                for ( int i = 0; i < _passProfiles[t].renderTargetFormats.size(); ++i )
                {
                    builder.addRenderTarget( _passProfiles[(size_t)pass.passType].renderTargetFormats[i] );
                }
                builder.setDepthFormat( _passProfiles[(size_t)pass.passType].depthTargetFormat );

                arch.pipelines[(size_t)pass.passType][t] = builder.create();
            }
        }
    }
}

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
        case MaterialPassType::Voxelization:
            return "Voxel";
        case MaterialPassType::Raytracing:
            return "RT";
        default:
            return "Unknown";
    }
}

std::string MaterialLibrary::toString( MaterialTopologyType type ) {
    switch ( type )
    {
        case MaterialTopologyType::Triangles:
            return "Tri";
        case MaterialTopologyType::Lines:
            return "Line";
        case MaterialTopologyType::Points:
            return "Pnt";
        case MaterialTopologyType::Meshlets:
            return "Mesh";
        default:
            return "Unknown";
    }
}

Graphics::PrimitiveTopology MaterialLibrary::toRHITopology( MaterialTopologyType type ) {
    switch ( type )
    {
        case MaterialTopologyType::Triangles:
            return Graphics::PrimitiveTopology::TriangleList;
        case MaterialTopologyType::Lines:
            return Graphics::PrimitiveTopology::LineList;
        case MaterialTopologyType::Points:
            return Graphics::PrimitiveTopology::PointList;
        case MaterialTopologyType::Meshlets:
            return Graphics::PrimitiveTopology::TriangleList;
        default:
            return Graphics::PrimitiveTopology::TriangleList;
    }
}

MaterialTopologyFlags MaterialLibrary::topologyToFlags( MaterialTopologyType type ) {
    switch ( type )
    {
        case MaterialTopologyType::Triangles:
            return MaterialTopologyTriangles;
        case MaterialTopologyType::Lines:
            return MaterialTopologyLines;
        case MaterialTopologyType::Points:
            return MaterialTopologyPoints;
        case MaterialTopologyType::Meshlets:
            return MaterialTopologyNone;
        default:
            return MaterialTopologyTriangles;
    }
}

} // namespace Core::Render
AXION_NAMESPACE_END