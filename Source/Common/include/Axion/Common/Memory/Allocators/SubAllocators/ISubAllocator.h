#pragma once
#include <Axion/Common/Memory/Allocators/IAllocator.h>

AXION_NAMESPACE_BEGIN
namespace Memory {

// Visibility Policies
struct VisibilityHostOnly {
    static constexpr bool isCPUReadable = true;
    static constexpr bool isGPUVisible  = false;
};

struct VisibilityDeviceOnly {
    static constexpr bool isCPUReadable = false;
    static constexpr bool isGPUVisible  = true;
};

struct VisibilityShared {
    static constexpr bool isCPUReadable = true;
    static constexpr bool isGPUVisible  = true;
};

// Represents a logical slice of ANY container (CPU array, GPU buffer, File, etc.)
template <typename TContainer>
struct SubAllocation {
    TContainer* container = nullptr;

    u64 size   = 0;
    u64 stride = 0;
    u64 count  = 0;

    u64 offset  = 0;
    u32 padding = 0;

    u64   gpuAddress = 0;
    byte* cpuAddress = nullptr;

    bool isValid() const { return container != nullptr && size > 0; }
};

template <typename TContainer, typename VisibilityPolicy>
class ISubAllocator
{
public:
    virtual ~ISubAllocator() = default;

    virtual SubAllocation<TContainer> allocate( u64 size, u64 alignment = 256 )     = 0;
    virtual void                      free( SubAllocation<TContainer>& allocation ) = 0;
    virtual void                      reset()                                       = 0;

    virtual u64 getCapacity() const = 0;
    virtual u64 getUsed() const     = 0;

    const VisibilityPolicy& getVisibilityPolicy()  const { return _visibilityPolicy; }

protected:
    VisibilityPolicy _visibilityPolicy {};
};

} // namespace Memory
AXION_NAMESPACE_END