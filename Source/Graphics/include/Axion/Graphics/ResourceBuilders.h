#pragma once
#include "Axion/Common/Helpers.h"
#include "Axion/Graphics/RHI/Resource.h"
#include <string>

AXION_NAMESPACE_BEGIN
namespace Graphics {

#pragma region Texture
/// @brief Base class for building Texture descriptions using Fluent Interface pattern (CRTP).
/// @tparam T The derived builder class.
template <typename T>
class TextureBuilderBase
{
public:
    TextureBuilderBase( std::string name ) {
        _desc.debugName = std::move( name );
        _desc.dimension = TextureDimension::Texture2D;
        _desc.format    = Format::RGBA8_UNORM;
        _desc.viewFlags = TextureViewFlags::TextureViewShaderResource;
        _desc.size      = { 1, 1, 1 };
    }

    /// @brief Sets texture dimensions. Auto-promotes to Texture3D if depth > 1.
    T& extent( uint width, uint height, uint depth = 1 ) {
        _desc.size = { width, height, depth };
        if ( depth > 1 )
            _desc.dimension = TextureDimension::Texture3D;
        return static_cast<T&>( *this );
    }

    /// @brief Sets texture dimensions.
    T& extent( const Extent3D& extent ) {
        _desc.size = extent;
        if ( extent.depth > 1 )
            _desc.dimension = TextureDimension::Texture3D;
        return static_cast<T&>( *this );
    }

    /// @brief Sets the pixel format.
    T& format( Format fmt ) {
        _desc.format = fmt;
        return static_cast<T&>( *this );
    }

    /// @brief Explicitly sets the dimension type.
    T& dim( TextureDimension dim ) {
        _desc.dimension = dim;
        return static_cast<T&>( *this );
    }

    /// @brief Configures as a Cubemap (Sets Cube dimension and ArraySize 6).
    T& asCubeMap() {
        _desc.dimension = TextureDimension::TextureCube;
        _desc.arraySize = 6;
        return static_cast<T&>( *this );
    }

    /// @brief Sets the number of mip levels.
    T& mips( uint levels ) {
        _desc.mipLevels = levels;
        return static_cast<T&>( *this );
    }

    /// @brief Sets the array size.
    T& array( uint size ) {
        _desc.arraySize = size;
        return static_cast<T&>( *this );
    }

    /// @brief Sets MSAA sample count.
    T& samples( uint samples ) {
        _desc.sampleCount = samples;
        return static_cast<T&>( *this );
    }

    /// @brief Enables Render Target usage (Color Attachment).
    T& asRenderTarget() {
        _desc.viewFlags |= TextureViewFlags::TextureViewRenderTarget;
        return static_cast<T&>( *this );
    }

    /// @brief Enables Depth/Stencil usage.
    T& asDepthStencil() {
        _desc.viewFlags = TextureViewFlags::TextureViewDepthStencil;
        return static_cast<T&>( *this );
    }

    /// @brief Enables UAV usage (Storage/RWTexture).
    T& asStorage() {
        _desc.viewFlags |= TextureViewFlags::TextureViewUnorderedAccess;
        return static_cast<T&>( *this );
    }

    /// @brief Manually sets view flags.
    T& flags( TextureViewFlags flags ) {
        _desc.viewFlags = flags;
        return static_cast<T&>( *this );
    }

    T& clearValue( const ClearValue& val ) {
        _desc.clearValue = val; 
        return static_cast<T&>( *this );
    }

protected:
    RHI::TextureDesc _desc;
};

#pragma endregion
#pragma region Buffer

/// @brief Base class for building Buffer descriptions using Fluent Interface pattern (CRTP).
/// @tparam T The derived builder class.
template <typename T>
class BufferBuilderBase
{
public:
    BufferBuilderBase( std::string name ) {
        _desc.debugName  = std::move( name );
        _desc.memoryType = MemoryUsage::GPUOnly;
    }

    /// @brief Sets total size in bytes.
    T& size( size_t numBytes ) {
        _desc.size = numBytes;
        return static_cast<T&>( *this );
    }

    /// @brief Sets element stride (for structured buffers).
    T& stride( uint strideBytes ) {
        _desc.stride = strideBytes;
        return static_cast<T&>( *this );
    }

    /// @brief Sets memory to GPU-Only (Fastest, no CPU access).
    T& onGPU() {
        _desc.memoryType = MemoryUsage::GPUOnly;
        return static_cast<T&>( *this );
    }

