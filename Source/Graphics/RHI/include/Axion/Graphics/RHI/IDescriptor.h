#pragma once
#include "Axion/Common/Math.h"
#include "Axion/Graphics/RHI/Common.h"
#include "Axion/Graphics/RHI/Memory.h"
#include "Axion/Graphics/RHI/IResource.h"

AXION_NAMESPACE_BEGIN

namespace Graphics::RHI {

constexpr uint UNBOUNDED_DESCRIPTOR_ARRAY = 0xFFFFFFFF;

struct DescriptorBinding {
    uint           binding = 0; // register(t#, b#, s#, etc.)
    DescriptorType type;
    ShaderStage    stageMask = ShaderStage::Vertex | ShaderStage::Pixel;
    uint           arraySize = 1; ///< -1 for unbounded
};

struct DescriptorLayoutDesc {
    std::vector<DescriptorBinding> bindings;
};

DEFINE_OWNER_PTR_FOR_TYPE( IDescriptorSet, DescriptorSet )

class IDescriptorSet : public IObject
{
public:
    virtual ~IDescriptorSet() = default;

    virtual void attach( uint binding, ITexture* tex, ResourceState bindingState )                                               = 0;
    virtual void attach( uint binding, IBuffer* buf, ResourceState bindingState )                                                = 0;
    virtual void attach( uint binding, ISampler* samp )                                                                          = 0;
    virtual void attach( uint binding, IAccel* accel )                                                                           = 0;
    virtual void attachDynamic( uint binding, IBuffer* buf, ulong offset, ulong range, uint stride, ResourceState bindingState ) = 0;
    virtual void attachBufferView( uint binding, const BufferView& bufferView, ResourceState bindingState )                      = 0;

    // Bindless Workflow
    virtual void attachBindless( uint binding, uint arrayIndex, ITexture* tex, ResourceState bindingState )                                    = 0;
    virtual void attachBindlessArray( uint binding, uint startArrayIndex, const std::vector<ITexture*>& textures, ResourceState bindingState ) = 0;
    virtual void attachBindless( uint binding, uint arrayIndex, IBuffer* buf, ResourceState bindingState )                                     = 0;
    virtual void attachBindlessArray( uint binding, uint startArrayIndex, const std::vector<IBuffer*>& buffers, ResourceState bindingState )   = 0;
    virtual void attachBindless( uint binding, uint arrayIndex, ISampler* samp )                                                               = 0;
    virtual void attachBindlessArray( uint binding, uint startArrayIndex, const std::vector<ISampler*>& samplers )                             = 0;
    virtual void attachBindless( uint binding, uint arrayIndex, IAccel* accel )                                                                = 0;
    virtual void attachBindlessArray( uint binding, uint startArrayIndex, const std::vector<IAccel*>& accels )                                 = 0;
};

DEFINE_OWNER_PTR_FOR_TYPE( IDescriptorAllocator, DescriptorAllocator )
class IPipelineLayout;

class IDescriptorAllocator : public IObject
{
public:
    struct Description {
        uint        numDescriptors = 256;
        uint        numViews       = 256;
        uint        numSamplers    = 64;
        std::string debugName;
    };

    virtual ~IDescriptorAllocator() = default;

    virtual IDescriptorSet* allocate( IPipelineLayout* layout, uint setIndex ) = 0;
    virtual void            reset()                                            = 0;

    virtual const IDescriptorAllocator::Description& getDescription() const = 0;

    virtual void lockPersistent()   = 0;
    virtual void unlockPersistent() = 0;
};

typedef IDescriptorAllocator::Description DescriptorAllocatorDesc;

} // namespace Graphics::RHI

AXION_NAMESPACE_END
