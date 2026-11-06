
#pragma once
#include "Axion/Graphics/RHI/IPipeline.h"
#include "RendererSubsystem.h"
#include "ShaderCache.h"
#ifndef AXION_DISTRIBUTION
#include "ShaderCompiler.h"
#endif

AXION_NAMESPACE_BEGIN

namespace Graphics {

class ShaderRegistry final : public IShaderRegistry, public RendererSubsystem
{
public:
    explicit ShaderRegistry();
    ~ShaderRegistry() override;

    void initialize( const SubsystemInitContext& ctx ) override;

#ifdef AXION_TOOLS_BUILD
    /// @brief Initializes the registry without a device: every registered shader targets the given format and profile.
    void initializeOffline( const OfflineShaderRegistrySettings& settings );

    bool exportShaderCache( StringView directory, StringView cacheName ) override;
#endif

    Builder shader( StringView name ) override { return Builder( *this, name ); }

    const ShaderBundle&         getBundle( ShaderHandle handle ) const override;
    std::optional<ShaderHandle> findShader( StringView name ) const override;
    const ShaderBundle&         compileShader( ShaderHandle handle ) override;
    const ShaderBundle&         compileShader( StringView name ) override;
    void                        compileAllShaders( u32 threadCount = 1 ) override;
    u32                         size() const override { return (u32)_shaders.size(); };
    bool                        loadShaderCache( StringView directory, StringView cacheName ) override;

private:
    ShaderHandle registerShader( const ShaderDesc& desc ) override;

    enum class ShaderState : uint8_t
    {
        Uncompiled,
        Compiling,
        Ready,
        Failed
    };
    struct ShaderRecord {
        ShaderBundle bundle;
        ShaderDesc   desc;
        ShaderState  state = ShaderState::Uncompiled;
        bool         alive = false;
    };

#ifndef AXION_DISTRIBUTION
    ShaderCompiler           _compiler;
    ShaderCompiler::Settings _compilerSettings; ///< The compiler only starts when a shader really has to be compiled
#endif
    ShaderCache          _cache;                       ///< Precompiled shaders. compileShader() looks here first once loaded
    Shader::NativeFormat _targetFormat      = Shader::DXIL; ///< Native format of the active graphics API
    bool                 _bindlessSupported = false;   ///< Device supports pure bindless
#ifdef AXION_TOOLS_BUILD
    bool _offline = false; ///< No device: shaders are forced to _targetFormat
#endif

    STLW::Vector<ShaderRecord>                 _shaders;
    STLW::UnorderedMap<String64, ShaderHandle> _nameToHandle;
};

} // namespace Graphics
AXION_NAMESPACE_END