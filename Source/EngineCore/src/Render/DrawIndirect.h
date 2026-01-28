#pragma once
#include "Axion/Graphics/RHI/Memory.hpp"

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
    Graphics::RHI::BufferView      cmdBufferView;
    Graphics::RHI::BufferView      batchMapView;
    std::vector<IndirectDrawBatch> batches;
};

} // namespace Core::Render

AXION_NAMESPACE_END