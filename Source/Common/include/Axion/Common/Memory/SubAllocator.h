#pragma once
#include "Axion/Common/Defines.h"
#include <cstddef>

AXION_NAMESPACE_BEGIN
namespace Memory {

// Visibility Policies
struct HostVisible {
    static constexpr bool isCPUReadable = true;
    static constexpr bool isGPUVisible = false;
};

struct DeviceVisible {
    static constexpr bool isCPUReadable = false;
    static constexpr bool isGPUVisible = true;
};

struct SharedVisibility {
    static constexpr bool isCPUReadable = true;
    static constexpr bool isGPUVisible = true;
};

// Represents a logical slice of ANY container (CPU array, GPU buffer, File, etc.)
template <typename TContainer>
struct SubAllocation {
    TContainer* container = nullptr;
    size_t offset = 0;
    size_t size = 0;

    bool isValid() const { return container != nullptr && size > 0; }
};

// --- The Base Interface ---
template <typename T>
class ISubAllocator {
public:
    virtual ~ISubAllocator() = default;

    virtual SubAllocation<T> allocate(size_t size, size_t alignment = 256) = 0;
    virtual void free(SubAllocation<T>& allocation) = 0;
    virtual void reset() = 0;
    
    virtual size_t getCapacity() const = 0;
    virtual size_t getUsed() const = 0;
};

} // namespace Memory
AXION_NAMESPACE_END