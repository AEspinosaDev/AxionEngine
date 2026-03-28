#include "Axion/Graphics/RHI/IDevice.h"
#include "Axion/Graphics/RHI/Memory.h"

AXION_NAMESPACE_BEGIN
namespace Graphics::RHI {

class TransientAllocator : public ITransientAllocator
{
public:
    TransientAllocator( IDevice* device, const Description& desc );
    ~TransientAllocator();

    BufferView allocateScratch( ulong size, ulong alignment ) override;
    BufferView allocateUpload( ulong size, ulong alignment ) override;
    void       reset() override;

    const Description& getDescription() const override { return _desc; }

    NativeObject       getNativeObject( ObjectType /*type*/ ) override { return nullptr; }
    void               setDebugName( const std::string& name ) override { _desc.debugName = name; }
    const std::string& getDebugName() const override { return _desc.debugName; };
    std::string        toString() const override { return _desc.debugName; }

private:
    Description _desc;

    BufferOwnerPtr  _scratchBuffer;
    LinearAllocator _scratchAllocator;

    BufferOwnerPtr  _uploadBuffer;
    LinearAllocator _uploadAllocator;
};

} // namespace Graphics::RHI
AXION_NAMESPACE_END