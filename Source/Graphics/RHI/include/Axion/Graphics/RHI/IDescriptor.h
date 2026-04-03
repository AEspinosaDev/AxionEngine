#pragma once
#include "Axion/Common/Math.h"
#include "Axion/Graphics/RHI/Common.h"
#include "Axion/Graphics/RHI/IResource.h"
#include "Axion/Graphics/RHI/Memory.h"

AXION_NAMESPACE_BEGIN

namespace Graphics::RHI {

constexpr u32 UNBOUNDED_DESCRIPTOR_ARRAY = 0xFFFFFFFF;

struct DescriptorBinding {
    u32            binding = 0; // register(t#, b#, s#, etc.)
    DescriptorType type;
    ShaderStage    stageMask = ShaderStage::Vertex | ShaderStage::Pixel;
    u32            arraySize = 1; ///< -1 for unbounded
};

struct DescriptorLayoutDesc {
    STLW::Vector<DescriptorBinding> bindings;
};

DEFINE_OWNER_PTR_FOR_TYPE( IDescriptorSet, DescriptorSet )

class IDescriptorSet : public IDeviceObject
{
public:
    virtual ~IDescriptorSet() = default;

    virtual void attach( u32 binding, ITexture* tex, ResourceState bindingState )                                          = 0;
    virtual void attach( u32 binding, IBuffer* buf, ResourceState bindingState )                                           = 0;
    virtual void attach( u32 binding, ISampler* samp )                                                                     = 0;
    virtual void attach( u32 binding, IAccel* accel )                                                                      = 0;
    virtual void attachDynamic( u32 binding, IBuffer* buf, u64 offset, u64 range, u32 stride, ResourceState bindingState ) = 0;
    virtual void attachBufferSlice( u32 binding, const BufferSlice& bufferView, ResourceState bindingState )               = 0;

    // Bindless Workflow
    virtual void attachBindless( u32 binding, u32 arrayIndex, ITexture* tex, ResourceState bindingState )                                     = 0;
    virtual void attachBindlessArray( u32 binding, u32 startArrayIndex, const STLW::Vector<ITexture*>& textures, ResourceState bindingState ) = 0;
    virtual void attachBindless( u32 binding, u32 arrayIndex, IBuffer* buf, ResourceState bindingState )                                      = 0;
    virtual void attachBindlessArray( u32 binding, u32 startArrayIndex, const STLW::Vector<IBuffer*>& buffers, ResourceState bindingState )   = 0;
    virtual void attachBindless( u32 binding, u32 arrayIndex, ISampler* samp )                                                                = 0;
    virtual void attachBindlessArray( u32 binding, u32 startArrayIndex, const STLW::Vector<ISampler*>& samplers )                             = 0;
    virtual void attachBindless( u32 binding, u32 arrayIndex, IAccel* accel )                                                                 = 0;
    virtual void attachBindlessArray( u32 binding, u32 startArrayIndex, const STLW::Vector<IAccel*>& accels )                                 = 0;
};

DEFINE_OWNER_PTR_FOR_TYPE( IDescriptorAllocator, DescriptorAllocator )
class IPipelineLayout;

class IDescriptorAllocator : public IDeviceObject
{
public:
    struct Description {
        u32      numDescriptors = 256;
        u32      numViews       = 256;
        u32      numSamplers    = 64;
        String64 debugName      = "";
    };

    virtual ~IDescriptorAllocator() = default;

    virtual IDescriptorSet* allocate( IPipelineLayout* layout, u32 setIndex ) = 0;
    virtual void            reset()                                           = 0;

    virtual const IDescriptorAllocator::Description& getDescription() const = 0;

    virtual void lockPersistent()   = 0;
    virtual void unlockPersistent() = 0;
};

typedef IDescriptorAllocator::Description DescriptorAllocatorDesc;

} // namespace Graphics::RHI

AXION_NAMESPACE_END
