#pragma once
#include <Axion/Common/Common.h>

AXION_NAMESPACE_BEGIN

namespace Core::Scene {

struct LightComponent {

    enum class Type : uchar
    {
        Directional = 0,
        Point       = 1,
        Spot        = 2,
        Area        = 3
    };

    LightComponent()                        = default;
    LightComponent( const LightComponent& ) = default;
    LightComponent( Math::Vec3 color, float intensity, Type type = Type::Point )
        : color( color )
        , intensity( intensity )
        , type( type ) {
        };

    Type type        = Type::Point;
    bool active      = true;
    bool castShadows = false;

    Math::Vec3 color          = { 1.0f, 1.0f, 1.0f };
    bool       useTemperature = false;
    float      temperature    = 6500.0f; // Kelvin (D65 as default)
    float      intensity      = 100.0f;  // Lux/Candels depending on type

    float range = 10.0f;

    float innerAngle = 30.0f;
    float outerAngle = 45.0f;

    Math::Vec2 areaSize = { 1.0f, 1.0f };

    // MeshHandle geometryLight;
    // TextureHandle iesProfile;
};

} // namespace Core::Scene

AXION_NAMESPACE_END