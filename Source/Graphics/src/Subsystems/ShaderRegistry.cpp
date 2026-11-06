#include "ShaderRegistry.h"

AXION_NAMESPACE_BEGIN

namespace Graphics {

static const ShaderBundle INVALID_BUNDLE = {};

/// @brief Handles a shader that can't be taken from the shader cache.
/// Distribution builds have no shader compiler, so there is no fallback: the application exits.
static void reportShaderCacheFailure( StringView message ) {
#ifdef AXION_DISTRIBUTION
    AXION_LOG_FATAL_ERROR( Logger::Module::GFX, "Shader cache: {}", message );
    AXION_FATAL_ERROR_POPUP_WINDOW( "Shader cache: {}", message );
    std::exit( EXIT_FAILURE );
#else
    AXION_LOG_WARN( Logger::Module::GFX, "Shader cache: {}. Falling back to the shader compiler", message );
#endif
}

ShaderRegistry::ShaderRegistry() : RendererSubsystem() {
}

ShaderRegistry::~ShaderRegistry() {
    AXION_LOG_INFO( Logger::Module::GFX, "Destroying Renderer's Shader Registry" );
#ifndef AXION_DISTRIBUTION
    _compiler.end();
#endif
}

void ShaderRegistry::initialize( const SubsystemInitContext& ctx ) {
    _bindlessSupported = ctx.device->queryFeatureSupport( RHI::FeatureType::HeapDirectlyIndexed );

    // Default settings target bindless-capable profiles: downgrade the profile of the active API if the device can't do it
    switch ( ctx.device->getGraphicsAPI() )
    {
        case API::DirectX12:
            _targetFormat = Shader::DXIL;
#ifndef AXION_DISTRIBUTION
            if ( !_bindlessSupported )
                _compilerSettings.dxilProfile = "sm_6_5"; // No ResourceDescriptorHeap (needs SM 6.6)
#endif
            break;
        case API::Vulkan:
            _targetFormat = Shader::SPIR_V;
            // TODO: Vulkan backend not implemented yet. Define the bindless SPIR-V profile (descriptor indexing) and its fallback here
            break;
        default:
            AXION_LOG_ERROR( Logger::Module::GFX, "Shader Registry: unknown graphics API, using default shader profiles" );
            break;
    }

#ifndef AXION_DISTRIBUTION
    AXION_LOG_INFO( Logger::Module::GFX,
                    "Shader Registry Subsystem Initialized Succesfully | Bindless: {} | Profiles -> DXIL: {} SPIR-V: {} GLSL: {}",
                    _bindlessSupported,
                    _compilerSettings.dxilProfile,
                    _compilerSettings.spirvProfile,
                    _compilerSettings.glslProfile );
#endif
}

ShaderHandle ShaderRegistry::registerShader( const ShaderDesc& desc ) {
    std::scoped_lock lock( _mutex );

    u32 id = UINT32_MAX;
    for ( u32 i = 0; i < _shaders.size(); ++i )
    {
        if ( !_shaders[i].alive )
        {
            id          = i;
            _shaders[i] = { {}, desc, /*std::move( layout ),*/ ShaderState::Uncompiled, true };
            break;
        }
    }
    if ( id == UINT32_MAX )
    {
        id = (u32)_shaders.size();
        _shaders.push_back( { {}, desc, /*std::move( layout ),*/ ShaderState::Uncompiled, true } );
    }
#ifdef AXION_TOOLS_BUILD
    if ( _offline )
        _shaders[id].desc.format = _targetFormat;
#endif
    // Map name
    if ( !desc.name.empty() )
        _nameToHandle[desc.name] = { id };

    AXION_LOG_INFO( Logger::Module::GFX, "Registered Shader [{}] with path: {}", desc.name, desc.path );
    return ShaderHandle { id };
}

const ShaderBundle& ShaderRegistry::getBundle( ShaderHandle handle ) const {
    // std::scoped_lock lock( _mutex );
    if ( handle.id >= _shaders.size() )
    {
        AXION_LOG_ERROR( Logger::Module::GFX, "Accessing invalid ShaderHandle ID: {}", handle.id );
        return INVALID_BUNDLE;
    }

    auto& record = _shaders[handle.id];
    if ( !record.alive )
    {
        AXION_LOG_ERROR( Logger::Module::GFX, "Accessing dead ShaderHandle: {}", record.desc.name );
        return INVALID_BUNDLE;
    }

    if ( !record.bundle.isValid() )
    {
        AXION_LOG_WARN( Logger::Module::GFX, "Shader bytecode is empty (not compiled yet?): {}", record.desc.name );
        return INVALID_BUNDLE;
    }

    return record.bundle;
}

std::optional<ShaderHandle> ShaderRegistry::findShader( StringView name ) const {
    // std::scoped_lock lock( _mutex );

    auto it = _nameToHandle.find( name );
    if ( it == _nameToHandle.end() )
        return std::nullopt;
    return it->second;
}

const ShaderBundle& ShaderRegistry::compileShader( ShaderHandle handle ) {
    // std::scoped_lock lock( _mutex );

    if ( handle.id >= _shaders.size() )
    {
        AXION_LOG_ERROR( Logger::Module::GFX, "Accessing invalid ShaderHandle ID: {}", handle.id );
        return INVALID_BUNDLE;
    }

    auto& record = _shaders[handle.id];
    if ( record.state == ShaderState::Ready )
    {
        return record.bundle;
    }

    if ( _cache.isLoaded() )
    {
        std::string cacheError;
        if ( _cache.resolve( record.desc, record.bundle, cacheError ) )
        {
            record.state = ShaderState::Ready;
            AXION_LOG_INFO( Logger::Module::GFX, "Loaded Shader [{}] from the shader cache", record.desc.name );
            return record.bundle;
        }
        reportShaderCacheFailure( cacheError );
    }

#ifdef AXION_DISTRIBUTION
    reportShaderCacheFailure( fmt::format( "shader [{}] needs a shader cache: distribution builds have no shader compiler", record.desc.name ) );
    record.state = ShaderState::Failed;
#else
    AXION_LOG_INFO( Logger::Module::GFX, "Compiling Shader [{}] with path: {}", record.desc.name, record.desc.path );

    _compiler.begin( _compilerSettings );
    if ( _compiler.compileFile( record.desc, record.bundle ) )
    {
        record.state = ShaderState::Ready;
    } else
    {
        record.state = ShaderState::Failed;
        AXION_LOG_ERROR( Logger::Module::GFX, "Failed to compile shader  [{}]", record.desc.name );
        // Aquí podrías cargar un bytecode de "Error Shader" (rosa chillón) por defecto
    }
#endif

    return record.bundle;
}

const ShaderBundle& ShaderRegistry::compileShader( StringView name ) {
    auto handleOpt = findShader( name );

    if ( !handleOpt.has_value() )
    {
        AXION_LOG_ERROR( Logger::Module::GFX, "Cannot compile shader, name not found: {}", name );
        return INVALID_BUNDLE;
    }
    return compileShader( *handleOpt );
}

void ShaderRegistry::compileAllShaders( u32 threadCount ) {
    if ( threadCount > 1 )
    {
        // TO DO . . .
    } else
    {
        AXION_LOG_INFO( Logger::Module::GFX, "Compiling ALL Shaders | Num Threads: {} ", 1 );
        for ( size_t i = 0; i < _shaders.size(); ++i )
        {
            if ( _shaders[i].alive && _shaders[i].state == ShaderState::Uncompiled )
            {
                compileShader( (ShaderHandle)i );
            }
        }
    }
}

bool ShaderRegistry::loadShaderCache( StringView directory, StringView cacheName ) {
    std::scoped_lock lock( _mutex );

    std::string error;
    if ( !_cache.load( directory, cacheName, _targetFormat, error ) )
    {
        reportShaderCacheFailure( error );
        return false;
    }
    AXION_LOG_INFO( Logger::Module::GFX, "Loaded Shader Cache [{}] from: {}", cacheName, directory );
    return true;
}

#ifdef AXION_TOOLS_BUILD

void ShaderRegistry::initializeOffline( const OfflineShaderRegistrySettings& settings ) {
    _offline      = true;
    _targetFormat = settings.format;

    if ( !settings.profile.empty() )
    {
        switch ( settings.format )
        {
            case Shader::DXIL:
                _compilerSettings.dxilProfile = settings.profile;
                break;
            case Shader::SPIR_V:
                _compilerSettings.spirvProfile = settings.profile;
                break;
            case Shader::GLSL:
                _compilerSettings.glslProfile = settings.profile;
                break;
        }
    }
}

bool ShaderRegistry::exportShaderCache( StringView directory, StringView cacheName ) {
    std::scoped_lock lock( _mutex );

    STLW::Vector<ShaderCache::ExportEntry> entries;
    entries.reserve( _shaders.size() );

    bool allCompiled = true;
    for ( const ShaderRecord& record : _shaders )
    {
        if ( !record.alive )
            continue;

        if ( record.state != ShaderState::Ready )
        {
            AXION_LOG_ERROR( Logger::Module::GFX, "Can't export the shader cache: shader [{}] is not compiled", record.desc.name );
            allCompiled = false;
            continue;
        }
        entries.push_back( { &record.desc, &record.bundle } );
    }
    if ( !allCompiled )
        return false;

    const String32 profile = _targetFormat == Shader::DXIL     ? _compilerSettings.dxilProfile
                             : _targetFormat == Shader::SPIR_V ? _compilerSettings.spirvProfile
                                                               : _compilerSettings.glslProfile;

    std::string error;
    if ( !ShaderCache::write( directory, cacheName, _targetFormat, profile, entries, error ) )
    {
        AXION_LOG_ERROR( Logger::Module::GFX, "Can't export the shader cache: {}", error );
        return false;
    }
    return true;
}

ShaderRegistryOwnerPtr createOfflineShaderRegistry( const OfflineShaderRegistrySettings& settings ) {
    auto registry = Memory::makeOwned<ShaderRegistry>();
    registry->initializeOffline( settings );
    return registry;
}

#endif

} // namespace Graphics
AXION_NAMESPACE_END