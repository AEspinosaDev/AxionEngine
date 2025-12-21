#pragma once
#include "Axion/Graphics/RHI/CommandList.h"
#include "Axion/Graphics/RHI/Common.h"
#include "Axion/Graphics/RHI/Pipeline.h"
#include "Axion/Graphics/RHI/Resource.h"
#include "Axion/Graphics/RHI/ShaderBindingTable.h"
#include "Axion/Graphics/RHI/Swapchain.h"

AXION_NAMESPACE_BEGIN

namespace Graphics::RHI {

DEFINE_COM_PTR_FOR_TYPE( IDevice, Device )

struct Fence {
    ulong value = 0;
};

class IDevice : public IResource
{
public:
    virtual ~IDevice() = default;

    virtual SwapchainPtr   createSwapchain( const NativeObject& Ptr, const SwapchainDesc& desc = {} )  = 0;
    virtual CommandListPtr createCommandList( const CommandListDesc& desc )                            = 0;
    virtual TexturePtr     createTexture( const TextureDesc& desc, const void* initialData = nullptr ) = 0;
    virtual BufferPtr      createBuffer( const BufferDesc& desc, const void* initialData = nullptr )   = 0;
    virtual SamplerPtr     createSampler( const SamplerDesc& desc )                                    = 0;
    virtual AccelPtr       createAccel( const AccelDesc& desc )                                        = 0;
    virtual bool           updateAccel( IAccel* accel, const AccelDesc& desc )                         = 0;
    // virtual std::vector<AccelPtr>  createAccelBatch( const std::vector<AccelDesc>& descs )                     = 0;
    virtual PipelineLayoutPtr      createPipelineLayout( const PipelineLayoutDesc& desc )           = 0;
    virtual GraphicPipelinePtr     createGraphicPipeline( const GraphicPipelineDesc& desc )         = 0;
    virtual ComputePipelinePtr     createComputePipeline( const ComputePipelineDesc& desc )         = 0;
    virtual RayTracingPipelinePtr  createRayTracingPipeline( const RayTracingPipelineDesc& desc )   = 0;
    virtual DescriptorAllocatorPtr createDescriptorAllocator( const DescriptorAllocatorDesc& desc ) = 0;
    virtual SBTAllocatorPtr        createSBTAllocator( const SBTAllocatorDesc& desc )               = 0;

    virtual void executeCommandLists( const std::vector<ICommandList*>& lists, QueueType workingQueue, Fence& frameFence ) = 0;
    virtual void waitForFrame( const Fence& frameFence, QueueType workingQueue )                                           = 0;
    virtual void waitForQueue( QueueType workingQueue, QueueType dstQueue )                                                = 0;
    virtual void queueWaitIdle( QueueType workingQueue, Fence& frameFence )                                                = 0;
    virtual bool waitIdle()                                                                                                = 0;

    virtual bool          queryFeatureSupport( Feature feature, void* pInfo = nullptr, size_t infoSize = 0 ) const = 0;
    virtual FormatSupport queryFormatSupport( Format format ) const                                                = 0;
    virtual API           getGraphicsAPI()                                                                         = 0;

protected:
    virtual void checkExtensions() = 0;
};

} // namespace Graphics::RHI

AXION_NAMESPACE_END
