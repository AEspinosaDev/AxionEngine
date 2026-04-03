#pragma once
#include <Axion/Common/Logging.h>
#include <Axion/Core/ECS/Entity.h>
#include <algorithm>
#include <cassert>
#include <vector>

AXION_NAMESPACE_BEGIN

namespace Core::ECS {

class IPool
{
public:
    virtual ~IPool()                          = default;
    virtual void remove( EntityID entity )    = 0;
    virtual bool has( EntityID entity ) const = 0;
    virtual void clear()                      = 0;

    virtual const std::vector<EntityID>& getEntities() const = 0;
};

template <typename T>
class Pool : public IPool
{
public:
    Pool( u64 capacity = MAX_ENTITIES ) {
        _components.reserve( 100 );
        _entityIndices.reserve( 100 );

        _sparse.resize( capacity, NULL_ENTITY );
    }

    virtual ~Pool() = default;

    bool has( EntityID entity ) const override {
        return entity < _sparse.size() && _sparse[entity] != NULL_ENTITY;
    }

    T& add( EntityID entity, const T& component ) {
        AXION_LOG_ASSERT( !has( entity ), Logger::Module::Core, "Entity already has this component!" );

        // 1. Push data to the dense array
        _components.push_back( component );

        // 2. Store which entity owns this data (for reverse lookup)
        _entityIndices.push_back( entity );

        // 3. Map the entity ID to the index in the dense array
        u64 index = _components.size() - 1;

        if ( entity >= _sparse.size() )
            _sparse.resize( entity + 1, NULL_ENTITY );

        _sparse[entity] = (EntityID)index;

        return _components.back();
    }

    T& get( EntityID entity ) {
        AXION_LOG_ASSERT( has( entity ), Logger::Module::Core, "Entity does not have this component!!" );
        return _components[_sparse[entity]];
    }
    const T& get( EntityID entity ) const {
        AXION_LOG_ASSERT( has( entity ), Logger::Module::Core, "Entity does not have this component!!" );
        return _components[_sparse[entity]];
    }
    
    void remove( EntityID entity ) override {
        AXION_LOG_ASSERT( has( entity ), Logger::Module::Core, "Entity does not have this component!!" );

        u64 indexToRemove = _sparse[entity];
        u64 lastIndex     = _components.size() - 1;

        if ( indexToRemove != lastIndex )
        {
            EntityID lastEntity = _entityIndices[lastIndex];

            // Move the last component to the hole
            _components[indexToRemove]    = std::move( _components[lastIndex] );
            _entityIndices[indexToRemove] = lastEntity;

            // Update the sparse map for the swapped entity
            _sparse[lastEntity] = (EntityID)indexToRemove;
        }

        _components.pop_back();
        _entityIndices.pop_back();

        // Mark the sparse slot as empty
        _sparse[entity] = NULL_ENTITY;
    }

    void clear() override {
        _components.clear();
        _entityIndices.clear();
        std::fill( _sparse.begin(), _sparse.end(), NULL_ENTITY );
    }

    std::vector<T>&              getData() { return _components; }
    const std::vector<T>&        getData() const { return _components; }
    const std::vector<EntityID>& getEntities() const override { return _entityIndices; }

private:
    // Components
    std::vector<T> _components;
    // Dense Index -> EntityID
    std::vector<EntityID> _entityIndices;
    // Sparse array: Sparse[EntityID] -> Dense Index
    std::vector<EntityID> _sparse;
};

} // namespace Core::ECS

AXION_NAMESPACE_END