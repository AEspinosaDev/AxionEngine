#pragma once
#include "Axion/Common/Math.h"
#include "Axion/Graphics/RHI/Common.h"
#include "Axion/Graphics/RHI/Memory.h"
#include "Axion/Graphics/RHI/IPipeline.h"
#include "Axion/Graphics/RHI/IResource.h"
#include "Axion/Graphics/RHI/ShaderBindingTable.h"

AXION_NAMESPACE_BEGIN

namespace Graphics::RHI {

DEFINE_COM_PTR_FOR_TYPE( ICommandList, CommandList )

/// @brief Interface for recording GPU commands.
/// Represents a hardware command buffer.
class ICommandList : public IObject
{
public:
    /// @brief Configuration descriptor for creating a Command List.
    struct Description {
        QueueType   queueType;     ///< The queue type this list will be submitted to (Graphics, Compute, Copy).
        uint        numFrames = 1; ///< Number of internal buffers for frame-in-flight rotation.
        std::string debugName = "";
    };

    virtual ~ICommandList() = default;

    // -------------------------------------------------------------------------
    // RECORDING LIFECYCLE
    // -------------------------------------------------------------------------

    /// @brief Resets the command list and starts recording.
    virtual void begin() = 0;

    /// @brief Stops recording. The list is now ready for submission.
    virtual void end() = 0;

    /// @brief Sets the index for the current frame in flight (0..FramesInFlight-1).
    /// Used to rotate internal allocators or versioned resources.
    virtual void setCurrentFrame( uint index ) = 0;

    /// @brief Returns the current frame index being recorded.
    virtual uint getCurrentFrame() const = 0;

    /// @brief Returns the creation description.
    virtual const Description& getDescription() const = 0;

    // -------------------------------------------------------------------------
    // SYNCHRONIZATION & BARRIERS
    // -------------------------------------------------------------------------

    /// @brief Inserts a resource barrier to transition a texture state.
    /// Ignored if the resource is already in the target state.
    virtual void barrier( ITexture* texture, ResourceState newState ) = 0;

    /// @brief Inserts a resource barrier to transition a buffer state.
    /// Ignored automatically if the buffer is on an Upload Heap.
    virtual void barrier( IBuffer* buffer, ResourceState newState ) = 0;

    // -------------------------------------------------------------------------
    // TRANSFER OPERATIONS
    // -------------------------------------------------------------------------

    /// @brief Clears a texture with a specific value.
    /// @param barrierPolicy If Auto, handles transition to RenderTarget/UnorderedAccess/DepthWrite automatically.
    virtual void clearTexture( ITexture* texture, const ClearValue& clearValue, BarrierPolicy barrierPolicy = BarrierPolicy::Auto ) = 0;

    /// @brief Copies data from one buffer to another (GPU to GPU).
    /// @param barrierPolicy If Auto, injects CopyDest/CopySource barriers. Use None for batching.
    virtual void copyBuffer( IBuffer* dst, IBuffer* src, ulong numBytes, ulong dstOffset = 0, ulong srcOffset = 0, BarrierPolicy barrierPolicy = BarrierPolicy::Auto ) = 0;

    /// @brief Copies a whole texture to another (GPU to GPU).
    /// @param barrierPolicy If Auto, injects CopyDest/CopySource barriers.
    virtual void copyTexture( ITexture* dst, ITexture* src, BarrierPolicy barrierPolicy = BarrierPolicy::Auto ) = 0;

    /// @brief Uploads CPU data to a GPU buffer using a Transient Allocator (Staging).
    /// @param allocator The frame-transient allocator to allocate upload memory from.
    /// @param barrierPolicy If Auto, transitions dst to CopyDest.
    virtual void uploadBuffer( IBuffer* dst, const void* data, ulong size, ulong dstOffset, ITransientAllocator* allocator, BarrierPolicy barrierPolicy = BarrierPolicy::Auto ) = 0;

    /// @brief Uploads CPU pixel data to a Texture using a Transient Allocator (Staging).
    /// Automatically handles row-pitch alignment and padding requirements.
    /// @param allocator The frame-transient allocator to allocate upload memory from.
    virtual void uploadTexture( ITexture* dst, const void* data, ITransientAllocator* allocator, uint mipSlice = 0, uint arraySlice = 0, BarrierPolicy barrierPolicy = BarrierPolicy::Auto ) = 0;

