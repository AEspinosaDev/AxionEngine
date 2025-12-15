#pragma once
#include "Axion/Common/Math.h"
#include "Axion/Graphics/RHI/Common.h"
#include "Axion/Graphics/RHI/Pipeline.h"
#include "Axion/Graphics/RHI/Resource.h"
#include "Axion/Graphics/RHI/ShaderBindingTable.h"

AXION_NAMESPACE_BEGIN

namespace Graphics::RHI {

DEFINE_COM_PTR_FOR_TYPE( ICommandList, CommandList )

class ICommandList : public IResource
{
public:
    struct Description {
        QueueType   queueType;
        uint        numFrames = 1;
        std::string debugName = "";
    };

    virtual ~ICommandList() = default;

    virtual void               begin()                       = 0;
    virtual void               end()                         = 0;
    virtual void               setCurrentFrame( uint index ) = 0;
    virtual uint               getCurrentFrame() const       = 0;
    virtual const Description& getDescription() const        = 0;

    virtual void barrier( ITexture* texture, ResourceState newState )                                               = 0;
    virtual void barrier( IBuffer* buffer, ResourceState newState )                                                 = 0;
    virtual void clearTexture( ITexture* texture, const ClearValue& clearValue )                                    = 0;
    virtual void copyBuffer( IBuffer* dst, IBuffer* src, ulong numBytes, ulong dstOffset = 0, ulong srcOffset = 0 ) = 0;
    virtual void copyTexture( ITexture* dst, ITexture* src )                                                        = 0;

    virtual void bindComputePipeline( IComputePipeline* pipeline )       = 0;
    virtual void bindGraphicPipeline( IGraphicPipeline* pipeline )       = 0;
    virtual void bindDescriptorSet( uint setIndex, IDescriptorSet* set ) = 0;

    virtual void dispatch( const Extent3D& gridSize )                                             = 0;
    virtual void dispatchRays( const SBT::BufferView& sbtBufferView, const Extent3D& screenSize ) = 0;

    virtual void beginRendering( const RenderingDesc& info )                                                    = 0;
    virtual void endRendering()                                                                                 = 0;
    virtual void draw( uint vertexCount, uint instanceCount = 1, uint firstVertex = 0, uint firstInstance = 0 ) = 0;
    virtual void drawIndexed( uint indexCount,
                              uint instanceCount = 1,
                              uint firstIndex    = 0,
                              int  vertexOffset  = 0,
                              uint firstInstance = 0 )                                                          = 0;
    virtual void bindVertexBuffer( uint slot, IBuffer* buffer )                                                 = 0;
    virtual void bindIndexBuffer( IBuffer* buffer )                                                             = 0;

    template <typename T>
    void pushConstants( uint rootIndex, const T& data, uint offset32Bit = 0 ) {
        static_assert( sizeof( T ) % 4 == 0, "Push Constant struct size must be 4-byte aligned" );
        auto size = sizeof( T ) / 4;
        pushConstants( rootIndex, &data, size, offset32Bit );
    }

protected:
    enum class PipelineBindPoint : uchar
    {
        None,
        Compute,
        Graphic
    };

    virtual void pushConstants( uint setIndex, const void* data, uint numValues32Bit, uint offset32Bit = 0 ) = 0;
};

typedef ICommandList::Description CommandListDesc;

} // namespace Graphics::RHI

AXION_NAMESPACE_END