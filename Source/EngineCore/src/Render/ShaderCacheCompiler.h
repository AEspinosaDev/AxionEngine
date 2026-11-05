#pragma once
#ifdef AXION_TOOLS_BUILD

#include <Axion/Core/Render/IShaderCacheCompiler.h>
#include <Axion/Graphics/Subsystems/IShaderRegistry.h>
#include <functional>

AXION_NAMESPACE_BEGIN

namespace Core::Render {

/**
 * @brief Renderer agnostic implementation of the shader cache compiler.
 * It only knows a shader registry that works without a device. What to compile comes from a recipe given by each renderer.
 */
class ShaderCacheCompiler final : public IShaderCacheCompiler
{
public:
    /// @brief Registers in the given registry every shader the renderer needs.
    using ShaderRecipe = std::function<void( Graphics::IShaderRegistry& shaders )>;

    /**
     * @param cacheName Prefix of the cache files (eg: "Rasterizer").
     * @param settings Configuration of the compilation.
     * @param recipe Renderer specific shader registrations.
     */
    ShaderCacheCompiler( StringView cacheName, const Settings& settings, ShaderRecipe recipe );

    bool compileAndExport() override;

private:
    String64     _cacheName;
    Settings     _settings;
    ShaderRecipe _recipe;
};

} // namespace Core::Render

AXION_NAMESPACE_END

#endif
