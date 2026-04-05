#include <Axion/Graphics/RHI/TransientDataAllocator.h>
#include <Axion/Graphics/RHI/IDevice.h>

AXION_NAMESPACE_BEGIN
namespace Graphics::RHI {

TransientDataAllocator::TransientDataAllocator( IDevice* device, const Description& desc ){
    initialize( device, desc );
}

void TransientDataAllocator::initialize( IDevice* device, const Description& desc ) {
    if( _initialized )
    {
        AXION_LOG_WARN( Logger::Module::RHI, "TransientDataAllocator [{}] is already initialized.", _desc.debugName );
        return;
    }
    _desc = desc;

    // 1. SCRATCH (GPU Only / UAV)
    {
        BufferDesc bDesc = {};
        bDesc.size       = desc.scratchSize;
        bDesc.memoryType = MemoryUsage::GPUOnly;
        bDesc.viewFlags  = BufferViewUnorderedAccess;
        bDesc.debugName  = desc.debugName + "_ScratchBuffer";
        _scratchBuffer   = device->createBuffer( bDesc );
        _scratchAllocator.initialize( _scratchBuffer.get() );
    }

    // 2. UPLOAD (CPU Visible)
    {
        BufferDesc bDesc = {};
        bDesc.size       = desc.uploadSize;
        bDesc.memoryType = MemoryUsage::CPUVisible;
        bDesc.viewFlags  = BufferViewNone;
        bDesc.debugName  = desc.debugName + "_UploadBuffer";
        _uploadBuffer    = device->createBuffer( bDesc );
        _uploadBuffer->map();
        _uploadAllocator.initialize( _uploadBuffer.get() );
    }
    _initialized = true;
}

TransientDataAllocator::~TransientDataAllocator() {
    if ( _uploadBuffer )
        _uploadBuffer->unmap();
}

BufferSlice TransientDataAllocator::allocateScratch( u64 size, u64 alignment ) {
    return _scratchAllocator.allocate( size, alignment );
}

BufferSlice TransientDataAllocator::allocateUpload( u64 size, u64 alignment ) {
    return _uploadAllocator.allocate( size, alignment );
}

void TransientDataAllocator::reset() {
    _scratchAllocator.reset();
    _uploadAllocator.reset();
}

void TransientDataAllocator::setDebugName( StringView name ) {
}

StringView TransientDataAllocator::getDebugName() const {
    return StringView();
}

} // namespace Graphics::RHI
AXION_NAMESPACE_END