#pragma once
#include "MaterialLibrary.h"

AXION_NAMESPACE_BEGIN

namespace Core::Render {

template <u32 PassCount>
MaterialLibrary<PassCount>::ArchetypeBuilder MaterialLibrary<PassCount>::beginMaterial( StringView name ) {
    return MaterialLibrary<PassCount>::ArchetypeBuilder( *this, name );
}
template <u32 PassCount>
void MaterialLibrary<PassCount>::initialize( Graphics::API api ) {
    _api = api;
    _initialized = true;
}

template <u32 PassCount>
void MaterialLibrary<PassCount>::setPassProfile( u32 passSlot, const MaterialPassProfile& profile ) {
    _passProfiles[passSlot] = profile;
}

template <u32 PassCount>
u32 MaterialLibrary<PassCount>::getArchetypeID( StringView name ) const {
    if ( auto it = _archetypeLookup.find( name ); it != _archetypeLookup.end() )
        return it->second;

    // AXION_LOG_WARN( "Material '{}' not found. Fallback to Error Material.", name );
    return 0;
}

template <u32 PassCount>
void MaterialLibrary<PassCount>::registerArchetype( const MaterialArchetypeDesc& desc ) {

    if ( _archetypeLookup.contains( desc.name ) )
        return;

    MaterialArchetype arch( desc );
    _archetypes.push_back( arch );
    _archetypeLookup[desc.name] = (u32)_archetypes.size() - 1;

    AXION_LOG_INFO( Logger::Module::Core, "Registered Material [{}]", desc.name );
}

template <u32 PassCount>
void MaterialLibrary<PassCount>::registerShaders( Graphics::IShaderRegistry& shaders ) {

    for ( auto& arch : _archetypes )
    {
        for ( auto& passProfile : _passProfiles )
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

            arch.shaderHandles[(size_t)passProfile.] = builder.load();
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

                auto builder = pipelines.graphic( pipName ).shader( shaderHandle ).setLayout( _passProfiles[passTypeID].layoutHandle );

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
} // namespace Core::Render
AXION_NAMESPACE_END