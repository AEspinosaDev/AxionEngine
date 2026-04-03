#include "Axion/Graphics/RHI/IDevice.h"
#include "Axion/Graphics/RHI/Memory.h"

AXION_NAMESPACE_BEGIN
namespace Graphics::RHI {

class TransientAllocator : public ITransientAllocator
{
public:
    TransientAllocator( IDevice* device, const Description& desc );
    ~TransientAllocator();

    BufferSlice allocateScratch( u64 size, u64 alignment ) override;
    BufferSlice allocateUpload( u64 size, u64 alignment ) override;
    void        reset() override;

    const Description& getDescription() const override { return _desc; }

    NativeObject getNativeObject( ObjectType /*type*/ ) override { return nullptr; }
    void         setDebugName( StringView name ) override;
    StringView   getDebugName() const override;
    STLW::String toString() const override;

private:
    Description _desc;

    BufferOwnerPtr          _scratchBuffer;
    BufferLinearAllocator<> _scratchAllocator;

    BufferOwnerPtr          _uploadBuffer;
    BufferLinearAllocator<> _uploadAllocator;
};

} // namespace Graphics::RHI
AXION_NAMESPACE_END