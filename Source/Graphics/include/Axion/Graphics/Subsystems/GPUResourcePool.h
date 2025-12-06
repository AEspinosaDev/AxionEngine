#pragma once
#include "Axion/Graphics/Handle.h"
#include "Axion/Graphics/RHI/Resource.h"
#include "Axion/Graphics/ResourceBuilders.h"
#include <optional>
#include <string>
#include <vector>

AXION_NAMESPACE_BEGIN

namespace Graphics {

/// @brief Interface for the central GPU Resource Pool.
/// Manages the lifecycle, storage, and retrieval of physical GPU resources (Buffers and Textures).
class IGPUResourcePool
{
public:
    virtual ~IGPUResourcePool() = default;

    // Non-copyable
    IGPUResourcePool( const IGPUResourcePool& )            = delete;
    IGPUResourcePool& operator=( const IGPUResourcePool& ) = delete;

    // Forward declarations of nested builders
    class BufferBuilder;
    class TextureBuilder;
    class SamplerBuilder;

    // -------------------------------------------------------------------------
    // ENTRY POINTS
    // -------------------------------------------------------------------------

    /// @brief Starts the fluent construction of a GPU Buffer.
    /// @param name Debug name for the resource.
    virtual BufferBuilder buffer( const std::string& name ) = 0;

    /// @brief Starts the fluent construction of a GPU Texture.
    /// @param name Debug name for the resource.
    virtual TextureBuilder texture( const std::string& name ) = 0;

    /// @brief Starts the fluent construction of a GPU Sampler.
    /// @param name Debug name for the resource.
    virtual SamplerBuilder sampler( const std::string& name ) = 0;

    // -------------------------------------------------------------------------
    // RUNTIME ACCESS
    // -------------------------------------------------------------------------

    /// @brief Retrieves the raw RHI buffer pointer associated with a handle.
    /// @return Pointer to IBuffer or nullptr if handle is invalid/dead.
    virtual RHI::IBuffer* getBuffer( BufferHandle handle ) = 0;

    /// @brief Looks up a buffer handle by its debug name.
    virtual std::optional<BufferHandle> findBuffer( const std::string& name ) const = 0;

    /// @brief Destroys the buffer and frees GPU memory immediately.
    virtual void destroyBuffer( BufferHandle handle ) = 0;

    /// @brief Retrieves the raw RHI texture pointer associated with a handle.
    /// @return Pointer to ITexture or nullptr if handle is invalid/dead.
    virtual RHI::ITexture* getTexture( TextureHandle handle ) = 0;

    /// @brief Looks up a texture handle by its debug name.
    virtual std::optional<TextureHandle> findTexture( const std::string& name ) const = 0;

    /// @brief Destroys the texture and frees GPU memory immediately.
    virtual void destroyTexture( TextureHandle handle ) = 0;

    /// @brief Retrieves the raw RHI sampler pointer associated with a handle.
    /// @return Pointer to ISampler or nullptr if handle is invalid/dead.
    virtual RHI::ISampler* getSampler( SamplerHandle handle ) = 0;

    /// @brief Looks up a sampler handle by its debug name.
    virtual std::optional<SamplerHandle> findSampler( const std::string& name ) const = 0;

    /// @brief Destroys the sampler and frees GPU memory immediately.
    virtual void destroySampler( SamplerHandle handle ) = 0;

    // -------------------------------------------------------------------------
    // LIFECYCLE & UTILS
    // -------------------------------------------------------------------------

    /// @brief Destroys ALL resources in the pool. Use with caution.
    virtual void clear() = 0;

    /// @brief Returns the total number of buffer slots occupied (whether they are alive or not).
    virtual uint bufferCount() const = 0;

    /// @brief Returns the total number of texture slots occupied (whether they are alive or not).
    virtual uint textureCount() const = 0;

    /// @brief Returns the total number of samplers slots occupied (whether they are alive or not).
    virtual uint samplerCount() const = 0;

protected:
    IGPUResourcePool() = default;

    // Internal creation methods called by builders
    virtual BufferHandle  createBuffer( const RHI::BufferDesc& desc, const void* initialData, bool allowLookup = true )   = 0;
    virtual TextureHandle createTexture( const RHI::TextureDesc& desc, const void* initialData, bool allowLookup = true ) = 0;
    virtual SamplerHandle createSampler( const RHI::SamplerDesc& desc, bool allowLookup = true )                          = 0;

    friend class BufferBuilder;
    friend class TextureBuilder;
    friend class SamplerBuilder;
};

// -----------------------------------------------------------------------------
// BUILDER IMPLEMENTATIONS
// -----------------------------------------------------------------------------

/// @brief Fluent builder for configuring and creating Textures in the pool.
class IGPUResourcePool::TextureBuilder : public TextureBuilderBase<TextureBuilder>
{
public:
    TextureBuilder( IGPUResourcePool& pool, std::string name )
        : TextureBuilderBase( std::move( name ) )
        , _pool( pool ) {}

    /// @brief Marks resource as transient (Not fetchable by name).
    TextureBuilder& transient() {
        _allowLookup = false;
        return *this;
    }

    /// @brief Sets initial data to upload to the texture upon creation.
    TextureBuilder& withData( const void* data ) {
        _initialData = data;
        return *this;
    }

    /// @brief Finalizes configuration and creates the physical resource.
    TextureHandle create() {
        return _pool.createTexture( _desc, _initialData, _allowLookup );
    }

private:
    IGPUResourcePool& _pool;
    const void*       _initialData = nullptr;
    bool              _allowLookup = true;
};

/// @brief Fluent builder for configuring and creating Buffers in the pool.
class IGPUResourcePool::BufferBuilder : public BufferBuilderBase<BufferBuilder>
{
public:
    BufferBuilder( IGPUResourcePool& pool, std::string name )
        : BufferBuilderBase( std::move( name ) )
        , _pool( pool ) {}

    /// @brief Marks if name will be added as a key for future lookups.
    BufferBuilder& transient() {
        _allowLookup = false;
        return *this;
    }

    /// @brief Sets initial data to upload to the buffer upon creation.
    BufferBuilder& withData( const void* data ) {
        _initialData = data;
        return *this;
    }

    /// @brief Finalizes configuration and creates the physical resource.
    BufferHandle create() {
        return _pool.createBuffer( _desc, _initialData, _allowLookup );
    }

private:
    IGPUResourcePool& _pool;
    const void*       _initialData = nullptr;
    bool              _allowLookup = true;
};

/// @brief Fluent builder for configuring and creating Textures in the pool.
class IGPUResourcePool::SamplerBuilder : public SamplerBuilderBase<SamplerBuilder>
{
public:
    SamplerBuilder( IGPUResourcePool& pool, std::string name )
        : SamplerBuilderBase( std::move( name ) )
        , _pool( pool ) {}

    /// @brief Marks if name will be added as a key for future lookups.
    SamplerBuilder& transient() {
        _allowLookup = false;
        return *this;
    }

    /// @brief Finalizes configuration and creates the physical resource.
    SamplerHandle create() {
        return _pool.createSampler( _desc, _allowLookup );
    }

private:
    IGPUResourcePool& _pool;
    bool              _allowLookup = true;
};

} // namespace Graphics
AXION_NAMESPACE_END