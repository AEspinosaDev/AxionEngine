#include "MaterialLibrary.h"

AXION_NAMESPACE_BEGIN
namespace Core::Render {

template <u32 PassCount>
inline void MaterialLibrary<PassCount>::initialize( const MaterialLibraryDesc& desc ) {
    _api = desc.gfxApi;

    // Store pass profiles config. Profiles must be given in pass slot order
    AXION_LOG_ASSERT( desc.passProfiles.size() == PassCount, Logger::Module::Core, "MaterialLibrary expects {} pass profiles, got {}", PassCount, desc.passProfiles.size() );
    for ( u32 i = 0; i < PassCount && i < desc.passProfiles.size(); ++i )
        _passProfiles[i] = desc.passProfiles[i];

    _initialized = true;

    // _pendingArchetypeStates.reserve( 1024 );
    _pipelineCache.reserve( 1024 );
}

template <u32 PassCount>
void MaterialLibrary<PassCount>::registerArchetype( StringView name, StringView shaderModule, StringView shaderSpecializationType ) {

    if ( _archetypeLookup.contains( name ) )
        return;

    MaterialArchetype<PassCount> arch( name, shaderModule, shaderSpecializationType );
    _archetypes.push_back( arch );
    _archetypeLookup[name] = (u32)_archetypes.size() - 1;

    AXION_LOG_INFO( Logger::Module::Core, "Registered Material [{}]", name );
}

template <u32 PassCount>
inline u32 MaterialLibrary<PassCount>::updateArchetypeState( u32 archetypeID, const Graphics::RenderState& state ) {
    if ( archetypeID >= _archetypes.size() )
        return AXION_INVALID_U32;

    ArchetypeStateEntry entry   = { archetypeID, state };
    u64                 hashKey = entry.hash();

    if ( auto it = _pipelineLookup.find( hashKey ); it != _pipelineLookup.end() )
    {
        AXION_LOG_ASSERT( _bundleKeys[it->second] == entry, Logger::Module::Core, "MaterialLibrary: pipeline bundle hash collision" );
        return it->second;
    }

    u32 newBundleID = (u32)_pipelineCache.size();

    _pipelineLookup[hashKey] = newBundleID;
    _pipelineCache.emplace_back();
    _bundleKeys.push_back( entry );

    _pendingArchetypeStates.push( entry );

    return newBundleID;
}
template <u32 PassCount>
const MaterialPassProfile& MaterialLibrary<PassCount>::getPassProfile( u32 passSlot ) const {
    if ( passSlot >= PassCount )
    {
        AXION_LOG_ERROR( Logger::Module::Core, "Invalid pass slot {} requested. Max supported is {}. Returning first pass profile as fallback.", passSlot, PassCount - 1 );
        return _passProfiles[0];
    }
    return _passProfiles[passSlot];
}

template <u32 PassCount>
u32 MaterialLibrary<PassCount>::getArchetypeID( StringView name ) const {
    if ( auto it = _archetypeLookup.find( name ); it != _archetypeLookup.end() )
        return it->second;

   
    AXION_LOG_ERROR( Logger::Module::Core, "Material archetype '{}' not registered", name );
    return AXION_INVALID_U32;
}
template <u32 PassCount>
inline Graphics::PipelineHandle MaterialLibrary<PassCount>::getPipelineHandle( u32 bundleID, u32 passSlot ) const {

    if ( bundleID < _pipelineCache.size() )
    {
        return _pipelineCache[bundleID].getHandleForPass( passSlot );
    }

    return Graphics::PipelineHandle();
}

template <u32 PassCount>
void MaterialLibrary<PassCount>::registerShaders( Graphics::IShaderRegistry& shaders ) {

    for ( auto& arch : _archetypes )
    {
        for ( u32 passId = 0; passId < PassCount; ++passId )
        {
            const auto& passProfile = _passProfiles[passId];
            String64 shaderName = passProfile.needsSpecialization ? passProfile.name + "_shader_" + arch.name : passProfile.name + "_shader";

            auto builder = shaders.shader( shaderName )
                               .path( passProfile.shaderPath )
                               .entryPoints( passProfile.entryPoints )
                               .include( AXION_SHADER_DIR "/Slang/Common" )
                               .autoReflect( false ); // As defined layouts should be used

            for ( const auto& includePath : passProfile.includePaths )
                builder.include( includePath );

           

            if ( passProfile.needsSpecialization )
            {
                builder.addModule( arch.shaderModule );
                builder.addSpecialization( arch.shaderSpecializationType );
            }

            _api == Graphics::API::DirectX12 ? builder.asDXIL() : builder.asSPIRV();

            // Builder does not duplicate the shader, if its already registered, so this is safe to call for multiple archetypes sharing the same pass profile
            arch.shaderHandles[passId] = builder.load();
        }
    }
}
template <u32 PassCount>
void MaterialLibrary<PassCount>::updatePipelines( Graphics::IPipelineRegistry& pipelines ) {

    while ( !_pendingArchetypeStates.empty() )
    {
        const ArchetypeStateEntry state   = _pendingArchetypeStates.front(); // Copy: the entry is popped below
        u64                       hashKey = state.hash();
        auto&                     arch    = _archetypes[state.archetypeID];

        _pendingArchetypeStates.pop();

        auto lookupIt = _pipelineLookup.find( hashKey );
        if ( lookupIt == _pipelineLookup.end() )
            continue;

        u32 bundleID = lookupIt->second;

        // Is compiled ??
        if ( _pipelineCache[bundleID].built )
            continue;

        // If it doesn't exist, create it.
        else
        {
            PipelineBundle<PassCount> newBundle;

            for ( u32 passId = 0; passId < PassCount; ++passId )
            {
                const MaterialPassProfile& passProfile = _passProfiles[passId];

                // Sanity check
                auto& shaderHandle = arch.shaderHandles[passId];
                if ( !shaderHandle.isValid() )
                {
                    if ( !passProfile.shaderPath.empty() )
                    {
                        AXION_LOG_ERROR( Logger::Module::Core, "Skipping pipeline creation for Material '{}' Pass '{}': Shader compilation failed.", arch.name, passProfile.name );
                    }
                    continue;
                }

                // State-independent pass already built for this archetype: share it
                const bool stateless = passProfile.isStateIndependent();
                if ( stateless && arch.statelessPipelines[passId].isValid() )
                {
                    newBundle.handles[passId] = arch.statelessPipelines[passId];
                    continue;
                }

                // Short name: String64 truncates and the registry would mix up states with the same truncated name
                String64 pipName = stateless ? passProfile.name + "_" + arch.name
                                             : passProfile.name + "_" + arch.name + "_" + fmt::format( "{:016x}", hashKey );

                Graphics::RenderState finalState = passProfile.defaultState;

                if ( (u32)passProfile.overrideMask & (u32)StateOverrideFlags::Topology )
                    finalState.topology = state.state.topology;
                if ( (u32)passProfile.overrideMask & (u32)StateOverrideFlags::FillMode )
                    finalState.fillMode = state.state.fillMode;
                if ( (u32)passProfile.overrideMask & (u32)StateOverrideFlags::CullMode )
                    finalState.cullMode = state.state.cullMode;
                if ( (u32)passProfile.overrideMask & (u32)StateOverrideFlags::BlendOp )
                    finalState.blendOp = state.state.blendOp;
                if ( (u32)passProfile.overrideMask & (u32)StateOverrideFlags::DepthOp )
                    finalState.depthOp = state.state.depthOp;
                if ( (u32)passProfile.overrideMask & (u32)StateOverrideFlags::DepthWrite )
                    finalState.depthWrite = state.state.depthWrite;
                if ( (u32)passProfile.overrideMask & (u32)StateOverrideFlags::DepthTest )
                    finalState.depthTest = state.state.depthTest;

                switch ( passProfile.bindPointType )
                {
                    ///////////////////////
                    // RASTER PSO
                    ///////////////////////
                    case Graphics::RHI::PipelineBindPoint::Graphic: {

                        auto builder = pipelines.graphic( pipName ).shader( shaderHandle ).setLayout( passProfile.layoutHandle );

                        Graphics::RHI::RasterizerState rasterizerState;
                        rasterizerState.fillMode = finalState.fillMode; // Likely to be extended in the future
                        builder.setRasterizer( rasterizerState );
                        builder.setTopology( finalState.topology );
                        builder.cullMode( finalState.cullMode );
                        builder.setDepthStencilState( { .depthEnable    = finalState.depthTest,
                                                        .depthWriteMask = finalState.depthWrite,
                                                        .depthFunc      = finalState.depthOp } );

                        for ( int i = 0; i < passProfile.renderTargetFormats.size(); ++i )
                        {
                            builder.addRenderTarget( passProfile.renderTargetFormats[i] );
                        }
                        builder.setDepthFormat( passProfile.depthTargetFormat );

                        newBundle.handles[passId] = builder.create();
                    }
                    break;

                    ///////////////////////
                    // COMPUTE PSO
                    ///////////////////////
                    case Graphics::RHI::PipelineBindPoint::Compute: {

                        auto builder = pipelines.compute( pipName ).shader( shaderHandle ).setLayout( passProfile.layoutHandle );

                        newBundle.handles[passId] = builder.create();
                    }
                    break;

                    ///////////////////////
                    // MESH PSO
                    ///////////////////////
                    case Graphics::RHI::PipelineBindPoint::Mesh: {

                        auto builder = pipelines.mesh( pipName ).shader( shaderHandle ).setLayout( passProfile.layoutHandle );

                        Graphics::RHI::RasterizerState rasterizerState;
                        rasterizerState.fillMode = finalState.fillMode;
                        builder.setRasterizer( rasterizerState );
                        builder.cullMode( finalState.cullMode );
                        builder.setDepthStencilState( { .depthEnable    = finalState.depthTest,
                                                        .depthWriteMask = finalState.depthWrite,
                                                        .depthFunc      = finalState.depthOp } );

                        for ( int i = 0; i < passProfile.renderTargetFormats.size(); ++i )
                        {
                            builder.addRenderTarget( passProfile.renderTargetFormats[i] );
                        }
                        builder.setDepthFormat( passProfile.depthTargetFormat );

                        newBundle.handles[passId] = builder.create();
                    }
                    break;

                    ///////////////////////
                    // RTX PSO
                    ///////////////////////
                    case Graphics::RHI::PipelineBindPoint::RTX: {
                        ///////////////////////
                        ///////////////////////
                        ///////////////////////
                        // TBD ...
                        ///////////////////////
                        ///////////////////////
                        ///////////////////////
                        newBundle.handles[passId] = {/*INVALID*/};
                    }
                    break;
                }

                if ( stateless )
                    arch.statelessPipelines[passId] = newBundle.handles[passId];
            }

            newBundle.built          = true;
            _pipelineCache[bundleID] = std::move( newBundle );
        }
    }
}

} // namespace Core::Render
AXION_NAMESPACE_END