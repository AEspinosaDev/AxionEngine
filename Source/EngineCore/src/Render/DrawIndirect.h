#pragma once
#include "Axion/Graphics/RHI/Common.h"

AXION_NAMESPACE_BEGIN

namespace Core::Render {

// -----------------------------------------------------------------------------
// INDIRECT RENDERING
// -----------------------------------------------------------------------------

struct IndirectDrawBatch {
    u32 psoBundleID;  ///< MaterialLibrary pipeline bundle (archetype + render state)
    u32 bufferOffset; ///< Bytes, relative to the start of the command list
    u32 drawCount;
};

struct IndirectCommandPayload {
    Graphics::BufferSlice           commandBufferSlice; ///< DispatchMeshIndirectCommand list (CPU upload)
    Graphics::BufferSlice           batchMapSlice;      ///< Sorted instance -> command (GPU culling only)
    STLW::Vector<IndirectDrawBatch> batches;
};

} // namespace Core::Render

AXION_NAMESPACE_END