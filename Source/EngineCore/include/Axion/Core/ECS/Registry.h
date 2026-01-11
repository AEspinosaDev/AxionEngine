#pragma once
#include <Axion/Core/ECS/Entity.h>
#include <Axion/Core/ECS/MultiView.hpp>
#include <Axion/Core/ECS/SparseSet.hpp>
#include <memory>
#include <typeindex>
#include <unordered_map>
#include <vector>
#include <type_traits>

AXION_NAMESPACE_BEGIN

namespace Core::ECS {

class Registry
{
public:
    Registry() = default;
    ~Registry() { clear(); }

    EntityID createEntity() { return _entityCounter++; }

    void destroyEntity( EntityID entity ) {
        for ( auto& [type, pool] : _pools ) {
            if ( pool->has( entity ) ) pool->remove( entity );
        }
    }

    // --- MUTABLE ACCESS ---
    template <typename T>
    T& addComponent( EntityID entity, T component ) {
        return ensurePool<T>()->add( entity, component );
    }

    template <typename T>
    void removeComponent( EntityID entity ) {
        ensurePool<T>()->remove( entity );
    }

    template <typename T>
    T& getComponent( EntityID entity ) {
        return ensurePool<T>()->get( entity );
    }

    // --- CONST ACCESS ---
    template <typename T>
    bool hasComponent( EntityID entity ) const {
        const auto* pool = getPool<T>();
        return pool && pool->has( entity );
    }

    template <typename T>
    const std::vector<T>& view() const {
        const auto* pool = getPool<T>();
        if ( !pool ) {
            static const std::vector<T> empty;
            return empty;
        }
        return pool->getData();
    }

    // --- MULTIVIEW ---
    // getPool<Components>() devolverá el puntero const o mutable según T.
    template <typename... Components>
    MultiView<Components...> multiView() const {
        return MultiView<Components...>( getPool<Components>()... );
    }

    void clear() {
        for ( auto& [type, pool] : _pools ) pool->clear();
        _entityCounter = 0;
    }

private:
    template<typename T>
    using RawType = std::remove_const_t<T>;

    // 1. MUTABLE GETTER 
    template <typename T>
    Pool<RawType<T>>* ensurePool() {
        std::type_index typeIdx = std::type_index( typeid( RawType<T> ) );
        if ( _pools.find( typeIdx ) == _pools.end() ) {
            _pools[typeIdx] = std::make_unique<Pool<RawType<T>>>();
        }
        return static_cast<Pool<RawType<T>>*>( _pools[typeIdx].get() );
    }

    // 2. CONST GETTER 
    template <typename T>
    const Pool<RawType<T>>* getPool() const {
        std::type_index typeIdx = std::type_index( typeid( RawType<T> ) );
        auto it = _pools.find( typeIdx );
        if ( it == _pools.end() ) return nullptr;
        return static_cast<const Pool<RawType<T>>*>( it->second.get() );
    }

    EntityID _entityCounter = 0;
    std::unordered_map<std::type_index, std::unique_ptr<IPool>> _pools;
};

} // namespace Core::ECS
AXION_NAMESPACE_END