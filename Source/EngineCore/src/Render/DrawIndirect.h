#pragma once
#include "Axion/Graphics/RHI/Memory.h"

AXION_NAMESPACE_BEGIN

namespace Core::Render {

// -----------------------------------------------------------------------------
// INDIRECT RENDERING
// -----------------------------------------------------------------------------

struct IndirectDrawBatch {
    u32 archetypeID;
    u32 topologyID;
    u32 bufferOffset;
    u32 drawCount;
};

struct IndirectCommandPayload {
    Graphics::BufferSlice      commandBufferSlice;
    Graphics::BufferSlice      batchMapSlice;
    STLW::Vector<IndirectDrawBatch> batches;
    bool                            dirty = true;

    struct Cache {
        STLW::Vector<Graphics::RHI::DrawIndexedIndirectCommand> commands;
        STLW::Vector<u32>                                       batchMap;
    };
};

} // namespace Core::Render

AXION_NAMESPACE_END