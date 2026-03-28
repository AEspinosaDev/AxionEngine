#pragma once
#include <Axion/Common/Memory/STLAdapter.h>
#include <unordered_map>

AXION_NAMESPACE_BEGIN

namespace STLW {

// Unordered Map wrapper
template <typename K, typename V, typename Hash = std::hash<K>, typename KeyEqual = std::equal_to<K>>
class UnorderedMap : public std::unordered_map<K, V, Hash, KeyEqual, Memory::STLAdapter<std::pair<const K, V>>>
{
public:
    using Base = std::unordered_map<K, V, Hash, KeyEqual, Memory::STLAdapter<std::pair<const K, V>>>;
    using Base::Base;

    explicit UnorderedMap( Memory::IAllocator* allocator )
        : Base( 0, Hash(), KeyEqual(), Memory::STLAdapter<std::pair<const K, V>>( allocator ) ) {}
    explicit UnorderedMap()
        : Base( 0, Hash(), KeyEqual(), Memory::STLAdapter<std::pair<const K, V>>( nullptr ) ) {}
};

// Unordered Multimap wrapper
template <typename K, typename V, typename Hash = std::hash<K>, typename KeyEqual = std::equal_to<K>>
class UnorderedMultimap : public std::unordered_multimap<K, V, Hash, KeyEqual, Memory::STLAdapter<std::pair<const K, V>>>
{
public:
    using Base = std::unordered_multimap<K, V, Hash, KeyEqual, Memory::STLAdapter<std::pair<const K, V>>>;
    using Base::Base;

    explicit UnorderedMultimap( Memory::IAllocator* allocator )
        : Base( 0, Hash(), KeyEqual(), Memory::STLAdapter<std::pair<const K, V>>( allocator ) ) {}
    explicit UnorderedMultimap()
        : Base( 0, Hash(), KeyEqual(), Memory::STLAdapter<std::pair<const K, V>>( nullptr ) ) {}
};

} // namespace STLW

AXION_NAMESPACE_END