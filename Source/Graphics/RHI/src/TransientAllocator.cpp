#include "TransientAllocator.h"

AXION_NAMESPACE_BEGIN
namespace Graphics::RHI {

TransientAllocator::TransientAllocator( IDevice* device, const Description& desc )
    : _desc( desc ) {
    // 1. SCRATCH (GPU Only / UAV)
    {
        BufferDesc bDesc  = {};
        bDesc.size        = desc.scratchSize;
        bDesc.memoryType  = MemoryUsage::GPUOnly;
        bDesc.viewFlags   = BufferViewUnorderedAccess;
        bDesc.debugName   = desc.debugName + "_ScratchBuffer";
        _scratchBuffer    = device->createBuffer( bDesc );
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
}

TransientAllocator::~TransientAllocator() {
    if ( _uploadBuffer )
        _uploadBuffer->unmap();
}

BufferView TransientAllocator::allocateScratch( u64 size, u64 alignment ) {
    return _scratchAllocator.allocate( size, alignment );
}

BufferView TransientAllocator::allocateUpload( u64 size, u64 alignment ) {
    return _uploadAllocator.allocate( size, alignment );
}

void TransientAllocator::reset() {
    _scratchAllocator.reset();
    _uploadAllocator.reset();
}

void TransientAllocator::setDebugName( StringView name ) {
}

StringView TransientAllocator::getDebugName() const {
    return StringView();
}

STLW::String TransientAllocator::toString() const {
    return STLW::String();
}

} // namespace Graphics::RHI
AXION_NAMESPACE_END