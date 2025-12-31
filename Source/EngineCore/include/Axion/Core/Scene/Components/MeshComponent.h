#pragma once
#include <Axion/Core/Assets/Handle.h>

AXION_NAMESPACE_BEGIN

namespace Core::Scene {

struct MeshComponent {

    Assets::MeshHandle     mesh;
    Assets::MaterialHandle material;

    bool visible = true;

    MeshComponent()                       = default;
    MeshComponent( const MeshComponent& ) = default;
};

} // namespace Core::Scene

AXION_NAMESPACE_END