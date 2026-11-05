#pragma once
#ifdef AXION_TOOLS_BUILD

#include <Axion/Common/Common.h>
#include <Axion/Common/Containers/STLWrapper/String.h>
#include <Axion/Common/Containers/String.h>
#include <Axion/Common/Memory/Pointers/OwnerPtr.h>

AXION_NAMESPACE_BEGIN

namespace Core::Render {

DEFINE_OWNER_PTR_FOR_TYPE( IShaderCacheCompiler, ShaderCacheCompiler )

/// @brief Native format of a shader cache.
enum class ShaderCacheTarget : byte
{
    DXIL,  ///< DirectX 12
    SPIRV, ///< Vulkan
    GLSL   ///< OpenGL
};

/**
 * @brief Compiles every shader a renderer needs and writes them to a shader cache, without a window, a device or a GPU.
 * The cache is read at runtime with the RendererUseShaderCache flag.
 */
class IShaderCacheCompiler
{
public:
    /// @brief Configuration of a shader cache compilation.
    struct Settings {
        ShaderCacheTarget target = ShaderCacheTarget::DXIL; ///< Native format of the cache.
        STLW::String      outputDir;                        ///< Output folder. Created if it doesn't exist.
        String32          profile;                          ///< Target profile (eg: "sm_6_6"). Empty for the default profile of the target.
    };

    virtual ~IShaderCacheCompiler() = default;

    /**
     * @brief Compiles the shaders and writes the cache files. A cache already in the output folder is kept if any shader fails.
     * @return True if every shader compiled and the cache was written.
     */
    virtual bool compileAndExport() = 0;
};

/// @brief Factory function to create the shader cache compiler of the rasterizer.
ShaderCacheCompilerOwnerPtr createRasterizerShaderCacheCompiler( const IShaderCacheCompiler::Settings& settings );

} // namespace Core::Render

AXION_NAMESPACE_END

#endif
