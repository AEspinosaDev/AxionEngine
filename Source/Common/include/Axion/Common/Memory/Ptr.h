#pragma once
#include "Axion/Common/Defines.h"
#include "Axion/Memory/MemoryCore.h"
#include <atomic>
#include <utility>

AXION_NAMESPACE_BEGIN
namespace Memory {

// -----------------------------------------------------------------------------
// OwnerPtr (Equivalent to std::unique_ptr)
// Move-only semantics. Strictly owns the memory and the lifecycle of the object.
// -----------------------------------------------------------------------------
template <typename T>
class OwnerPtr {
public:
    OwnerPtr() : _ptr(nullptr), _allocator(nullptr) {}
    
    OwnerPtr(T* ptr, IAllocator* allocator) : _ptr(ptr), _allocator(allocator) {}

    ~OwnerPtr() { reset(); }

    // Move constructor
    OwnerPtr(OwnerPtr&& other) noexcept : _ptr(other._ptr), _allocator(other._allocator) {
        other._ptr = nullptr;
        other._allocator = nullptr;
    }

    // Move assignment
    OwnerPtr& operator=(OwnerPtr&& other) noexcept {
        if (this != &other) {
            reset();
            _ptr = other._ptr;
            _allocator = other._allocator;
            other._ptr = nullptr;
            other._allocator = nullptr;
        }
        return *this;
    }

    // Disable copy
    OwnerPtr(const OwnerPtr&) = delete;
    OwnerPtr& operator=(const OwnerPtr&) = delete;

    T* operator->() const { return _ptr; }
    T& operator*() const { return *_ptr; }
    T* get() const { return _ptr; }
    explicit operator bool() const { return _ptr != nullptr; }

    void reset() {
        if (_ptr && _allocator) {
            _ptr->~T(); 
            _allocator->free(_ptr);
        }
        _ptr = nullptr;
        _allocator = nullptr;
    }

    T* release() {
        T* temp = _ptr;
        _ptr = nullptr;
        _allocator = nullptr;
        return temp;
    }

private:
    T* _ptr;
    IAllocator* _allocator;
};

// -----------------------------------------------------------------------------
// SharedPtr Control Block
// -----------------------------------------------------------------------------
template <typename T>
struct SharedControlBlock {
    std::atomic<uint32_t> refCount;
    IAllocator* allocator;
    
    // We allocate raw bytes for the payload to avoid default construction
    // and manually construct the object here using placement new.
    alignas(T) unsigned char payload[sizeof(T)];

    T* getPayload() {
        return reinterpret_cast<T*>(payload);
    }
};

// -----------------------------------------------------------------------------
// SharedPtr (Equivalent to std::shared_ptr)
// Reference counted ownership.
// -----------------------------------------------------------------------------
template <typename T>
class SharedPtr {
public:
    SharedPtr() : _ptr(nullptr), _controlBlock(nullptr) {}

    // Constructor used by MakeShared
    SharedPtr(T* ptr, SharedControlBlock<T>* block) : _ptr(ptr), _controlBlock(block) {
        if (_controlBlock) {
            _controlBlock->refCount.fetch_add(1, std::memory_order_relaxed);
        }
    }

    ~SharedPtr() { release(); }

    // Copy constructor
    SharedPtr(const SharedPtr& other) : _ptr(other._ptr), _controlBlock(other._controlBlock) {
        if (_controlBlock) {
            _controlBlock->refCount.fetch_add(1, std::memory_order_relaxed);
        }
    }

    // Copy assignment
    SharedPtr& operator=(const SharedPtr& other) {
        if (this != &other) {
            release();
            _ptr = other._ptr;
            _controlBlock = other._controlBlock;
            if (_controlBlock) {
                _controlBlock->refCount.fetch_add(1, std::memory_order_relaxed);
            }
        }
        return *this;
    }

    // Move constructor
    SharedPtr(SharedPtr&& other) noexcept : _ptr(other._ptr), _controlBlock(other._controlBlock) {
        other._ptr = nullptr;
        other._controlBlock = nullptr;
    }

    // Move assignment
    SharedPtr& operator=(SharedPtr&& other) noexcept {
        if (this != &other) {
            release();
            _ptr = other._ptr;
            _controlBlock = other._controlBlock;
            other._ptr = nullptr;
            other._controlBlock = nullptr;
        }
        return *this;
    }

    T* operator->() const { return _ptr; }
    T& operator*() const { return *_ptr; }
    T* get() const { return _ptr; }
    explicit operator bool() const { return _ptr != nullptr; }
    
    uint32_t use_count() const {
        return _controlBlock ? _controlBlock->refCount.load(std::memory_order_relaxed) : 0;
    }

private:
    void release() {
        if (_controlBlock) {
            // Decrement the reference count. If it reaches 1 (meaning it's about to be 0),
            // we are the last owner and must destroy the object.
            if (_controlBlock->refCount.fetch_sub(1, std::memory_order_acq_rel) == 1) {
                _ptr->~T();
                
                IAllocator* alloc = _controlBlock->allocator;
                // Destroy the control block itself
                _controlBlock->~SharedControlBlock<T>();
                // Free the entire memory chunk using the stored allocator
                alloc->free(_controlBlock);
            }
        }
        _ptr = nullptr;
        _controlBlock = nullptr;
    }

    T* _ptr;
    SharedControlBlock<T>* _controlBlock;
};

// -----------------------------------------------------------------------------
// Factory Functions
// -----------------------------------------------------------------------------

template<typename T, typename... Args>
OwnerPtr<T> MakeOwned(IAllocator* allocator, Args&&... args) {
    if (!allocator) return OwnerPtr<T>();
    
    void* mem = allocator->allocate(sizeof(T), alignof(T));
    T* obj = new(mem) T(std::forward<Args>(args)...);
    return OwnerPtr<T>(obj, allocator);
}

template<typename T, typename... Args>
SharedPtr<T> MakeShared(IAllocator* allocator, Args&&... args) {
    if (!allocator) return SharedPtr<T>();

    // Allocate memory for both the control block and the object in one go
    void* mem = allocator->allocate(sizeof(SharedControlBlock<T>), alignof(SharedControlBlock<T>));
    
    // Construct the control block
    auto* block = new(mem) SharedControlBlock<T>();
    block->refCount.store(0, std::memory_order_relaxed);
    block->allocator = allocator;

    // Construct the actual object inside the control block's payload array
    T* obj = new(block->getPayload()) T(std::forward<Args>(args)...);

    return SharedPtr<T>(obj, block);
}

} // namespace Memory
AXION_NAMESPACE_END