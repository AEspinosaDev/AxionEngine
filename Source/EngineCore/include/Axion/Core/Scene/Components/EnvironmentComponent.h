#pragma once
#include <Axion/Common/Common.h>
#include <Axion/Common/Math.h>
#include <Axion/Core/Assets/Handle.h>

AXION_NAMESPACE_BEGIN

namespace Core::Scene {

struct EnvironmentComponent {

    enum class Type : uchar
    {
        Global = 0,
        Volume = 1,
    };
    enum class SkyType : uchar
    {
        Constant   = 0,
        Procedural = 1, // Passes needed {Atmosphere Compute, Athmosphere -> Screen}
        HDRi       = 2, // Passes needed {HDRI -> Cubemap, cubemap -> irradiance SH}
    };

    EnvironmentComponent()                              = default;
    EnvironmentComponent( const EnvironmentComponent& ) = default;

    Type       type = Type::Global;
    Math::AABB aabb; // In case its volume type
    float      blendDistance = 0.0f;
    uint       priority      = 0;

    SkyType               skyType     = SkyType::Constant; // For now, only constant supported
    Math::Vec3            skyColor    = { 0.1f, 0.2f, 0.7f };
    Math::Vec3            groundColor = { 0.3f, 0.3f, 0.3f };
    float                 intensity   = 1.0f;
    Assets::TextureHandle hdriHandle;

    bool active = true;
};

} // namespace Core::Scene

AXION_NAMESPACE_END