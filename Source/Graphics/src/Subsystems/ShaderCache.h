#pragma once
#include <Axion/Common/Containers/STLWrapper/Maps.h>
#include <Axion/Common/Containers/STLWrapper/Vector.h>
#include <Axion/Graphics/ShaderCommon.h>
#include <string>

AXION_NAMESPACE_BEGIN

namespace Graphics {

/**
 * @brief Precompiled shaders of a renderer. Stored in two files per native format:
 * - <cacheName>.<format>.bin: bytecode of every stage, one after the other.
 * - <cacheName>.<format>.json: manifest with the byte range of each stage inside the binary.
 *
 * No reflection data is stored: cached shaders must be used with an explicit pipeline layout.
 * A shader is identified by its name plus a hash of its description, which detects a cache built from other settings.
 * Shader sources are not hashed: they aren't available at runtime, so a cache must be rebuilt when they change.
 */
class ShaderCache
{
public:
    /**
     * @brief Reads the manifest and the binary of a cache.
     * @param directory Folder that holds the cache files.
     * @param cacheName Prefix of the cache files.
     * @param expectedFormat Native format the cache must contain.
     * @param outError Reason of the failure.
     * @return True if the cache is ready to resolve shaders.
     */
    bool load( StringView directory, StringView cacheName, Shader::NativeFormat expectedFormat, std::string& outError );

    /// @brief Returns true if a cache has been successfully loaded.
    [[nodiscard]] bool isLoaded() const { return _loaded; }

    /**
     * @brief Fills a bundle with the cached bytecode of a shader.
     * @param desc Description of the requested shader.
     * @param outBundle Bundle to fill. Left untouched on failure.
     * @param outError Reason of the failure.
     * @return True if the shader is in the cache and matches its description.
     */
    bool resolve( const ShaderDesc& desc, ShaderBundle& outBundle, std::string& outError ) const;

#ifdef AXION_TOOLS_BUILD
    /// @brief Shader to be written to a cache.
    struct ExportEntry {
        const ShaderDesc*   desc   = nullptr;
        const ShaderBundle* bundle = nullptr;
    };

    /**
     * @brief Writes a cache. Shaders registered more than once with the same description are written once.
     * @param directory Output folder. Created if it doesn't exist.
     * @param cacheName Prefix of the cache files.
     * @param format Native format of the bytecode.
     * @param profile Profile the shaders were compiled with.
     * @param entries Shaders to write.
     * @param outError Reason of the failure.
     * @return True if both files were written.
     */
    static bool write( StringView                       directory,
                       StringView                       cacheName,
                       Shader::NativeFormat             format,
                       StringView                       profile,
                       const STLW::Vector<ExportEntry>& entries,
                       std::string&                     outError );
#endif

private:
    /// @brief Byte range of a stage inside the binary.
    struct StageRange {
        ShaderType type = ShaderType::None;
        String32   entryPoint;
        u64        offset = 0;
        u64        size   = 0;
    };
    struct Entry {
        u64                      descHash = 0;
        STLW::Vector<StageRange> stages;
    };

    STLW::UnorderedMap<String64, Entry> _entries;
    STLW::Vector<byte>                  _blob;
    bool                                _loaded = false;
};

} // namespace Graphics

AXION_NAMESPACE_END
