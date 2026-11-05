#include "ShaderCache.h"
#include <Axion/Common/Logging.h>
#include <algorithm>
#include <charconv>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

AXION_NAMESPACE_BEGIN

namespace Graphics {

namespace {

constexpr u64 MANIFEST_VERSION = 1;

struct ShaderTypeName {
    ShaderType  type;
    const char* name;
};

constexpr ShaderTypeName SHADER_TYPE_NAMES[] = {
    { ShaderType::Compute, "Compute" },
    { ShaderType::Vertex, "Vertex" },
    { ShaderType::Hull, "Hull" },
    { ShaderType::Domain, "Domain" },
    { ShaderType::Geometry, "Geometry" },
    { ShaderType::Pixel, "Pixel" },
    { ShaderType::Amplification, "Amplification" },
    { ShaderType::Mesh, "Mesh" },
    { ShaderType::RayGeneration, "RayGeneration" },
    { ShaderType::AnyHit, "AnyHit" },
    { ShaderType::ClosestHit, "ClosestHit" },
    { ShaderType::Miss, "Miss" },
    { ShaderType::Intersection, "Intersection" },
    { ShaderType::Callable, "Callable" },
};

const char* shaderTypeToString( ShaderType type ) {
    for ( const ShaderTypeName& entry : SHADER_TYPE_NAMES )
    {
        if ( entry.type == type )
            return entry.name;
    }
    return "Unknown";
}

bool shaderTypeFromString( StringView name, ShaderType& outType ) {
    for ( const ShaderTypeName& entry : SHADER_TYPE_NAMES )
    {
        if ( name == entry.name )
        {
            outType = entry.type;
            return true;
        }
    }
    return false;
}

const char* formatToString( Shader::NativeFormat format ) {
    switch ( format )
    {
        case Shader::DXIL:
            return "dxil";
        case Shader::SPIR_V:
            return "spirv";
        case Shader::GLSL:
            return "glsl";
    }
    return "unknown";
}

std::filesystem::path manifestPath( StringView directory, StringView cacheName, Shader::NativeFormat format ) {
    return std::filesystem::path( directory ) / fmt::format( "{}.{}.json", cacheName, formatToString( format ) );
}

/**
 * @brief FNV-1a hash of the settings that define a shader program.
 * Only the file name of the source is hashed: paths differ between the machine that builds the cache and the one that loads it.
 * The native format is left out as well: each format has its own cache files.
 */
u64 hashShaderDesc( const ShaderDesc& desc ) {
    u64 hash = 0xcbf29ce484222325ull;

    const auto mix = [&hash]( StringView text ) {
        for ( const char c : text )
        {
            hash ^= (unsigned char)c;
            hash *= 0x100000001b3ull;
        }
        // Field separator: {"ab", "c"} must not hash like {"a", "bc"}
        hash ^= 0xffull;
        hash *= 0x100000001b3ull;
    };

    mix( std::filesystem::path( StringView( desc.path ) ).filename().string() );

    for ( const Shader::EntryPoint& entryPoint : desc.entryPoints )
    {
        mix( entryPoint.name );
        mix( shaderTypeToString( entryPoint.type ) );
    }
    for ( const String64& module : desc.additionalModules )
        mix( module );
    for ( const String64& type : desc.spececializationTypeNames )
        mix( type );
    if ( desc.preprocessorDefines.has_value() )
    {
        for ( const Shader::PreprocessorDefine& define : *desc.preprocessorDefines )
        {
            mix( define.name );
            mix( define.value );
        }
    }
    return hash;
}

bool readString( const nlohmann::json& object, const char* key, std::string& out ) {
    const auto it = object.find( key );
    if ( it == object.end() || !it->is_string() )
        return false;
    out = it->get_ref<const std::string&>();
    return true;
}

bool readU64( const nlohmann::json& object, const char* key, u64& out ) {
    const auto it = object.find( key );
    if ( it == object.end() || !it->is_number_unsigned() )
        return false;
    out = it->get<u64>();
    return true;
}

#ifdef AXION_TOOLS_BUILD
/**
 * @brief Checks the digest of a DXIL container: magic "DXBC" + 16 bytes of digest.
 * Validated containers carry a real hash. Unvalidated ones carry a constant (all zeros or a "bypass" pattern) and
 * only run on machines with developer mode enabled, so the cache wouldn't work once distributed.
 */
bool isDXILContainerSigned( const STLW::Vector<byte>& code ) {
    constexpr size_t MAGIC_SIZE  = 4;
    constexpr size_t DIGEST_SIZE = 16;

    if ( code.size() < MAGIC_SIZE + DIGEST_SIZE || std::memcmp( code.data(), "DXBC", MAGIC_SIZE ) != 0 )
        return false;

    const byte* digest = code.data() + MAGIC_SIZE;
    return std::any_of( digest, digest + DIGEST_SIZE, [digest]( byte value ) { return value != digest[0]; } );
}
#endif

} // namespace

bool ShaderCache::load( StringView directory, StringView cacheName, Shader::NativeFormat expectedFormat, std::string& outError ) {
    _entries.clear();
    _blob.clear();
    _loaded = false;

    // Manifest
    const std::filesystem::path manifestFile = manifestPath( directory, cacheName, expectedFormat );

    std::ifstream manifestStream( manifestFile, std::ios::binary );
    if ( !manifestStream )
    {
        outError = fmt::format( "can't open manifest '{}'", manifestFile.string() );
        return false;
    }
    const std::string manifestText( ( std::istreambuf_iterator<char>( manifestStream ) ), std::istreambuf_iterator<char>() );

    const nlohmann::json root = nlohmann::json::parse( manifestText, nullptr, /*allow_exceptions*/ false );
    if ( !root.is_object() )
    {
        outError = fmt::format( "manifest '{}' is not valid JSON", manifestFile.string() );
        return false;
    }

    u64 version = 0;
    if ( !readU64( root, "version", version ) || version != MANIFEST_VERSION )
    {
        outError = fmt::format( "manifest '{}' has version {}, expected {}", manifestFile.string(), version, MANIFEST_VERSION );
        return false;
    }

    std::string format;
    if ( !readString( root, "format", format ) || format != formatToString( expectedFormat ) )
    {
        outError = fmt::format( "manifest '{}' has format '{}', expected '{}'", manifestFile.string(), format, formatToString( expectedFormat ) );
        return false;
    }

    // Binary
    const auto  blobIt = root.find( "blob" );
    std::string blobName;
    u64         blobSize = 0;
    if ( blobIt == root.end() || !readString( *blobIt, "file", blobName ) || !readU64( *blobIt, "size", blobSize ) )
    {
        outError = fmt::format( "manifest '{}' doesn't describe its binary", manifestFile.string() );
        return false;
    }

    // The binary always lives next to its manifest
    const std::filesystem::path blobFile = manifestFile.parent_path() / std::filesystem::path( blobName ).filename();

    std::error_code fileError;
    const u64       actualSize = std::filesystem::file_size( blobFile, fileError );
    if ( fileError || actualSize != blobSize )
    {
        outError = fmt::format( "binary '{}' has {} bytes, the manifest expects {}", blobFile.string(), fileError ? 0 : actualSize, blobSize );
        return false;
    }

    _blob.resize( blobSize );
    std::ifstream blobStream( blobFile, std::ios::binary );
    if ( !blobStream || !blobStream.read( reinterpret_cast<char*>( _blob.data() ), (std::streamsize)blobSize ) )
    {
        _blob.clear();
        outError = fmt::format( "can't read binary '{}'", blobFile.string() );
        return false;
    }

    // Shaders
    const auto shadersIt = root.find( "shaders" );
    if ( shadersIt == root.end() || !shadersIt->is_object() )
    {
        outError = fmt::format( "manifest '{}' has no shader list", manifestFile.string() );
        return false;
    }

    for ( const auto& [name, shader] : shadersIt->items() )
    {
        Entry entry;

        std::string hashText;
        const bool  validHash = readString( shader, "hash", hashText ) &&
                                std::from_chars( hashText.data(), hashText.data() + hashText.size(), entry.descHash, 16 ).ec == std::errc();

        const auto stagesIt = shader.find( "stages" );
        if ( !validHash || stagesIt == shader.end() || !stagesIt->is_array() )
        {
            outError = fmt::format( "manifest '{}': shader '{}' is malformed", manifestFile.string(), name );
            return false;
        }

        for ( const nlohmann::json& stage : *stagesIt )
        {
            StageRange  range;
            std::string typeName, entryPoint;

            if ( !readString( stage, "type", typeName ) || !shaderTypeFromString( typeName, range.type ) ||
                 !readString( stage, "entry", entryPoint ) ||
                 !readU64( stage, "offset", range.offset ) || !readU64( stage, "size", range.size ) ||
                 range.offset > blobSize || range.size > blobSize - range.offset )
            {
                outError = fmt::format( "manifest '{}': shader '{}' has an invalid stage", manifestFile.string(), name );
                return false;
            }
            range.entryPoint = StringView( entryPoint );
            entry.stages.push_back( range );
        }

        _entries[String64( StringView( name ) )] = std::move( entry );
    }

    _loaded = true;
    return true;
}

bool ShaderCache::resolve( const ShaderDesc& desc, ShaderBundle& outBundle, std::string& outError ) const {

    const auto it = _entries.find( desc.name );
    if ( it == _entries.end() )
    {
        outError = fmt::format( "shader [{}] is not in the cache", desc.name );
        return false;
    }
    if ( desc.autoReflect )
    {
        outError = fmt::format( "shader [{}] needs reflection, which is not cached", desc.name );
        return false;
    }
    if ( it->second.descHash != hashShaderDesc( desc ) )
    {
        outError = fmt::format( "shader [{}] was cached with other settings, rebuild the cache", desc.name );
        return false;
    }

    outBundle.stageBlobs.reserve( it->second.stages.size() );
    for ( const StageRange& stage : it->second.stages )
    {
        STLW::Vector<byte> code( stage.size );
        std::memcpy( code.data(), _blob.data() + stage.offset, stage.size );

        outBundle.stageBlobs.push_back( { .type           = stage.type,
                                          .code           = std::move( code ),
                                          .entryPointName = stage.entryPoint } );
    }

    // Explicit layout data given by the user, same as the compiler does when reflection is off
    if ( desc.layoutDesc.has_value() )
        outBundle.layoutDesc = *desc.layoutDesc;
    if ( desc.vertexAttributes.has_value() )
        outBundle.vertexAttributes = *desc.vertexAttributes;

    return true;
}

#ifdef AXION_TOOLS_BUILD

bool ShaderCache::write( StringView                       directory,
                         StringView                       cacheName,
                         Shader::NativeFormat             format,
                         StringView                       profile,
                         const STLW::Vector<ExportEntry>& entries,
                         std::string&                     outError ) {

    using OrderedJson = nlohmann::ordered_json;

    OrderedJson        shaders = OrderedJson::object();
    STLW::Vector<byte> blob;

    for ( const ExportEntry& entry : entries )
    {
        const ShaderDesc&   desc     = *entry.desc;
        const ShaderBundle& bundle   = *entry.bundle;
        const std::string   name     = std::string( StringView( desc.name ) );
        const std::string   hashText = fmt::format( "{:016x}", hashShaderDesc( desc ) );

        if ( const auto it = shaders.find( name ); it != shaders.end() )
        {
            // Names are the key of the cache: the same name can't hold two different programs
            if ( it->at( "hash" ).get_ref<const std::string&>() != hashText )
            {
                outError = fmt::format( "shader name [{}] is used by different programs", name );
                return false;
            }
            continue;
        }

        if ( desc.autoReflect )
        {
            outError = fmt::format( "shader [{}] needs reflection, which can't be cached. Give it an explicit layout", name );
            return false;
        }

        OrderedJson stages = OrderedJson::array();
        for ( const ShaderBundle::StageBlob& stage : bundle.stageBlobs )
        {
            if ( format == Shader::DXIL && !isDXILContainerSigned( stage.code ) )
            {
                outError = fmt::format( "shader [{}] stage '{}' has no valid DXIL signature. It would only run with developer mode enabled", name, stage.entryPointName );
                return false;
            }

            OrderedJson range;
            range["type"]   = shaderTypeToString( stage.type );
            range["entry"]  = std::string( StringView( stage.entryPointName ) );
            range["offset"] = (u64)blob.size();
            range["size"]   = (u64)stage.code.size();
            stages.push_back( std::move( range ) );

            blob.insert( blob.end(), stage.code.begin(), stage.code.end() );
        }

        OrderedJson shader;
        if ( !desc.materialPass.empty() )
            shader["pass"] = std::string( StringView( desc.materialPass ) );
        if ( !desc.materialArchetype.empty() )
            shader["archetype"] = std::string( StringView( desc.materialArchetype ) );
        shader["hash"]   = hashText;
        shader["stages"] = std::move( stages );

        shaders[name] = std::move( shader );
    }

    const std::filesystem::path manifestFile = manifestPath( directory, cacheName, format );
    const std::string           blobName     = fmt::format( "{}.{}.bin", cacheName, formatToString( format ) );

    OrderedJson root;
    root["version"]      = MANIFEST_VERSION;
    root["renderer"]     = std::string( cacheName );
    root["format"]       = formatToString( format );
    root["profile"]      = std::string( profile );
    root["blob"]["file"] = blobName;
    root["blob"]["size"] = (u64)blob.size();
    root["shaders"]      = std::move( shaders );

    std::error_code fileError;
    std::filesystem::create_directories( manifestFile.parent_path(), fileError );
    if ( fileError )
    {
        outError = fmt::format( "can't create folder '{}': {}", manifestFile.parent_path().string(), fileError.message() );
        return false;
    }

    const std::filesystem::path blobFile = manifestFile.parent_path() / blobName;

    std::ofstream blobStream( blobFile, std::ios::binary | std::ios::trunc );
    blobStream.write( reinterpret_cast<const char*>( blob.data() ), (std::streamsize)blob.size() );
    if ( !blobStream.good() )
    {
        outError = fmt::format( "can't write binary '{}'", blobFile.string() );
        return false;
    }

    std::ofstream manifestStream( manifestFile, std::ios::binary | std::ios::trunc );
    manifestStream << root.dump( 2 ) << '\n';
    if ( !manifestStream.good() )
    {
        outError = fmt::format( "can't write manifest '{}'", manifestFile.string() );
        return false;
    }

    AXION_LOG_INFO( Logger::Module::GFX, "Exported Shader Cache: {} shaders, {} bytes -> {}", root["shaders"].size(), blob.size(), manifestFile.string() );
    return true;
}

#endif

} // namespace Graphics

AXION_NAMESPACE_END
