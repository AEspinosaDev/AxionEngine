
#pragma once
#include "Axion/Graphics/RHI/IPipeline.h"
#include "ShaderCompiler.h"
#include "RendererSubsystem.h"

AXION_NAMESPACE_BEGIN

namespace Graphics {

class ShaderRegistry final : public IShaderRegistry, public RendererSubsystem
{
public:
    explicit ShaderRegistry();
    ~ShaderRegistry() override;

    void initialize( const SubsystemInitContext& ctx ) override;

    Builder shader( const std::string& name ) override { return Builder( *this, name ); }

    const ShaderBundle&         getBundle( ShaderHandle handle ) const override;
    std::optional<ShaderHandle> findShader( const std::string& name ) const override;
    const ShaderBundle&         compileShader( ShaderHandle handle ) override;
    const ShaderBundle&         compileShader( const std::string& name ) override;
    void                        compileAllShaders( uint threadCount = 1 ) override;
    uint                        size() const override { return (uint)_shaders.size(); };

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

    ShaderCompiler _compiler;

    std::vector<ShaderRecord>                     _shaders;
    std::unordered_map<std::string, ShaderHandle> _nameToHandle;
};

} // namespace Graphics
AXION_NAMESPACE_END