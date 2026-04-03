#pragma once
#include <Axion/Common/Common.h>

AXION_NAMESPACE_BEGIN

namespace Core::Scene {

class LightComponent
{
public:
    enum class Type : byte
    {
        Directional = 0,
        Point       = 1,
        Spot        = 2,
        Area        = 3
    };
    LightComponent()                        = default;
    LightComponent( const LightComponent& ) = default;
    LightComponent( const Math::Vec3& color, float intensity, Type type = Type::Point )
        : _type( type )
        , _color( color )
        , _intensity( intensity ) {
    }

    Type              getType() const { return _type; }
    bool              isActive() const { return _active; }
    bool              castsShadows() const { return _castShadows; }
    const Math::Vec3& getColor() const { return _color; }
    bool              usesTemperature() const { return _useTemperature; }
    float             getTemperature() const { return _temperature; }
    float             getIntensity() const { return _intensity; }
    float             getRange() const { return _range; }
    float             getInnerAngle() const { return _innerAngle; }
    float             getOuterAngle() const { return _outerAngle; }
    const Math::Vec2& getAreaSize() const { return _areaSize; }

    void setType( Type type ) { _type = type; }
    void setActive( bool active ) { _active = active; }
    void setCastShadows( bool castShadows ) { _castShadows = castShadows; }
    void setColor( const Math::Vec3& color ) { _color = color; }
    void setUseTemperature( bool useTemperature ) { _useTemperature = useTemperature; }
    void setTemperature( float temperature ) { _temperature = temperature; }
    void setIntensity( float intensity ) { _intensity = intensity; }
    void setRange( float range ) { _range = range; }
    void setInnerAngle( float innerAngle ) { _innerAngle = innerAngle; }
    void setOuterAngle( float outerAngle ) { _outerAngle = outerAngle; }
    void setAreaSize( const Math::Vec2& areaSize ) { _areaSize = areaSize; }

private:
    Type _type        = Type::Point;
    bool _active      = true;
    bool _castShadows = false;

    Math::Vec3 _color          = { 1.0f, 1.0f, 1.0f };
    bool       _useTemperature = false;
    float      _temperature    = 6500.0f; // Kelvin (D65 as default)
    float      _intensity      = 100.0f;  // Lux/Candelas depending on type

    float _range = 10.0f;

    float _innerAngle = 30.0f;
    float _outerAngle = 45.0f;

    Math::Vec2 _areaSize = { 1.0f, 1.0f };

    // Assets::MeshHandle _geometryLight;
    // Assets::TextureHandle _iesProfile;
};

} // namespace Core::Scene

AXION_NAMESPACE_END