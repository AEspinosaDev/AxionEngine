#pragma once
#include "Axion/Graphics/RHI/Common.h"

AXION_NAMESPACE_BEGIN

namespace Graphics::RHI {

#pragma region Time Queries

DEFINE_OWNER_PTR_FOR_TYPE( ITimeQueryPool, TimeQueryPool )

/// @brief GPU clock queries of the graphics queue, one range per frame in flight. Written and resolved through ICommandList
class ITimeQueryPool : public IDeviceObject
{
public:
    virtual ~ITimeQueryPool() = default;

    struct Description {
        u32      capacity  = 128; ///< Queries per frame
        u32      numFrames = 1;   ///< Frames in flight
        String64 debugName;
    };
    virtual const Description& getDescription() const = 0;

    /// @brief Clock ticks per second
    virtual u64 getFrequency() const = 0;

    /// @brief Copies the ticks resolved by a frame. Its submission must be finished (wait on its fence first)
    virtual void readResults( u32 frame, u32 count, u64* outTicks ) = 0;
};

using TimeQueryPoolDesc = ITimeQueryPool::Description;

#pragma endregion
#pragma region Occlusion Queries

/// @brief TBD
class IOcclusionQueryPool : public IDeviceObject
{
public:
    virtual ~IOcclusionQueryPool() = default;
};

#pragma endregion

} // namespace Graphics::RHI

AXION_NAMESPACE_END
