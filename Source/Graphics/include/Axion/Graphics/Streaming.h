// #pragma once
// #include "Axion/Graphics/RHI/Common.h"
// #include "Axion/Graphics/RHI/IResource.h"
// #include <Axion/Common/Memory/Allocators/SubAllocators/ISubAllocator.h>

// AXION_NAMESPACE_BEGIN

// namespace Graphics {


// using BufferSlice = Memory::SubAllocation<IBuffer>;

// DEFINE_OWNER_PTR_FOR_TYPE( ITransientAllocator, TransientAllocator )
// /**
//  * @brief Manages transient memory for a single frame (Scratch & Upload heaps).
//  * Automatically resets at the start of the frame. Useful for data streaming
//  */
// class ITransientAllocator : public IDeviceObject
// {
// public:
//     struct Description {
//         u64      scratchSize = 64 * 1024 * 1024;
//         u64      uploadSize  = 64 * 1024 * 1024;
//         String64 debugName;
//     };

//     virtual ~ITransientAllocator() = default;

//     virtual BufferSlice allocateScratch( u64 size, u64 alignment = 256 ) = 0;
//     virtual BufferSlice allocateUpload( u64 size, u64 alignment = 256 )  = 0;

//     virtual void reset() = 0;

//     virtual const Description& getDescription() const = 0;
// };

// typedef ITransientAllocator::Description TransientAllocatorDesc;
// } // namespace Graphics::RHI

// AXION_NAMESPACE_END
