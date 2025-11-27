#include "ShaderRegistry.hpp"

AXION_NAMESPACE_BEGIN

namespace Graphics {

static const ShaderBundle INVALID_BUNDLE = {};

ShaderRegistry::ShaderRegistry() {
    AXION_LOG_INFO( Logger::Module::GFX, "Shader Registry Subsystem Initialized Succesfully" );
    _compiler.begin();
}

ShaderRegistry::~ShaderRegistry() {
    AXION_LOG_INFO( Logger::Module::GFX, "Destroying Renderer's Shader Registry" );
    _compiler.end();
}

ShaderHandle ShaderRegistry::registerShader( const ShaderDesc& desc ) {
    std::scoped_lock lock( _mutex );

    uint id = UINT32_MAX;
    for ( uint i = 0; i < _shaders.size(); ++i )
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
        id = (uint)_shaders.size();
        _shaders.push_back( { {}, desc, /*std::move( layout ),*/ ShaderState::Uncompiled, true } );
    }
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

std::optional<ShaderHandle> ShaderRegistry::findShader( const std::string& name ) const {
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

    AXION_LOG_INFO( Logger::Module::GFX, "Compiling Shader [{}] with path: {}", record.desc.name, record.desc.path );

    if ( _compiler.compileFile( record.desc, record.bundle ) )
    {
        record.state = ShaderState::Ready;
    } else
    {
        record.state = ShaderState::Failed;
        AXION_LOG_ERROR( Logger::Module::GFX, "Failed to compile shader  [{}]", record.desc.name );
        // Aquí podrías cargar un bytecode de "Error Shader" (rosa chillón) por defecto
    }

    return record.bundle;
}

const ShaderBundle& ShaderRegistry::compileShader( const std::string& name ) {
    auto handleOpt = findShader( name );

    if ( !handleOpt.has_value() )
    {
        AXION_LOG_ERROR( Logger::Module::GFX, "Cannot compile shader, name not found: {}", name );
        return INVALID_BUNDLE;
    }
    return compileShader( *handleOpt );
}

void ShaderRegistry::compileAllShaders( bool async ) {
    if ( async )
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

} // namespace Graphics
AXION_NAMESPACE_END