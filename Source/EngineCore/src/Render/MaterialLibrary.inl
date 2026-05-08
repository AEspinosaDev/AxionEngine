#include "MaterialLibrary.h"

AXION_NAMESPACE_BEGIN
namespace Core::Render {

template <u32 PassCount>
inline void MaterialLibrary<PassCount>::initialize( const Description& desc ) {
    _api = desc.gfxApi;

    // Store pass profiles config
    for ( auto& passProfile : desc.passProfiles )
        _passProfiles[passProfile.slot] = passProfile;

    // Register archetypes
    for ( auto& archetypeDesc : desc.archetypeDescs )
    {
        if ( _archetypeLookup.contains( desc.name ) )
            return;

        MaterialArchetype arch( desc );
        _archetypes.push_back( arch );
        _archetypeLookup[desc.name] = (u32)_archetypes.size() - 1;

        AXION_LOG_INFO( Logger::Module::Core, "Registered Material [{}]", desc.name );
    }

    _initialized = true;
}

template <u32 PassCount>
u32 MaterialLibrary<PassCount>::getArchetypeID( StringView name ) const {
    if ( auto it = _archetypeLookup.find( name ); it != _archetypeLookup.end() )
        return it->second;

    AXION_LOG_WARN( "Material '{}' not found. Fallback to Error Material.", name );
    return 0;
}

template <u32 PassCount>
void MaterialLibrary<PassCount>::registerShaders( Graphics::IShaderRegistry& shaders ) {

    for ( auto& arch : _archetypes )
    {
        for ( const auto& passProfile : _passProfiles )
        {
            StringView shaderName = passProfile.needsSpecialization ? passProfile.name + "_shader_" + arch.desc.name : passProfile.name + "_shader";

            auto builder = shaders.shader( shaderName )
                               .path( passProfile.shaderPath )
                               .entryPoints( passProfile.entryPoints )
                               .include( AXION_SHADER_DIR "/Slang/Common" )
                               .autoReflect( false ); // As defined layouts should be used

            if ( !passProfile.customIncludePath.empty() )
                builder.include( passProfile.customIncludePath );

            if ( passProfile.needsSpecialization )
            {
                builder.addModule( arch.desc.shadeModule );
                builder.addSpecialization( arch.desc.archetypeShaderSpcecializationType );
            }

            _api == Graphics::API::DirectX12 ? builder.asDXIL() : builder.asSPIRV();

            // Builder does not duplicate the shader, if its already registered, so this is safe to call for multiple archetypes sharing the same pass profile
            arch.shaderHandles[passProfile.slot] = builder.load();
        }
    }
}

template <u32 PassCount>
void MaterialLibrary<PassCount>::createPipelines( Graphics::IPipelineRegistry& pipelines ) {

    for ( auto& arch : _archetypes )
    {
        for ( const auto& passProfile : _passProfiles )
        {
            StringView pipName = passProfile.needsSpecialization ? passProfile.name + "_pip_" + arch.desc.name : passProfile.name + "_pip";

            switch ( passProfile.bindPointType )
            {
                case Graphics::RHI::PipelineBindPoint::Graphic: {

                    for ( u32 t = 0; t < (u32)Graphics::PrimitiveTopology::Count; ++t )
                    {
                        auto topoType = (Graphics::PrimitiveTopology)t;

                        if ( !( arch.desc.topologiesSupported & topoType ) )
                            continue;

                        auto& shaderHandle = arch.shaderHandles[passProfile.slot];
                        if ( !shaderHandle.isValid() )
                        {
                            if ( !passProfile.shaderPath.empty() )
                            {
                                AXION_LOG_ERROR( Logger::Module::Core, "Skipping pipeline creation for Material '{}' Pass '{}': Shader compilation failed.", arch.desc.name, passProfile.name );
                            }
                            continue;
                        }

                        auto builder = pipelines.graphic( pipName + getTopologyString( topoType ) ).shader( shaderHandle ).setLayout( passProfile.layoutHandle );

                        // TBD
                        //  Graphics::RHI::RasterizerState rasterizerState;
                        //  rasterizerState.fillMode              = pass.fillMode;
                        //  // rasterizerState.cullMode              = pass.cullMode;
                        //  rasterizerState.cullMode              = pass.cullMode;
                        //  rasterizerState.frontCounterClockwise = false;
                        //  rasterizerState.depthBias             = 0;
                        //  rasterizerState.depthBiasClamp        = 0.0f;
                        //  rasterizerState.slopeScaledDepthBias  = 0.0f;
                        //  rasterizerState.depthClipEnable       = true;
                        //  rasterizerState.multisampleEnable     = false;
                        //  rasterizerState.antialiasedLineEnable = false;
                        //  builder.setRasterizer( rasterizerState );

                        builder.setTopology( topoType );
                        builder.cullMode( passProfile.cullMode );

                        builder.setDepthStencilState( { .depthEnable    = passProfile.depthTest,
                                                        .depthWriteMask = passProfile.depthWrite,
                                                        .depthFunc      = passProfile.depthOp } );

                        for ( int i = 0; i < passProfile.renderTargetFormats.size(); ++i )
                        {
                            builder.addRenderTarget( passProfile.renderTargetFormats[i] );
                        }
                        builder.setDepthFormat( passProfile.depthTargetFormat );

                        arch.pipelines[passProfile.slot][t] = builder.create();
                    }
                }
                /* code */
                break;
                case Graphics::RHI::PipelineBindPoint::Compute: {

                    auto& shaderHandle = arch.shaderHandles[passProfile.slot];
                    if ( !shaderHandle.isValid() )
                    {
                        if ( !passProfile.shaderPath.empty() )
                        {
                            AXION_LOG_ERROR( Logger::Module::Core, "Skipping pipeline creation for Material '{}' Pass '{}': Shader compilation failed.", arch.desc.name, passProfile.name );
                        }
                        continue;
                    }

                    auto builder = pipelines.compute( pipName ).shader( shaderHandle ).setLayout( passProfile.layoutHandle );

                    arch.pipelines[passProfile.slot][0] = builder.create();
                }
                /* code */
                break;
                case Graphics::RHI::PipelineBindPoint::Mesh:
                    /* code */
                    break;
                case Graphics::RHI::PipelineBindPoint::RTX:
                    /* code */
                    break;
            }
        }
    }
}

} // namespace Core::Render
AXION_NAMESPACE_END