    // -------------------------------------------------------------------------
    // RAYTRACING ACCELERATION STRUCTURES
    // -------------------------------------------------------------------------

    /// @brief Updates an existing AS (Refit). Faster than build, but topology must match.
    /// Uses the allocator for scratch memory and instance uploads.
    virtual void updateAccel( IAccel* accel, const AccelDesc& newDesc, ITransientAllocator* allocator ) = 0;

    /// @brief Builds an AS from scratch.
    /// Uses the allocator for scratch memory and instance uploads.
    virtual void buildAccel( IAccel* accel, const AccelDesc& newDesc, ITransientAllocator* allocator ) = 0;

    // -------------------------------------------------------------------------
    // PIPELINE & BINDING
    // -------------------------------------------------------------------------

    virtual void bindComputePipeline( IComputePipeline* pipeline )       = 0;
    virtual void bindGraphicPipeline( IGraphicPipeline* pipeline )       = 0;
    virtual void bindRaytracingPipeline( IRayTracingPipeline* pipeline ) = 0;
    virtual void bindMeshPipeline( IMeshPipeline* pipeline )             = 0;

    /// @brief Binds a Descriptor Set (Resource Group) to a specific slot. Command buffer will automatically
    // use the last bound pipeline's layout
    virtual void bindDescriptorSet( uint setIndex, IDescriptorSet* set ) = 0;

    /// @brief Binds a Descriptor Set (Resource Group) to a specific slot. Command buffer will override and use
    // given layout and bind point;
    virtual void bindDescriptorSet( uint setIndex, IDescriptorSet* set, IPipelineLayout* layout, PipelineBindPoint bindPoint = PipelineBindPoint::Graphic ) = 0;

    // -------------------------------------------------------------------------
    // DISPATCH & DRAW
    // -------------------------------------------------------------------------

    /// @brief Dispatches a Compute Shader grid.
    virtual void dispatch( const Extent3D& gridSize ) = 0;

    /// @brief Dispatches a Ray Tracing grid.
    virtual void dispatchRays( const SBT::View& sbtView, const Extent3D& screenSize ) = 0;

    /// @brief Dispatches a Mesh grid (for mesh shading).
    virtual void dispatchMesh( const Extent3D& gridSize ) = 0;

    /// @brief Starts a dynamic rendering pass (No RenderPass object needed).
    virtual void beginRendering( const RenderingDesc& info ) = 0;

    /// @brief Ends the current rendering pass.
    virtual void endRendering() = 0;

    virtual void draw( uint vertexCount, uint instanceCount = 1, uint firstVertex = 0, uint firstInstance = 0 ) = 0;

    virtual void drawIndexed( uint indexCount,
                              uint instanceCount = 1,
                              uint firstIndex    = 0,
                              int  vertexOffset  = 0,
                              uint firstInstance = 0 ) = 0;

    virtual void bindVertexBuffer( uint slot, IBuffer* buffer ) = 0;
    virtual void bindIndexBuffer( IBuffer* buffer )             = 0;

    virtual void drawIndexedIndirect( IBuffer* indirectBuffer,
                                      ulong    bufferOffset,
                                      uint     maxDrawCount,
                                      IBuffer* countBuffer       = nullptr,
                                      ulong    countBufferOffset = 0 ) = 0;

    /// @brief Pushes 32-bit constants directly to the pipeline (Root Constants).
    /// @tparam T The struct type to push. Must be 4-byte aligned.
    template <typename T>
    void pushConstants( uint rootIndex, const T& data, uint offset32Bit = 0 ) {
        static_assert( sizeof( T ) % 4 == 0, "Push Constant struct size must be 4-byte aligned" );
        auto size = sizeof( T ) / 4;
        pushConstants( rootIndex, &data, size, offset32Bit );
    }

protected:
    /// @brief Internal implementation for push constants.
    virtual void pushConstants( uint setIndex, const void* data, uint numValues32Bit, uint offset32Bit = 0 ) = 0;
};

typedef ICommandList::Description CommandListDesc;

} // namespace Graphics::RHI

AXION_NAMESPACE_END