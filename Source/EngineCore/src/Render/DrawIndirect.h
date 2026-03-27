#pragma once
#include "Axion/Graphics/RHI/Memory.h"

AXION_NAMESPACE_BEGIN

namespace Core::Render {

// -----------------------------------------------------------------------------
// INDIRECT RENDERING
// -----------------------------------------------------------------------------

struct IndirectDrawBatch {
    uint archetypeID;
    uint topologyID;
    uint bufferOffset;
    uint drawCount;
};

struct IndirectCommandData {
    Graphics::RHI::BufferView      commandBufferView;
    Graphics::RHI::BufferView      batchMapView;
    std::vector<IndirectDrawBatch> batches;
    bool                           dirty = true;

    struct Cache {
        std::vector<Graphics::RHI::DrawIndexedIndirectCommand> commands;
        std::vector<uint>                                      batchMap;
    };
};

} // namespace Core::Render

AXION_NAMESPACE_END