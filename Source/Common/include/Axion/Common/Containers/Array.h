#pragma once
#include <Axion/Common/Common.h>
#include <Axion/Common/Logging.h>

AXION_NAMESPACE_BEGIN

/**
 * @brief A simple fixed-size array that provides stack allocation and basic array functionalities without dynamic memory allocation.
 */
template <typename T, u64 N>
class FixedArray
{
public:
    using ValueType = T;

    constexpr FixedArray() = default;
    constexpr FixedArray( const T& item ) {
        for ( u64 i = 0; i < N; ++i )
        {
            _data[i] = item;
        }
    }
    constexpr FixedArray( std::initializer_list<T> list ) {
        AXION_LOG_ASSERT( list.size() <= N, Logger::Module::Common, "Initializer list exceeds FixedArray capacity!" );

        u64 i = 0;
        for ( const auto& item : list )
        {
            if ( i >= N )
                break;
            _data[i++] = item;
        }
    }

    constexpr FixedArray( const FixedArray& )            = default;
    constexpr FixedArray& operator=( const FixedArray& ) = default;
    constexpr FixedArray( FixedArray&& )                 = default;
    constexpr FixedArray& operator=( FixedArray&& )      = default;

    constexpr T& operator[]( u64 index ) {
        AXION_LOG_ASSERT( index < N, Logger::Module::Common, "Index out of bounds: {} (Capacity: {})", index, N );
        return _data[index];
    }

    constexpr const T& operator[]( u64 index ) const {
        AXION_LOG_ASSERT( index < N, Logger::Module::Common, "Index out of bounds: {} (Capacity: {})", index, N );
        return _data[index];
    }

    constexpr T*       begin() { return _data; }
    constexpr const T* begin() const { return _data; }
    constexpr T*       end() { return _data + N; }
    constexpr const T* end() const { return _data + N; }

    constexpr u64      size() const { return N; }
    constexpr T*       data() { return _data; }
    constexpr const T* data() const { return _data; }

private:
    T _data[N] {};
};


template <typename T, u64 N = 16>
using SmallFixedArray = FixedArray<T, N>;

AXION_NAMESPACE_END
