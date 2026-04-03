#pragma once
#include <Axion/Common/Common.h>
#include <Axion/Common/Logging.h>
#include <memory>
#include <mutex>

AXION_NAMESPACE_BEGIN
namespace Memory {

using VMemoryAddress = std::uintptr_t;

#define AXION_MEMORY_MINIMUM_ALIGNMENT 8

// Represents a raw block of memory from the OS
struct VMemoryView {
    void* ptr  = nullptr;
    u32  size = 0;
    bool  isValid() const { return ptr != nullptr && size > 0; }
};


} // namespace Memory
AXION_NAMESPACE_END