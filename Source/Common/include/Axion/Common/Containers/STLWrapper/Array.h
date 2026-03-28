#pragma once
#include <Axion/Common/Memory/STLAdapter.h>
#include <array>

AXION_NAMESPACE_BEGIN

namespace STLW {

// Array wrapper
template <typename T, std::size_t N>
using Array = std::array<T, N>;

} // namespace STLW

AXION_NAMESPACE_END