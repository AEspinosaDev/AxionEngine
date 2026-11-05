/**
 * @file main.cpp
 * @brief Compiles every shader of the engine renderers into shader caches that can be injected at startup
 * (RendererUseShaderCache). Needs no window, device or GPU. DXIL is compiled with the DXC of the Windows SDK.
 */
#include <Axion/Common/Containers/String.h>
#include <Axion/Common/Logging.h>
#include <Axion/Core/Render/IShaderCacheCompiler.h>
#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

USING_AXION_NAMESPACE

namespace {

constexpr int EXIT_OK            = 0;
constexpr int EXIT_FAILED        = 1;
constexpr int EXIT_BAD_ARGUMENTS = 2;

/// @brief A native format the tool can cook.
struct Platform {
    std::string_view                name;
    Core::Render::ShaderCacheTarget target;
};

void printUsage() {
    fmt::println( "Usage: ShaderCacheBuilder --platform=<list> --out=<folder> [options]" );
    fmt::println( "" );
    fmt::println( "  --platform=<list>        Comma separated native formats: dxil, spirv, glsl" );
    fmt::println( "  --out=<folder>           Output folder. Created if it doesn't exist" );
    fmt::println( "  --profile-dxil=<name>    DXIL profile. Default: compiler default" );
    fmt::println( "  --profile-spirv=<name>   SPIR-V profile. Default: compiler default" );
    fmt::println( "  --profile-glsl=<name>    GLSL profile. Default: compiler default" );
    fmt::println( "  --verbose                Log every shader" );
    fmt::println( "  --help                   Show this help" );
}

/// @brief Returns the value of an argument of the form --key=value.
std::optional<std::string_view> argumentValue( std::string_view argument, std::string_view key ) {
    if ( argument.size() <= key.size() || !argument.starts_with( key ) || argument[key.size()] != '=' )
        return std::nullopt;
    return argument.substr( key.size() + 1 );
}

} // namespace

int main( int argc, char* argv[] ) {

    std::vector<Platform> platforms;
    std::string           outputDir, dxilProfile, spirvProfile, glslProfile;
    bool                  verbose = false;

    for ( int i = 1; i < argc; ++i )
    {
        const std::string_view argument = argv[i];

        if ( argument == "--help" )
        {
            printUsage();
            return EXIT_OK;
        }
        if ( argument == "--verbose" )
        {
            verbose = true;
        } else if ( auto list = argumentValue( argument, "--platform" ) )
        {
            size_t start = 0;
            while ( start <= list->size() )
            {
                const size_t           end  = std::min( list->find( ',', start ), list->size() );
                const std::string_view name = list->substr( start, end - start );

                if ( name == "dxil" )
                    platforms.push_back( { name, Core::Render::ShaderCacheTarget::DXIL } );
                else if ( name == "spirv" )
                    platforms.push_back( { name, Core::Render::ShaderCacheTarget::SPIRV } );
                else if ( name == "glsl" )
                    platforms.push_back( { name, Core::Render::ShaderCacheTarget::GLSL } );
                else
                {
                    fmt::println( stderr, "Unknown platform '{}'", name );
                    return EXIT_BAD_ARGUMENTS;
                }
                start = end + 1;
            }
        } else if ( auto out = argumentValue( argument, "--out" ) )
        {
            outputDir = *out;
        } else if ( auto dxil = argumentValue( argument, "--profile-dxil" ) )
        {
            dxilProfile = *dxil;
        } else if ( auto spirv = argumentValue( argument, "--profile-spirv" ) )
        {
            spirvProfile = *spirv;
        } else if ( auto glsl = argumentValue( argument, "--profile-glsl" ) )
        {
            glslProfile = *glsl;
        } else
        {
            fmt::println( stderr, "Unknown argument '{}'", argument );
            printUsage();
            return EXIT_BAD_ARGUMENTS;
        }
    }

    if ( platforms.empty() || outputDir.empty() )
    {
        printUsage();
        return EXIT_BAD_ARGUMENTS;
    }

    Logger::init( verbose ? Logger::Level::Info : Logger::Level::Warn );

    bool success = true;
    for ( Platform& platform : platforms )
    {
        Core::Render::IShaderCacheCompiler::Settings settings {};
        settings.target    = platform.target;
        settings.outputDir = outputDir;
        settings.profile   = platform.target == Core::Render::ShaderCacheTarget::DXIL    ? StringView( dxilProfile )
                             : platform.target == Core::Render::ShaderCacheTarget::SPIRV ? StringView( spirvProfile )
                                                                                         : StringView( glslProfile );

        fmt::println( "[{}] Compiling...", platform.name );

        auto       compiler = Core::Render::createRasterizerShaderCacheCompiler( settings );
        const bool compiled = compiler->compileAndExport();

        fmt::println( "[{}] {}", platform.name, compiled ? "OK" : "FAILED" );
        success = success && compiled;
    }

    Logger::shutdown();
    return success ? EXIT_OK : EXIT_FAILED;
}
