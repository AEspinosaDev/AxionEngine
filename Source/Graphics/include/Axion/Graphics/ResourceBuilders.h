#pragma once
#include "Axion/Graphics/RHI/Resource.h"
#include "Axion/Common/Helpers.h"
#include <string>

AXION_NAMESPACE_BEGIN
namespace Graphics {

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
        _desc.viewFlags |= TextureViewFlags::TextureViewDepthStencil;
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

protected:
    RHI::TextureDesc _desc;
};

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

} // namespace Graphics
AXION_NAMESPACE_END