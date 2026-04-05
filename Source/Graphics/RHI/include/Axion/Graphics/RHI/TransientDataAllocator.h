#pragma once
#include "Axion/Graphics/RHI/Common.h"
#include "Axion/Graphics/RHI/IResource.h"
#include <Axion/Common/Memory/Allocators/SubAllocators/ISubAllocator.h>

AXION_NAMESPACE_BEGIN

namespace Graphics::RHI {

class IDevice;

/**
 * @brief Helper class for managing transient memory for a single frame (Scratch & Upload heaps).
 * Automatically resets at the start of the frame. Useful for data batching.
 *
 * - Scratch allocations are typically GPU-only and used for temporary buffers accessed by the GPU.
 *
 * - Upload allocations are CPU-visible and used for staging data to be uploaded to the GPU.
 */
class TransientDataAllocator
{
public:
    struct Description {
        u64      scratchSize = MBYTES( 64 );
        u64      uploadSize  = MBYTES( 64 );
        String64 debugName;
    };

    TransientDataAllocator() = default;
    TransientDataAllocator( IDevice* device, const Description& desc );
    ~TransientDataAllocator();

    AXION_ENABLE_MOVE( TransientDataAllocator )
    AXION_DISABLE_COPY( TransientDataAllocator )

    void initialize( IDevice* device, const Description& desc );

    BufferSlice allocateScratch( u64 size, u64 alignment = 256 );
    BufferSlice allocateUpload( u64 size, u64 alignment = 256 );

    void reset();

    void       setDebugName( StringView name );
    StringView getDebugName() const;

    const Description& getDescription() const;

private:
    Description _desc;

    BufferOwnerPtr                                      _scratchBuffer = nullptr;
    BufferLinearAllocator<Memory::VisibilityDeviceOnly> _scratchAllocator;

    BufferOwnerPtr                                  _uploadBuffer = nullptr;
    BufferLinearAllocator<Memory::VisibilityShared> _uploadAllocator;

    bool _initialized = false;
};

typedef TransientDataAllocator::Description TransientDataAllocatorDesc;
} // namespace Graphics::RHI

AXION_NAMESPACE_END
