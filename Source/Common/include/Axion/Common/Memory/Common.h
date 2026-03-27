#pragma once
#include <Axion/Common/Common.h>
#include <memory>
#include <mutex>

AXION_NAMESPACE_BEGIN
namespace Memory {

using MemoryAddress = std::uintptr_t;


// template <typename T>
// class OwnerPtr;

// #define DEFINE_SHARED_PTR_FOR_TYPE( type, clean ) \
//     class type;                                   \
//     typedef std::shared_ptr<type> clean##Ptr;

// #define NEW_S( type ) \
//     std::make_shared<type>

// #define DEFINE_OWNER_PTR_FOR_TYPE( type, clean ) \
//     class type;                                   \
//     Axion::Memory::OwnerPtr<type> clean##Ptr;


} // namespace Memory
AXION_NAMESPACE_END