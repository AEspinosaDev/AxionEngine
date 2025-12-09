#pragma once
#include "Axion/Graphics/RHI/Common.h"

AXION_NAMESPACE_BEGIN

namespace Graphics::RHI {

#pragma region Texture

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
        ClearValue       clearValue  = { .color = { 0.0f, 0.0f, 0.0f, 1.0f }, .depth = { 1.0f } }; // Only if RenderTarget or DepthStencil

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

#pragma endregion
#pragma region Buffer

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

#pragma endregion
#pragma region Accel

DEFINE_COM_PTR_FOR_TYPE( IAccel, Accel )

class IAccel : public IResource
{
public:
    struct Description {
        AccelType                      type;
        AccelBuildFlags                flags;
        std::vector<AccelGeometryDesc> geometries; // Only valid if type == AccelType::BottomLevel
        std::vector<AccelInstanceDesc> instances;  // Only valid if type == AccelType::TopLevel
        std::string                    debugName = nullptr;

        bool operator==( const Description& other ) const {
            return type == other.type &&
                   flags == other.flags &&
                   geometries == other.geometries &&
                   instances == other.instances &&
                   debugName == other.debugName;
        }
        bool operator!=( const Description& other ) const {
            return !operator==( other );
        }
    };

    virtual ~IAccel() = default;

    virtual const Description& getDescription() const   = 0;
    virtual AccelType          getType() const          = 0;
    virtual ulong              getDeviceAddress() const = 0;

    // 'scratchBuffer' might be needed ??
    // virtual void build( void* commandList ) = 0;
};

using AccelDesc = IAccel::Description;

#pragma endregion
#pragma region Sampler

DEFINE_COM_PTR_FOR_TYPE( ISampler, Sampler )

class ISampler : public IResource
{
public:
    virtual ~ISampler() = default;

    struct Description {
        Filter      minFilter     = Filter::Linear;
        Filter      magFilter     = Filter::Linear;
        Filter      mipFilter     = Filter::Linear;
        AddressMode addressU      = AddressMode::Repeat;
        AddressMode addressV      = AddressMode::Repeat;
        AddressMode addressW      = AddressMode::Repeat;
        uint        maxAnisotropy = 16;
        float       maxLOD        = 12.0;
        float       minLOD        = 0.0f;
        float       mipLODBias    = 0.0f;
        CompareOp   compareOp     = CompareOp::Never;
        std::string debugName;

        bool operator==( const Description& other ) const {
            return minFilter == other.minFilter &&
                   magFilter == other.magFilter &&
                   mipFilter == other.mipFilter &&
                   addressU == other.addressU &&
                   addressV == other.addressV &&
                   addressW == other.addressW &&
                   maxAnisotropy == other.maxAnisotropy &&
                   maxLOD == other.maxLOD &&
                   minLOD == other.minLOD &&
                   mipLODBias == other.mipLODBias &&
                   compareOp == other.compareOp &&
                   debugName == other.debugName;
        }

        bool operator!=( const Description& other ) const {
            return !operator==( other );
        }
    };
    virtual const Description& getDescription() const = 0;
};

using SamplerDesc = ISampler::Description;

} // namespace Graphics::RHI

AXION_NAMESPACE_END