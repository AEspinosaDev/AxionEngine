#pragma once
#include "Axion/Graphics/RHI/Common.h"
#include "Axion/Graphics/RHI/ICommandList.h"
#include "Axion/Graphics/RHI/Memory.h"
#include "Axion/Graphics/RHI/IPipeline.h"
#include "Axion/Graphics/RHI/IResource.h"
#include "Axion/Graphics/RHI/ShaderBindingTable.h"
#include "Axion/Graphics/RHI/ISwapchain.h"

AXION_NAMESPACE_BEGIN

namespace Graphics::RHI {

DEFINE_COM_PTR_FOR_TYPE( IDevice, Device )

/// @brief Simple synchronization primitive holding a fence value.
struct Fence {
    ulong value = 0;
};

/// @brief Represents a logical connection to the physical graphics adapter.
/// Acts as the factory for all GPU resources and the entry point for command submission.
class IDevice : public IObject
{
public:
    virtual ~IDevice() = default;

    // -------------------------------------------------------------------------
    // RESOURCE CREATION
    // -------------------------------------------------------------------------

    /// @brief Creates a swapchain associated with a native window handle.
    /// @param Ptr Native window handle (HWND on Windows).
    virtual SwapchainPtr createSwapchain( const NativeObject& Ptr, const SwapchainDesc& desc = {} ) = 0;

    /// @brief Creates a command list for recording GPU commands.
    virtual CommandListPtr createCommandList( const CommandListDesc& desc ) = 0;

    /// @brief Creates a texture resource.
    /// @param initialData Optional pointer to raw pixel data. If provided, performs a synchronous upload.
    virtual TexturePtr createTexture( const TextureDesc& desc, const void* initialData = nullptr ) = 0;

    /// @brief Creates a buffer resource.
    /// @param initialData Optional pointer to raw data. If provided, performs a synchronous upload.
    virtual BufferPtr createBuffer( const BufferDesc& desc, const void* initialData = nullptr ) = 0;

    virtual SamplerPtr createSampler( const SamplerDesc& desc ) = 0;

    /// @brief Allocates a Raytracing Acceleration Structure.
    /// @param immediateBuild If true, blocks CPU to perform an immediate build/upload (useful for static scenes or tests).
    /// If false, allocates memory but leaves the AS empty (must be built via CommandList::buildAccel).
    virtual AccelPtr createAccel( const AccelDesc& desc, bool immediateBuild = false ) = 0;

    // -------------------------------------------------------------------------
    // PIPELINE CREATION
    // -------------------------------------------------------------------------

    virtual PipelineLayoutPtr     createPipelineLayout( const PipelineLayoutDesc& desc )         = 0;
    virtual GraphicPipelinePtr    createGraphicPipeline( const GraphicPipelineDesc& desc )       = 0;
    virtual ComputePipelinePtr    createComputePipeline( const ComputePipelineDesc& desc )       = 0;
    virtual RayTracingPipelinePtr createRayTracingPipeline( const RayTracingPipelineDesc& desc ) = 0;
    virtual MeshPipelinePtr       createMeshPipeline( const MeshPipelineDesc& desc )             = 0;

    // -------------------------------------------------------------------------
    // ALLOCATORS
    // -------------------------------------------------------------------------

    /// @brief Creates a pool for managing resource descriptors (SRV/UAV/CBV).
    virtual DescriptorAllocatorPtr createDescriptorAllocator( const DescriptorAllocatorDesc& desc ) = 0;

    /// @brief Creates an allocator for Shader Binding Tables (Raytracing).
    virtual SBTAllocatorPtr createSBTAllocator( const SBTAllocatorDesc& desc ) = 0;

    /// @brief Creates a transient linear allocator for per-frame dynamic uploads.
    virtual TransientAllocatorPtr createTransientAllocator( const TransientAllocatorDesc& desc ) = 0;

    // -------------------------------------------------------------------------
    // SUBMISSION & SYNCHRONIZATION
    // -------------------------------------------------------------------------

    /// @brief Submits a batch of command lists to the GPU queue.
    /// @param frameFence The fence to signal when this batch completes execution.
    virtual void executeCommandLists( const std::vector<ICommandList*>& lists, QueueType workingQueue, Fence& frameFence ) = 0;

    /// @brief Blocks the CPU until the specified fence value is reached (Frame Pacing).
    virtual void waitForFrame( const Fence& frameFence, QueueType workingQueue ) = 0;

    /// @brief Makes the 'workingQueue' wait for 'dstQueue' to finish its current work (GPU-side sync).
    virtual void waitForQueue( QueueType workingQueue, QueueType dstQueue ) = 0;

    /// @brief Helper to wait for a specific queue to be completely idle.
    virtual void queueWaitIdle( QueueType workingQueue, Fence& frameFence ) = 0;

    /// @brief Blocks CPU until the entire Device (all queues) is idle.
    virtual bool waitIdle() = 0;

    /// @brief Executes a lambda with a temporary command list immediately.
    /// Thread-safe. Blocks until GPU finishes execution.
    /// Useful for initializing resources or small one-off operations.
    virtual void oneTimeSubmit( std::function<void( ICommandList* cmd )>& commands ) = 0;

    // -------------------------------------------------------------------------
    // CAPABILITIES & INFO
    // -------------------------------------------------------------------------

    /// @brief Queries if a specific hardware feature is supported.
    virtual bool queryFeatureSupport( Feature feature, void* pInfo = nullptr, size_t infoSize = 0 ) const = 0;

    /// @brief Checks support for a specific texture/buffer format.
    virtual FormatSupport queryFormatSupport( Format format ) const = 0;

    /// @brief Returns the underlying graphics API backend (DirectX12, Vulkan, etc.).
    virtual API getGraphicsAPI() = 0;

protected:
    virtual void checkExtensions() = 0;
};

} // namespace Graphics::RHI

AXION_NAMESPACE_END