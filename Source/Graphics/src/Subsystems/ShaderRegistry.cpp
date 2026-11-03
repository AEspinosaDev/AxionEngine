#include "ShaderRegistry.h"

AXION_NAMESPACE_BEGIN

namespace Graphics {

static const ShaderBundle INVALID_BUNDLE = {};

ShaderRegistry::ShaderRegistry() : RendererSubsystem() {
}

ShaderRegistry::~ShaderRegistry() {
    AXION_LOG_INFO( Logger::Module::GFX, "Destroying Renderer's Shader Registry" );
    _compiler.end();
}

void ShaderRegistry::initialize( const SubsystemInitContext& ctx ) {
    _bindlessSupported = ctx.device->queryFeatureSupport( RHI::FeatureType::HeapDirectlyIndexed );

    // Default settings target bindless-capable profiles: downgrade the profile of the active API if the device can't do it
    ShaderCompiler::Settings compilerSettings;
    switch ( ctx.device->getGraphicsAPI() )
    {
        case API::DirectX12:
            if ( !_bindlessSupported )
                compilerSettings.dxilProfile = "sm_6_5"; // No ResourceDescriptorHeap (needs SM 6.6)
            break;
        case API::Vulkan:
            // TODO: Vulkan backend not implemented yet. Define the bindless SPIR-V profile (descriptor indexing) and its fallback here
            break;
        default:
            AXION_LOG_ERROR( Logger::Module::GFX, "Shader Registry: unknown graphics API, using default shader profiles" );
            break;
    }
    _compiler.begin( compilerSettings );

    AXION_LOG_INFO( Logger::Module::GFX,
                    "Shader Registry Subsystem Initialized Succesfully | Bindless: {} | Profiles -> DXIL: {} SPIR-V: {} GLSL: {}",
                    _bindlessSupported,
                    compilerSettings.dxilProfile,
                    compilerSettings.spirvProfile,
                    compilerSettings.glslProfile );
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

} // namespace Graphics
AXION_NAMESPACE_END