#pragma once
#include <Axion/Graphics/RHI/Device.h>
#include <Axion/Graphics/Subsystems/GPUResourcePool.h>

AXION_NAMESPACE_BEGIN
namespace Graphics {

DEFINE_UNIQUE_PTR_FOR_TYPE( GPUResourcePool, GPUResourcePool )

class GPUResourcePool final : public IGPUResourcePool
{
public:
    explicit GPUResourcePool( RHI::IDevice* device );
    ~GPUResourcePool() override;

    BufferBuilder  buffer( const std::string& name ) override { return BufferBuilder( *this, name ); }
    TextureBuilder texture( const std::string& name ) override { return TextureBuilder( *this, name ); }
    SamplerBuilder sampler( const std::string& name ) override { return SamplerBuilder( *this, name ); }

    RHI::IBuffer*                getBuffer( BufferHandle handle ) override;
    std::optional<BufferHandle>  findBuffer( const std::string& name ) const override;
    void                         destroyBuffer( BufferHandle handle ) override;
    RHI::ITexture*               getTexture( TextureHandle handle ) override;
    std::optional<TextureHandle> findTexture( const std::string& name ) const override;
    void                         destroyTexture( TextureHandle handle ) override;
    RHI::ISampler*               getSampler( SamplerHandle handle ) override;
    std::optional<SamplerHandle> findSampler( const std::string& name ) const override;
    void                         destroySampler( SamplerHandle handle ) override;

    // Special functions for renderer interop
    TextureHandle registerExternalTexture( RHI::ITexture* ptr, const std::string& name );
    BufferHandle  registerExternalBuffer( RHI::IBuffer* ptr, const std::string& name );
    SamplerHandle registerExternalSampler( RHI::ISampler* ptr, const std::string& name );

    void clear() override;
    uint bufferCount() const override { return (uint)_buffers.size(); };
    uint textureCount() const override { return (uint)_textures.size(); };
    uint samplerCount() const override { return (uint)_samplers.size(); };


private:
    BufferHandle  createBuffer( const RHI::BufferDesc& desc, const void* initialData, bool allowLookup = true ) override;
    TextureHandle createTexture( const RHI::TextureDesc& desc, const void* initialData, bool allowLookup = true ) override;
    SamplerHandle createSampler( const RHI::SamplerDesc& desc, bool allowLookup = true ) override;

private:
    RHI::IDevice*      _device = nullptr;
    mutable std::mutex _mutex;

    template <typename T>
    struct ResourceRecord {
        T           ptr = nullptr;
        std::string name;
        ushort      generation = 0;
        bool        alive      = false;
    };

    // Buffers
    std::vector<ResourceRecord<RHI::BufferPtr>>   _buffers;
    std::unordered_map<std::string, BufferHandle> _buffNameToHandle;
    // Textures
    std::vector<ResourceRecord<RHI::TexturePtr>>   _textures;
    std::unordered_map<std::string, TextureHandle> _texNameToHandle;
    // Samplers
    std::vector<ResourceRecord<RHI::SamplerPtr>>   _samplers;
    std::unordered_map<std::string, SamplerHandle> _samplerNameToHandle;
};

} // namespace Graphics
AXION_NAMESPACE_END
