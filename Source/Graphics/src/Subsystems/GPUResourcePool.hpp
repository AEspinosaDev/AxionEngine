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

    BufferBuilder buffer( const std::string& name ) override { return BufferBuilder( *this, name ); }
    // TODO: TextureBuilder texture( const std::string& name );

    RHI::IBuffer*               getBuffer( BufferHandle handle ) override;
    std::optional<BufferHandle> findBuffer( const std::string& name ) const override;
    void                        destroyBuffer( BufferHandle handle ) override;
    void                        clear() override;
    uint                        size() const override { return (uint)_buffers.size(); };

private:
    BufferHandle createBuffer( const RHI::BufferDesc& desc, const void* initialData ) override;

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
    std::unordered_map<std::string, BufferHandle> _nameToHandle;
    // Textures
    //  std::vector<ResourceRecord<RHI::TexturePtr>>   _textures;
    //  std::unordered_map<std::string, TextureHandle> _nameToHandle;
};

} // namespace Graphics
AXION_NAMESPACE_END
