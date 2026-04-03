#pragma once
#include "Axion/Graphics/Handle.h"
#include "Axion/Graphics/RHI/IResource.h"
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

    // Non-copyable (Owned by Renderer)
    AXION_DISABLE_COPY( IGPUResourcePool )

    // Forward declarations of nested builders
    class BufferBuilder;
    class TextureBuilder;
    class SamplerBuilder;
    class AccelBuilder;

    // -------------------------------------------------------------------------
    // ENTRY POINTS
    // -------------------------------------------------------------------------

    /// @brief Starts the fluent construction of a GPU Buffer.
    /// @param name Debug name for the resource.
    virtual BufferBuilder buffer( StringView name ) = 0;

    /// @brief Starts the fluent construction of a GPU Texture.
    /// @param name Debug name for the resource.
    virtual TextureBuilder texture( StringView name ) = 0;

    /// @brief Starts the fluent construction of a GPU Sampler.
    /// @param name Debug name for the resource.
    virtual SamplerBuilder sampler( StringView name ) = 0;

    /// @brief Starts the fluent construction of a GPU Acceleration Structure.
    /// @param name Debug name for the resource.
    virtual AccelBuilder accel( StringView name ) = 0;

    // -------------------------------------------------------------------------
    // RUNTIME ACCESS
    // -------------------------------------------------------------------------

    /// @brief Retrieves the raw RHI buffer pointer associated with a handle.
    /// @return Pointer to IBuffer or nullptr if handle is invalid/dead.
    virtual RHI::IBuffer* getBuffer( BufferHandle handle ) = 0;

    /// @brief Looks up a buffer handle by its debug name.
    virtual std::optional<BufferHandle> findBuffer( StringView name ) const = 0;

    /// @brief Destroys the buffer and frees GPU memory immediately.
    virtual void destroyBuffer( BufferHandle handle ) = 0;

    /// @brief Retrieves the raw RHI texture pointer associated with a handle.
    /// @return Pointer to ITexture or nullptr if handle is invalid/dead.
    virtual RHI::ITexture* getTexture( TextureHandle handle ) = 0;

    /// @brief Looks up a texture handle by its debug name.
    virtual std::optional<TextureHandle> findTexture( StringView name ) const = 0;

    /// @brief Destroys the texture and frees GPU memory immediately.
    virtual void destroyTexture( TextureHandle handle ) = 0;

    /// @brief Retrieves the raw RHI sampler pointer associated with a handle.
    /// @return Pointer to ISampler or nullptr if handle is invalid/dead.
    virtual RHI::ISampler* getSampler( SamplerHandle handle ) = 0;

    /// @brief Looks up a sampler handle by its debug name.
    virtual std::optional<SamplerHandle> findSampler( StringView name ) const = 0;

    /// @brief Destroys the sampler and frees GPU memory immediately.
    virtual void destroySampler( SamplerHandle handle ) = 0;

    /// @brief Retrieves the raw RHI Acccel pointer associated with a handle.
    /// @return Pointer to IAccel or nullptr if handle is invalid/dead.
    virtual RHI::IAccel* getAccel( AccelHandle handle ) = 0;

    /// @brief Looks up a accel handle by its debug name.
    virtual std::optional<AccelHandle> findAccel( StringView name ) const = 0;

    /// @brief Destroys the accel and frees GPU memory immediately.
    virtual void destroyAccel( AccelHandle handle ) = 0;

    // -------------------------------------------------------------------------
    // LIFECYCLE & UTILS
    // -------------------------------------------------------------------------

    /// @brief Destroys ALL resources in the pool. Use with caution.
    virtual void clear() = 0;

    /// @brief Returns the total number of buffer slots occupied (whether they are alive or not).
    virtual u32 bufferCount() const = 0;

    /// @brief Returns the total number of texture slots occupied (whether they are alive or not).
    virtual u32 textureCount() const = 0;

    /// @brief Returns the total number of samplers slots occupied (whether they are alive or not).
    virtual u32 samplerCount() const = 0;

    /// @brief Returns the total number of samplers slots occupied (whether they are alive or not).
    virtual u32 accelCount() const = 0;

protected:
    IGPUResourcePool() = default;

    // Internal creation methods called by builders
    virtual BufferHandle  createBuffer( const RHI::BufferDesc& desc, const void* initialData, bool allowLookup = true )   = 0;
    virtual TextureHandle createTexture( const RHI::TextureDesc& desc, const void* initialData, bool allowLookup = true ) = 0;
    virtual SamplerHandle createSampler( const RHI::SamplerDesc& desc, bool allowLookup = true )                          = 0;
    virtual AccelHandle   createAccel( const RHI::AccelDesc& desc, bool instantBuild = false, bool allowLookup = true )   = 0;

    friend class BufferBuilder;
    friend class TextureBuilder;
    friend class SamplerBuilder;
    friend class AccelBuilder;
};

// -----------------------------------------------------------------------------
// BUILDER IMPLEMENTATIONS
// -----------------------------------------------------------------------------

/// @brief Fluent builder for configuring and creating Textures in the pool.
class IGPUResourcePool::TextureBuilder : public TextureBuilderBase<TextureBuilder>
{
public:
    TextureBuilder( IGPUResourcePool& pool, StringView name )
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
    BufferBuilder( IGPUResourcePool& pool, StringView name )
        : BufferBuilderBase( name )
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

/// @brief Fluent builder for configuring and creating Samplers in the pool.
class IGPUResourcePool::SamplerBuilder : public SamplerBuilderBase<SamplerBuilder>
{
public:
    SamplerBuilder( IGPUResourcePool& pool, StringView name )
        : SamplerBuilderBase( name )
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

/// @brief Fluent builder for configuring and creating Accleration Structures in the pool.
class IGPUResourcePool::AccelBuilder : public AccelBuilderBase<AccelBuilder>
{
public:
    AccelBuilder( IGPUResourcePool& pool, StringView name )
        : AccelBuilderBase( name )
        , _pool( pool ) {}

    /// @brief Marks if name will be added as a key for future lookups.
    AccelBuilder& transient() {
        _allowLookup = false;
        return *this;
    }

    /// @brief Marks if the accel will be built on creation time. Only recommended for simple testing or scenes.
    AccelBuilder& instantBuild() {
        _instantBuild = true;
        return *this;
    }

    /// @brief Finalizes configuration and creates the physical resource.
    AccelHandle create() {
        return _pool.createAccel( _desc, _instantBuild, _allowLookup );
    }

private:
    IGPUResourcePool& _pool;
    bool              _allowLookup  = true;
    bool              _instantBuild = false;
};

} // namespace Graphics
AXION_NAMESPACE_END