    /// @brief Sets memory to CPU-Visible (Upload Heap).
    T& onCPU() {
        _desc.memoryType = MemoryUsage::CPUVisible;
        return static_cast<T&>( *this );
    }

    /// @brief Sets memory to Readback Heap (GPU to CPU).
    T& readback() {
        _desc.memoryType = MemoryUsage::Readback;
        return static_cast<T&>( *this );
    }

    /// @brief Marks usage as Vertex Buffer.
    T& asVBO() {
        _desc.usageFlags |= BufferUsage::Vertex;
        return static_cast<T&>( *this );
    }

    /// @brief Marks usage as Index Buffer.
    T& asIBO() {
        _desc.usageFlags |= BufferUsage::Index;
        return static_cast<T&>( *this );
    }

    /// @brief Configures as Read-Only Structured Buffer (SRV).
    T& asReadOnlySSBO() {
        _desc.usageFlags |= BufferUsage::Storage;
        _desc.viewFlags |= BufferViewFlags::BufferViewShaderResource;
        return static_cast<T&>( *this );
    }

    /// @brief Configures as Read-Write Structured Buffer (UAV).
    T& asSSBO() {
        _desc.usageFlags |= BufferUsage::Storage;
        _desc.viewFlags |= BufferViewFlags::BufferViewUnorderedAccess;
        return static_cast<T&>( *this );
    }

    /// @brief Configures as a constant buffer, great for uniform handling (CBO).
    T& asCBO() {
        _desc.usageFlags |= BufferUsage::Uniform;
        _desc.viewFlags |= BufferViewFlags::BufferViewConstantBuffer;
        _desc.size = Helpers::alignUp( _desc.size, (size_t)256 );
        return static_cast<T&>( *this );
    }

    /// @brief Manually sets usage flags.
    T& usage( BufferUsage flags ) {
        _desc.usageFlags = flags;
        return static_cast<T&>( *this );
    }

    /// @brief Manually sets view flags.
    T& view( BufferViewFlags flags ) {
        _desc.viewFlags = flags;
        return static_cast<T&>( *this );
    }

protected:
    RHI::BufferDesc _desc;
};

#pragma endregion
#pragma region Sampler

template <typename T>
class SamplerBuilderBase
{
public:
    SamplerBuilderBase( std::string name ) {
        _desc.debugName = std::move( name );
        _desc.minFilter = Filter::Linear;
        _desc.magFilter = Filter::Linear;
        _desc.mipFilter = Filter::Linear;
        _desc.addressU  = AddressMode::Repeat;
        _desc.addressV  = AddressMode::Repeat;
        _desc.addressW  = AddressMode::Repeat;
    }

    T& filter( Filter min, Filter mag, Filter mip ) {
        _desc.minFilter = min;
        _desc.magFilter = mag;
        _desc.mipFilter = mip;
        return static_cast<T&>( *this );
    }

    /// @brief Configures Texture Address Mode for each direction equally (U,V,W).
    T& address( AddressMode mode ) {
        _desc.addressU = _desc.addressV = _desc.addressW = mode;
        return static_cast<T&>( *this );
    }

    /// @brief Configures Texture Address Mode for each direction individually (U,V,W)
    T& address( AddressMode u, AddressMode v, AddressMode w ) {
        _desc.addressU = u;
        _desc.addressV = v;
        _desc.addressW = w;
        return static_cast<T&>( *this );
    }

    /// @brief Max Anysotropic Filtering
    T& anisotropy( uint maxAniso ) {
        _desc.maxAnisotropy = maxAniso;
        return static_cast<T&>( *this );
    }

    /// @brief Sets LOD for mipmapping
    T& mipLOD( float min, float max ) {
        _desc.minLOD = min;
        _desc.maxLOD = max;
        return static_cast<T&>( *this );
    }

    /// @brief Sets LOD bias for mipmapping
    T& mipLODBias( float bias ) {
        _desc.mipLODBias = bias;
        return static_cast<T&>( *this );
    }

    /// @brief Sets Compare Operation
    T& compareOP( RHI::CompareOp op ) {
        _desc.compareOp = op;
        return static_cast<T&>( *this );
    }

protected:
    RHI::SamplerDesc _desc;
};

#pragma endregion
} // namespace Graphics
AXION_NAMESPACE_END