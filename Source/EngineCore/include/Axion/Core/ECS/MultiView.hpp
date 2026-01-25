#pragma once
#include <Axion/Core/ECS/SparseSet.hpp>
#include <tuple>
#include <vector>
#include <type_traits>

AXION_NAMESPACE_BEGIN

namespace Core::ECS {

class Registry;

template <typename... Components>
class MultiView
{
    friend class Registry;


    template<typename T>
    using RawType = std::remove_const_t<T>;

    template<typename T>
    using PoolType = Pool<RawType<T>>;

    template<typename T>
    using PoolPtr = std::conditional_t<std::is_const_v<T>, const PoolType<T>*, PoolType<T>*>;

    // --- TYPE RESOLVER ---
    template<typename Search, typename... List> struct Resolver;

    template<typename Search> 
    struct Resolver<Search> { using type = void; };

    template<typename Search, typename Head, typename... Tail>
    struct Resolver<Search, Head, Tail...> {
        using type = std::conditional_t<
            std::is_same_v<RawType<Search>, RawType<Head>>, // ¿Coinciden ignorando const?
            Head,                                           // Sí: Devolvemos el tipo real de la lista (Head)
            typename Resolver<Search, Tail...>::type        // No: Seguimos buscando
        >;
    };

public:
    struct Iterator {
        using EntityIter = std::vector<EntityID>::const_iterator;

        EntityIter current;
        EntityIter end;
        std::tuple<PoolPtr<Components>...> pools;

        Iterator( EntityIter start, EntityIter stop, std::tuple<PoolPtr<Components>...> p )
            : current( start ), end( stop ), pools( p ) 
        {
            if ( current != end && !isValid( *current ) ) ++( *this );
        }

        bool isValid( EntityID entity ) {
            return std::apply( [&]( auto*... p ) { return ( ( p->has( entity ) ) && ... ); }, pools );
        }

        Iterator& operator++() {
            do { ++current; } while ( current != end && !isValid( *current ) );
            return *this;
        }

        bool operator!=( const Iterator& other ) const { return current != other.current; }
        EntityID operator*() const { return *current; }
    };

    Iterator begin() {
        if ( !_candidatePool || _isBroken ) return Iterator( {}, {}, _pools );
        const auto& entities = _candidatePool->getEntities();
        return Iterator( entities.begin(), entities.end(), _pools );
    }

    Iterator end() {
        if ( !_candidatePool || _isBroken ) return Iterator( {}, {}, _pools );
        const auto& entities = _candidatePool->getEntities();
        return Iterator( entities.end(), entities.end(), _pools );
    }

    // --- GETTER INTELIGENTE ---
    template <typename T>
    auto& get( EntityID entity ) {
        using ActualT = typename Resolver<T, Components...>::type;

        static_assert(!std::is_same_v<ActualT, void>, "Error: Has pedido un componente que no está en esta MultiView.");

        return std::get<PoolPtr<ActualT>>( _pools )->get( entity );
    }

private:
    MultiView( PoolPtr<Components>... pools )
        : _pools( std::make_tuple( pools... ) ) 
    {
        size_t minSize = std::numeric_limits<size_t>::max();
        
        auto checkSize = [&]( auto* pool ) {
            if ( !pool ) {
                _isBroken = true;
                return;
            }
            if ( pool->getEntities().size() < minSize ) {
                minSize = pool->getEntities().size();
                _candidatePool = pool;
            }
        };

        ( checkSize( pools ), ... );

        if ( _isBroken ) _candidatePool = nullptr;
    }

    std::tuple<PoolPtr<Components>...> _pools;
    const IPool* _candidatePool = nullptr;
    bool _isBroken = false;
};

} // namespace Core::ECS
AXION_NAMESPACE_END