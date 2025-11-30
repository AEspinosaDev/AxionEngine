#pragma once
#include "Axion/Common/Math.h"
#include "Axion/Graphics/RHI/Common.h"
#include "Axion/Graphics/RHI/Resource.h"

AXION_NAMESPACE_BEGIN

namespace Graphics::RHI {

struct DescriptorBinding {
    uint           binding = 0; // register(t#, b#, s#, etc.)
    DescriptorType type;
    ShaderStage    stageMask = ShaderStage::Vertex | ShaderStage::Pixel;
    uint           arraySize = 1;
};

struct DescriptorLayoutDesc {
    std::vector<DescriptorBinding> bindings;
};

DEFINE_COM_PTR_FOR_TYPE( IDescriptorSet, DescriptorSet )

class IDescriptorSet : public IResource
{
public:
    virtual ~IDescriptorSet() = default;

    virtual void bind( uint binding, ITexture* tex, ResourceState usage ) = 0;
    virtual void bind( uint binding, IBuffer* buf, ResourceState usage )  = 0;
};

DEFINE_COM_PTR_FOR_TYPE( IDescriptorAllocator, DescriptorAllocator )
class IPipelineLayout;

class IDescriptorAllocator : public IResource
{
public:
    struct Description {
        uint        numDescriptors = 256;
        std::string debugName;
    };

    virtual ~IDescriptorAllocator() = default;

    virtual IDescriptorSet* allocate( IPipelineLayout* layout, uint setIndex ) = 0;
    virtual void            reset()                                            = 0;

    virtual const IDescriptorAllocator::Description& getDescription() const = 0;
};

typedef IDescriptorAllocator::Description DescriptorAllocatorDesc;

} // namespace Graphics::RHI

AXION_NAMESPACE_END
