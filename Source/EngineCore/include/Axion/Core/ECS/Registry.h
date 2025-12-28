#pragma once
#include <Axion/Core/ECS/Entity.h>
#include <Axion/Core/ECS/SparseSet.hpp>

#include <memory>
#include <typeindex>
#include <unordered_map>
#include <vector>

AXION_NAMESPACE_BEGIN

namespace Core::ECS {

class Registry
{
public:
    Registry() = default;
    ~Registry() { clear(); }

    EntityID createEntity() {
        return _entityCounter++;
    }

    void destroyEntity( EntityID entity ) {
        for ( auto& [type, pool] : _pools )
        {
            if ( pool->has( entity ) )
            {
                pool->remove( entity );
            }
        }
    }

    template <typename T>
    T& addComponent( EntityID entity, T component ) {
        return getPool<T>()->add( entity, component );
    }

    template <typename T>
    void removeComponent( EntityID entity ) {
        getPool<T>()->remove( entity );
    }

    template <typename T>
    T& getComponent( EntityID entity ) {
        return getPool<T>()->get( entity );
    }

    template <typename T>
    bool hasComponent( EntityID entity ) {
        return getPool<T>()->has( entity );
    }

    template <typename T>
    std::vector<T>& view() {
        return getPool<T>()->getData();
    }

    void clear() {
        for ( auto& [type, pool] : _pools )
        {
            pool->clear();
        }
        _entityCounter = 0;
    }

private:
    template <typename T>
    Pool<T>* getPool() {
        std::type_index typeIdx = std::type_index( typeid( T ) );

        if ( _pools.find( typeIdx ) == _pools.end() )
        {
            _pools[typeIdx] = std::make_unique<Pool<T>>();
        }

        return static_cast<Pool<T>*>( _pools[typeIdx].get() );
    }

private:
    EntityID                                                    _entityCounter = 0;
    std::unordered_map<std::type_index, std::unique_ptr<IPool>> _pools;
};

} // namespace Core::ECS

AXION_NAMESPACE_END