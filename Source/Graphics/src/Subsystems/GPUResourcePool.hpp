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

    RHI::IBuffer*                getBuffer( BufferHandle handle ) override;
    std::optional<BufferHandle>  findBuffer( const std::string& name ) const override;
    void                         destroyBuffer( BufferHandle handle ) override;
    RHI::ITexture*               getTexture( TextureHandle handle ) override;
    std::optional<TextureHandle> findTexture( const std::string& name ) const override;
    void                         destroyTexture( TextureHandle handle ) override;

    //Special functions for renderer interop
    TextureHandle registerExternalTexture( RHI::ITexture* ptr, const std::string& name );
    BufferHandle  registerExternalBuffer( RHI::IBuffer* ptr, const std::string& name );

    void clear() override;
    uint buffersSize() const override { return (uint)_buffers.size(); };
    uint texturesSize() const override { return (uint)_textures.size(); };

private:
    BufferHandle  createBuffer( const RHI::BufferDesc& desc, const void* initialData ) override;
    TextureHandle createTexture( const RHI::TextureDesc& desc, const void* initialData ) override;

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
};

} // namespace Graphics
AXION_NAMESPACE_END
