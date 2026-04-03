#include "Axion/Graphics/RHI/IDevice.h"
#include "Axion/Graphics/RHI/Memory.h"

AXION_NAMESPACE_BEGIN
namespace Graphics::RHI {

class TransientAllocator : public ITransientAllocator
{
public:
    TransientAllocator( IDevice* device, const Description& desc );
    ~TransientAllocator();

    BufferView allocateScratch( u64 size, u64 alignment ) override;
    BufferView allocateUpload( u64 size, u64 alignment ) override;
    void       reset() override;

    const Description& getDescription() const override { return _desc; }

    NativeObject getNativeObject( ObjectType /*type*/ ) override { return nullptr; }
    void         setDebugName( StringView name ) override;
    StringView   getDebugName() const override;
    STLW::String toString() const override;

private:
    Description _desc;

    BufferOwnerPtr  _scratchBuffer;
    LinearAllocator _scratchAllocator;

    BufferOwnerPtr  _uploadBuffer;
    LinearAllocator _uploadAllocator;
};

} // namespace Graphics::RHI
AXION_NAMESPACE_END