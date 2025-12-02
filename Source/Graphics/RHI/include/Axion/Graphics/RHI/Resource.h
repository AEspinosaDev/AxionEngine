#pragma once
#include "Axion/Graphics/RHI/Common.h"

AXION_NAMESPACE_BEGIN

namespace Graphics::RHI {

DEFINE_COM_PTR_FOR_TYPE( ITexture, Texture )

// Texture are always GPU. If you waNT TO wark with CPU ones, use a buffer.
class ITexture : public IResource
{
public:
    struct Description {
        Extent3D         size        = { 1, 1, 1 };
        Format           format      = Format::UNKNOWN;
        TextureDimension dimension   = TextureDimension::Texture2D;
        uint             mipLevels   = 1;
        uint             sampleCount = 1;
        uint             arraySize   = 1;
        std::string      debugName   = "";
        TextureViewFlags viewFlags   = TextureViewShaderResource;

        bool operator==( const Description& other ) const {
            return size == other.size &&
                   format == other.format &&
                   dimension == other.dimension &&
                   mipLevels == other.mipLevels &&
                   sampleCount == other.sampleCount &&
                   arraySize == other.arraySize &&
                   debugName == other.debugName &&
                   viewFlags == other.viewFlags;
        }
        bool operator!=( const Description& other ) const {
            return !operator==( other );
        }
    };
    virtual ~ITexture()                                          = default;
    virtual const ITexture::Description& getDescription() const  = 0;
    virtual ResourceState                getCurrentState() const = 0;
};

typedef ITexture::Description TextureDesc;

DEFINE_COM_PTR_FOR_TYPE( IBuffer, Buffer )

class IBuffer : public IResource
{
public:
    struct Description {
        size_t          size       = 0;
        uint            stride     = 1; // for structured buffers
        MemoryUsage     memoryType = MemoryUsage::GPUOnly;
        BufferUsage     usageFlags = BufferUsage::None;
        BufferViewFlags viewFlags  = BufferViewNone;
        std::string     debugName  = "";

        bool operator==( const Description& other ) const {
            return size == other.size &&
                   stride == other.stride &&
                   memoryType == other.memoryType &&
                   usageFlags == other.usageFlags &&
                   viewFlags == other.viewFlags &&
                   debugName == other.debugName;
        }

        bool operator!=( const Description& other ) const {
            return !operator==( other );
        }
    };

    virtual ~IBuffer()                                 = default;
    virtual const Description& getDescription() const  = 0;
    virtual ResourceState      getCurrentState() const = 0;

    virtual void copyData( const void* data, ulong size, ulong offset = 0 ) = 0;

    template <typename T>
    void copyData( const T& data, ulong offset = 0 ) {
        copyData( &data, sizeof( T ), offset );
    }
    // --- Vector Helper ---
    template <typename T>
    void copyData( const std::vector<T>& data, size_t offset = 0 ) {
        copyData( data.data(), data.size() * sizeof( T ), offset );
    }

protected:
    virtual void* map()   = 0;
    virtual void  unmap() = 0;
};

using BufferDesc = IBuffer::Description;

DEFINE_COM_PTR_FOR_TYPE( IAccel, Accel )

class IAccel : public IResource
{
public:
    virtual ~IAccel() = default;
};

} // namespace Graphics::RHI

AXION_NAMESPACE_END