#pragma once
#include "Axion/Common/Math.h"
#include "Axion/Graphics/RHI/Common.h"
#include "Axion/Graphics/RHI/IResource.h"

AXION_NAMESPACE_BEGIN

namespace Graphics::RHI {

constexpr u32 UNBOUNDED_DESCRIPTOR_ARRAY = 0xFFFFFFFF;

struct DescriptorRange {
    u32 base;
    u32 count;
};

struct DescriptorBinding {
    DescriptorRange range = {}; // register(t#, b#, s#, etc.)
    DescriptorType  type;
    ShaderStage     stageMask = ShaderStage::Vertex | ShaderStage::Pixel;
    u32             arraySize = 1; ///< -1 for unbounded
};

struct DescriptorLayoutDesc {
    SmallVector<DescriptorBinding> bindings;
};

DEFINE_OWNER_PTR_FOR_TYPE( IDescriptorSet, DescriptorSet )

class IDescriptorSet : public IDeviceObject
{
public:
    virtual ~IDescriptorSet() = default;

    /**
     * Attach a resource to an specific descriptor binding. Binding number is by hlsl register type, not global
     */
    virtual void attach( u32 regBinding, DescriptorType descType, ITexture* tex )                                          = 0;
    virtual void attach( u32 regBinding, DescriptorType descType, IBuffer* buf )                                           = 0;
    virtual void attach( u32 regBinding, ISampler* samp )                                                                  = 0;
    virtual void attach( u32 regBinding, IAccel* accel )                                                                   = 0;
    virtual void attachDynamic( u32 regBinding, DescriptorType descType, IBuffer* buf, u64 offset, u64 range, u32 stride ) = 0;
    virtual void attachBufferSlice( u32 regBinding, DescriptorType descType, const BufferSlice& bufferView )               = 0;

    // Bindless Workflow
    virtual void attachBindless( u32 regBinding, u32 arrayIndex, DescriptorType descType, ITexture* tex )                                     = 0;
    virtual void attachBindlessArray( u32 regBinding, u32 startArrayIndex, DescriptorType descType, const STLW::Vector<ITexture*>& textures ) = 0;
    virtual void attachBindless( u32 regBinding, u32 arrayIndex, DescriptorType descType, IBuffer* buf )                                      = 0;
    virtual void attachBindlessArray( u32 regBinding, u32 startArrayIndex, DescriptorType descType, const STLW::Vector<IBuffer*>& buffers )   = 0;
    virtual void attachBindless( u32 regBinding, u32 arrayIndex, ISampler* samp )                                                             = 0;
    virtual void attachBindlessArray( u32 regBinding, u32 startArrayIndex, const STLW::Vector<ISampler*>& samplers )                          = 0;
    virtual void attachBindless( u32 regBinding, u32 arrayIndex, IAccel* accel )                                                              = 0;
    virtual void attachBindlessArray( u32 regBinding, u32 startArrayIndex, const STLW::Vector<IAccel*>& accels )                              = 0;
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
