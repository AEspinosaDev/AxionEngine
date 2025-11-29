#pragma once
#include "Axion/Common/Defines.h"

AXION_NAMESPACE_BEGIN

namespace Helpers {

inline void hashCombine( size_t& seed, size_t value ) {
    seed ^= value + 0x9e3779b9 + ( seed << 6 ) + ( seed >> 2 );
}

} // namespace Helpers

AXION_NAMESPACE_END