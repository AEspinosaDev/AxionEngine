#pragma once
#include <Axion/Core/Assets/AssetTypes.h>

AXION_NAMESPACE_BEGIN

namespace Core::Scene {

struct MeshComponent {

    Assets::Mesh     mesh;
    Assets::Material material;

    bool visible = true; 

    MeshComponent()                       = default;
    MeshComponent( const MeshComponent& ) = default;
};

} // namespace Core::Scene

AXION_NAMESPACE_END