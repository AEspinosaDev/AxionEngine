#pragma once
#include <Axion/Core/Assets/Handle.h>

AXION_NAMESPACE_BEGIN

namespace Core::Scene {

class MeshComponent
{

public:
    MeshComponent()                       = default;
    MeshComponent( const MeshComponent& ) = default;

    MeshComponent( const Assets::MeshHandle&     meshHandle,
                   const Assets::MaterialHandle& materialHandle = {} )
        : _mesh( meshHandle )
        , _material( materialHandle ) {
    }

    const Assets::MeshHandle&     getMesh() const { return _mesh; }
    const Assets::MaterialHandle& getMaterial() const { return _material; }

    bool isVisible() const { return _visible; }
    bool isRaytraced() const { return _raytraced; }

    void setMesh( const Assets::MeshHandle& meshHandle ) { _mesh = meshHandle; }
    void setMaterial( const Assets::MaterialHandle& materialHandle ) { _material = materialHandle; }
    void setVisible( bool visible ) { _visible = visible; }
    void setRaytraced( bool raytraced ) { _raytraced = raytraced; }

private:
    Assets::MeshHandle     _mesh;
    Assets::MaterialHandle _material;

    bool _visible   = true;
    bool _raytraced = true;
};

} // namespace Core::Scene

AXION_NAMESPACE_END