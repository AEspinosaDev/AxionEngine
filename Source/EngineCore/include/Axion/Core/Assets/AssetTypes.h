#pragma once
#include <Axion/Common/Defines.h>

AXION_NAMESPACE_BEGIN

namespace Core::Assets {

using AssetHandle = uint;

struct Mesh {
    AssetHandle handle = 0;
    // Aquí irían punteros a VertexBuffer/IndexBuffer en GPU
};

struct Material {
    AssetHandle handle = 0;
    // Aquí irían colores, texturas, shaders...
};

} // namespace Core::Assets

AXION_NAMESPACE_END