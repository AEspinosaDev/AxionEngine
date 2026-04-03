#pragma once
#include <Axion/Common/Common.h>

AXION_NAMESPACE_BEGIN

namespace Core::Assets {

/**
 * @brief Numeric Handle for Assets
 *
 * @tparam T
 */
template <typename T>
struct Handle {
    u32 id         = UINT32_MAX;
    u32 generation = 0;

    bool isValid() const { return id != UINT32_MAX; }

    bool operator==( const Handle& other ) const {
        return id == other.id && generation == other.generation;
    }
    bool operator!=( const Handle& other ) const {
        return !( *this == other );
    }
};

struct MeshTag {
};
struct TextureTag {
};
struct MaterialTag {
};

using MeshHandle     = Handle<struct MeshTag>;
using TextureHandle  = Handle<struct TextureTag>;
using MaterialHandle = Handle<struct MaterialTag>;

} // namespace Core::Assets

AXION_NAMESPACE_END