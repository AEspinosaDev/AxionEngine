
#pragma once
#include "Axion/Graphics/RHI/IPipeline.h"
#include "RendererSubsystem.h"
#include "ShaderCompiler.h"

AXION_NAMESPACE_BEGIN

namespace Graphics {

class ShaderRegistry final : public IShaderRegistry, public RendererSubsystem
{
public:
    explicit ShaderRegistry();
    ~ShaderRegistry() override;

    void initialize( const SubsystemInitContext& ctx ) override;

    Builder shader( StringView name ) override { return Builder( *this, name ); }

    const ShaderBundle&         getBundle( ShaderHandle handle ) const override;
    std::optional<ShaderHandle> findShader( StringView name ) const override;
    const ShaderBundle&         compileShader( ShaderHandle handle ) override;
    const ShaderBundle&         compileShader( StringView name ) override;
    void                        compileAllShaders( u32 threadCount = 1 ) override;
    u32                         size() const override { return (u32)_shaders.size(); };

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

    STLW::Vector<ShaderRecord>                 _shaders;
    STLW::UnorderedMap<String64, ShaderHandle> _nameToHandle;
};

} // namespace Graphics
AXION_NAMESPACE_END