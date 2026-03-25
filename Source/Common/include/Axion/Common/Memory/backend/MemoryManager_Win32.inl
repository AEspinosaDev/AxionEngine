#pragma once
#include "Axion/Common/Defines.h"

AXION_NAMESPACE_BEGIN
namespace Memory {
    
inline VMemView VMemManager::virtualReserve(uint size) {
    if (size == 0) return {};
    uint alignedSize = alignToPageSize(size);
    // Only RESERVE, no COMMIT
    void* ptr = VirtualAlloc(nullptr, alignedSize, MEM_RESERVE, PAGE_NOACCESS);
    return { ptr, ptr ? alignedSize : 0 };
}

inline bool VMemManager::virtualCommit(VMemView view) {
    if (!view.isValid()) return false;
    // Commit the memory that was already reserved
    void* ptr = VirtualAlloc(view.ptr, view.size, MEM_COMMIT, PAGE_READWRITE);
    return ptr != nullptr;
}

inline void VMemManager::virtualDecommit(VMemView view) {
    if (view.isValid()) {
        // Frees physical RAM. Size must be explicitly passed.
        VirtualFree(view.ptr, view.size, MEM_DECOMMIT);
    }
}

inline void VMemManager::virtualRelease(VMemView view) {
    if (view.isValid()) {
        // Frees virtual address space. Size MUST be 0.
        VirtualFree(view.ptr, 0, MEM_RELEASE);
    }
}
} // namespace Memory
AXION_NAMESPACE_END