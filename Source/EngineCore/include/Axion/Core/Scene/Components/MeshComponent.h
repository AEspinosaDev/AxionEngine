#pragma once
#include <Axion/Core/Assets/Handle.h>

AXION_NAMESPACE_BEGIN

namespace Core::Scene {

struct MeshComponent {

    Assets::MeshHandle     mesh;
    Assets::MaterialHandle material;

    bool visible   = true;
    bool raytraced = true;

    MeshComponent()                       = default;
    MeshComponent( const MeshComponent& ) = default;
    MeshComponent( const Assets::MeshHandle&     meshHandle,
                   const Assets::MaterialHandle& materialHandle = {} )
        : mesh( meshHandle )
        , material( materialHandle ) {
        };
};

} // namespace Core::Scene

AXION_NAMESPACE_END