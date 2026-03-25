#pragma once

#include <sys/mman.h>
#include <unistd.h>

AXION_NAMESPACE_BEGIN
namespace Memory {

inline uint MemoryManager::getPageSize() {
    return static_cast<uint>(sysconf(_SC_PAGESIZE));
}

inline uint MemoryManager::alignToPageSize(uint size) {
    uint pageSize = getPageSize();
    return (size + pageSize - 1) & ~(pageSize - 1);
}

inline MemoryView MemoryManager::virtualAlloc(uint size) {
    if (size == 0) return {};

    uint alignedSize = alignToPageSize(size);
    
    // MAP_ANONYMOUS means it's not backed by a file
    void* ptr = mmap(nullptr, alignedSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    
    if (ptr == MAP_FAILED) {
        return {};
    }
    
    return { ptr, alignedSize };
}

inline void MemoryManager::virtualFree(MemoryView view) {
    if (view.isValid()) {
        // munmap requires the exact size that was mapped
        munmap(view.ptr, view.size);
    }
}

} // namespace Memory
AXION_NAMESPACE_END