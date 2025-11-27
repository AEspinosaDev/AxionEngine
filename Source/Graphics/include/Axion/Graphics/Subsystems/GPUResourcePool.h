
#pragma once
#include "Axion/Graphics/Handle.h"
#include "Axion/Graphics/RHI/Resource.h"
#include <optional>
#include <string>
#include <vector>

AXION_NAMESPACE_BEGIN

namespace Graphics {

class IGPUResourcePool
{
public:
    virtual ~IGPUResourcePool() = default;

    IGPUResourcePool( const IGPUResourcePool& )            = delete;
    IGPUResourcePool& operator=( const IGPUResourcePool& ) = delete;

    class BufferBuilder;

    virtual BufferBuilder buffer( const std::string& name ) = 0;
    // TODO: TextureBuilder texture( const std::string& name );

    virtual RHI::IBuffer*               getBuffer( BufferHandle handle )            = 0;
    virtual std::optional<BufferHandle> findBuffer( const std::string& name ) const = 0;
    virtual void                        destroyBuffer( BufferHandle handle )        = 0;
    virtual void                        clear()                                     = 0;
    virtual uint                        size() const                                = 0;

protected:
    IGPUResourcePool() = default;

    virtual BufferHandle createBuffer( const RHI::BufferDesc& desc, const void* initialData ) = 0;

    friend class BufferBuilder;
};

class IGPUResourcePool::BufferBuilder
{
public:
    BufferBuilder( IGPUResourcePool& pool, std::string name )
        : _pool( pool ) {
        _desc.debugName  = std::move( name );
        _desc.memoryType = MemoryUsage::GPUOnly;
    }

    BufferBuilder& size( size_t numBytes ) {
        _desc.size = numBytes;
        return *this;
    }
    BufferBuilder& stride( uint32_t strideBytes ) {
        _desc.stride = strideBytes;
        return *this;
    }
    BufferBuilder& withData( const void* data ) {
        _initialData = data;
        return *this;
    }
    BufferBuilder& onGPU() {
        _desc.memoryType = MemoryUsage::GPUOnly;
        return *this;
    }
    BufferBuilder& onCPU() {
        _desc.memoryType = MemoryUsage::CPUVisible;
        return *this;
    }
    BufferBuilder& readback() {
        _desc.memoryType = MemoryUsage::Readback;
        return *this;
    }
    BufferBuilder& asVBO() {
        _desc.usageFlags |= BufferUsage::Vertex;
        return *this;
    }
    BufferBuilder& asIBO() {
        _desc.usageFlags |= BufferUsage::Index;
        return *this;
    }
    BufferBuilder& asReadOnlySSBO() {
        _desc.usageFlags |= BufferUsage::Storage;
        _desc.viewFlags |= BufferViewFlags::BufferViewShaderResource;
        return *this;
    }
    BufferBuilder& asSSBO() {
        _desc.usageFlags |= BufferUsage::Storage;
        _desc.viewFlags |= BufferViewFlags::BufferViewUnorderedAccess;
        return *this;
    }
    BufferBuilder& usage( BufferUsage flags ) {
        _desc.usageFlags = flags;
        return *this;
    }
    BufferBuilder& view( BufferViewFlags flags ) {
        _desc.viewFlags = flags;
        return *this;
    }
    BufferHandle create() {
        return _pool.createBuffer( _desc, _initialData );
    }

private:
    IGPUResourcePool& _pool;
    RHI::BufferDesc   _desc;
    const void*       _initialData = nullptr;
};

} // namespace Graphics
AXION_NAMESPACE_END
