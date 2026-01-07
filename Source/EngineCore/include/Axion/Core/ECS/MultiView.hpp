#pragma once
#include <Axion/Common/Logging.h>
#include <Axion/Core/ECS/SparseSet.hpp>
#include <algorithm>
#include <cassert>
#include <vector>

AXION_NAMESPACE_BEGIN

namespace Core::ECS {

class Registry;

template <typename... Components>
class MultiView
{
    friend class Registry;
public:

    struct Iterator {
        using EntityIter = std::vector<EntityID>::const_iterator;

        EntityIter                       current;
        EntityIter                       end;
        std::tuple<Pool<Components>*...> pools;

        Iterator( EntityIter start, EntityIter stop, std::tuple<Pool<Components>*...> p )
            : current( start )
            , end( stop )
            , pools( p ) {

            if ( current != end && !isValid( *current ) )
                ++( *this );
        }

        bool isValid( EntityID entity ) {
            return std::apply( [&]( auto*... p ) { return ( ( p->has( entity ) ) && ... ); }, pools );
        }

        Iterator& operator++() {

            do
            {
                ++current;
            } while ( current != end && !isValid( *current ) );
            return *this;
        }

        bool operator!=( const Iterator& other ) const { return current != other.current; }

        EntityID operator*() const { return *current; }
    };

    Iterator begin() {
        if ( !_candidatePool )
            return Iterator( {}, {}, _pools ); // Safety empty

        const auto& entities = static_cast<IPool*>( _candidatePool )->getEntities();
        return Iterator( entities.begin(), entities.end(), _pools );
    }

    Iterator end() {
        if ( !_candidatePool )
            return Iterator( {}, {}, _pools );
        const auto& entities = static_cast<IPool*>( _candidatePool )->getEntities();
        return Iterator( entities.end(), entities.end(), _pools );
    }

    template <typename T>
    T& get( EntityID entity ) {
        return std::get<Pool<T>*>( _pools )->get( entity );
    }

private:
    MultiView( Pool<Components>*... pools )
        : _pools( std::make_tuple( pools... ) ) {

        size_t minSize  = std::numeric_limits<size_t>::max();
        IPool* smallest = nullptr;

        // Check smallest
        auto checkSize = [&]( auto* pool ) {
            if ( pool && pool->getEntities().size() < minSize )
            {
                minSize  = pool->getEntities().size();
                smallest = pool;
            }
        };

        //(C++17 fold expression)
        ( checkSize( pools ), ... );

        _candidatePool = smallest;
    }

    std::tuple<Pool<Components>*...> _pools;
    IPool*                           _candidatePool = nullptr;
};

} // namespace Core::ECS

AXION_NAMESPACE_END