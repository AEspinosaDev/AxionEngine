// #pragma once
// #include "MemoryManager.h"
// #include "Allocator.h"
// #include <type_traits>

// AXION_NAMESPACE_BEGIN
// namespace Memory {

// template <typename T>
// struct STLPoolAdapter {
//     using value_type = T;

//     IAllocator* allocator = nullptr;

//     STLPoolAdapter() = default;

//     // Explicit constructor to inject our custom allocator
//     STLPoolAdapter( IAllocator* alloc )
//         : allocator( alloc ) {}

//     // Templated copy constructor required by the standard
//     template <class U>
//     constexpr STLPoolAdapter( const STLPoolAdapter<U>& other ) noexcept {
//         allocator = other.allocator;
//     }

//     T* allocate( std::size_t n ) {
//         if ( !allocator )
//         {
//             // Fallback or assert if allocator is null
//             return static_cast<T*>( ::operator new( n * sizeof( T ) ) );
//         }
//         return static_cast<T*>( allocator->allocate( n * sizeof( T ), alignof( T ) ) );
//     }

//     void deallocate( T* p, std::size_t n ) noexcept {
//         if ( !allocator )
//         {
//             ::operator delete( p );
//             return;
//         }
//         allocator->free( p );
//     }
// };

// // Required equality operators for the STL concept
// template <class T, class U>
// bool operator==( const STLPoolAdapter<T>& a, const STLPoolAdapter<U>& b ) {
//     return a.allocator == b.allocator;
// }

// template <class T, class U>
// bool operator!=( const STLPoolAdapter<T>& a, const STLPoolAdapter<U>& b ) {
//     return a.allocator != b.allocator;
// }

// } // namespace Memory
// AXION_NAMESPACE_END