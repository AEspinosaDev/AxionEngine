#include <Render/ShaderCacheCompiler.h>
#ifdef AXION_TOOLS_BUILD

AXION_NAMESPACE_BEGIN

namespace Core::Render {

static Graphics::Shader::NativeFormat toNativeFormat( ShaderCacheTarget target ) {
    switch ( target )
    {
        case ShaderCacheTarget::SPIRV:
            return Graphics::Shader::SPIR_V;
        case ShaderCacheTarget::GLSL:
            return Graphics::Shader::GLSL;
        case ShaderCacheTarget::DXIL:
        default:
            return Graphics::Shader::DXIL;
    }
}

ShaderCacheCompiler::ShaderCacheCompiler( StringView cacheName, const Settings& settings, ShaderRecipe recipe )
    : _cacheName( cacheName )
    , _settings( settings )
    , _recipe( std::move( recipe ) ) {}

bool ShaderCacheCompiler::compileAndExport() {
    auto shaders = Graphics::createOfflineShaderRegistry( { .format  = toNativeFormat( _settings.target ),
                                                            .profile = _settings.profile } );
    _recipe( *shaders );
    shaders->compileAllShaders();

    return shaders->exportShaderCache( _settings.outputDir, _cacheName );
}

} // namespace Core::Render

AXION_NAMESPACE_END

#endif
