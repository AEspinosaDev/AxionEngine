#pragma once
#include "RendererSubsystem.h"

AXION_NAMESPACE_BEGIN
namespace Graphics {

class GPUResourcePool final : public IGPUResourcePool, public RendererSubsystem
{
public:
    explicit GPUResourcePool();
    ~GPUResourcePool() override;

    void initialize( const SubsystemInitContext& ctx ) override;

    BufferBuilder  buffer( StringView name ) override { return BufferBuilder( *this, name ); }
    TextureBuilder texture( StringView name ) override { return TextureBuilder( *this, name ); }
    SamplerBuilder sampler( StringView name ) override { return SamplerBuilder( *this, name ); }
    AccelBuilder   accel( StringView name ) override { return AccelBuilder( *this, name ); }

    RHI::IBuffer*                getBuffer( BufferHandle handle ) override;
    std::optional<BufferHandle>  findBuffer( StringView name ) const override;
    void                         destroyBuffer( BufferHandle handle ) override;
    RHI::ITexture*               getTexture( TextureHandle handle ) override;
    std::optional<TextureHandle> findTexture( StringView name ) const override;
    void                         destroyTexture( TextureHandle handle ) override;
    RHI::ISampler*               getSampler( SamplerHandle handle ) override;
    std::optional<SamplerHandle> findSampler( StringView name ) const override;
    void                         destroySampler( SamplerHandle handle ) override;
    RHI::IAccel*                 getAccel( AccelHandle handle ) override;
    std::optional<AccelHandle>   findAccel( StringView name ) const override;
    void                         destroyAccel( AccelHandle handle ) override;

    // Special functions for renderer interop
    TextureHandle registerExternalTexture( RHI::TextureOwnerPtr&& ptr, StringView name );
    BufferHandle  registerExternalBuffer( RHI::BufferOwnerPtr&& ptr, StringView name );
    SamplerHandle registerExternalSampler( RHI::SamplerOwnerPtr&& ptr, StringView name );
    AccelHandle   registerExternalAccel( RHI::AccelOwnerPtr&& ptr, StringView name );

    void clear() override;
    u32 bufferCount() const override { return (u32)_buffers.size(); };
    u32 textureCount() const override { return (u32)_textures.size(); };
    u32 samplerCount() const override { return (u32)_samplers.size(); };
    u32 accelCount() const override { return (u32)_accels.size(); };

private:
    BufferHandle  createBuffer( const RHI::BufferDesc& desc, const void* initialData, bool allowLookup = true ) override;
    TextureHandle createTexture( const RHI::TextureDesc& desc, const void* initialData, bool allowLookup = true ) override;
    SamplerHandle createSampler( const RHI::SamplerDesc& desc, bool allowLookup = true ) override;
    AccelHandle   createAccel( const RHI::AccelDesc& desc, bool instantBuild = false, bool allowLookup = true ) override;

private:
    template <typename T>
    struct ResourceRecord {
        T        ptr = nullptr;
        String64 name;
        u16   generation = 0;
        bool     alive      = false;
    };

    // Buffers
    STLW::Vector<ResourceRecord<RHI::BufferOwnerPtr>> _buffers;
    STLW::UnorderedMap<String64, BufferHandle>        _buffNameToHandle;
    // Textures
    STLW::Vector<ResourceRecord<RHI::TextureOwnerPtr>> _textures;
    STLW::UnorderedMap<String64, TextureHandle>        _texNameToHandle;
    // Samplers
    STLW::Vector<ResourceRecord<RHI::SamplerOwnerPtr>> _samplers;
    STLW::UnorderedMap<String64, SamplerHandle>        _samplerNameToHandle;
    // Acceleration Structures
    STLW::Vector<ResourceRecord<RHI::AccelOwnerPtr>> _accels;
    STLW::UnorderedMap<String64, AccelHandle>        _accelNameToHandle;
};

} // namespace Graphics
AXION_NAMESPACE_END
