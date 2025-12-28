#pragma once
#include <Axion/Common/Defines.h>
#include <Axion/Common/Math.h>

AXION_NAMESPACE_BEGIN

namespace Core::Scene {

struct CameraComponent {

    enum class ProjectionType
    {
        Perspective  = 0,
        Orthographic = 1
    };

    ProjectionType projectionType = ProjectionType::Perspective;

    float FOV       = 45.0f; // Deg
    float nearPlane = 0.1f;
    float farPlane  = 1000.0f;
    float orthoSize = 10.0f;

    bool primary = true;

    CameraComponent()                         = default;
    CameraComponent( const CameraComponent& ) = default;
};

} // namespace Core::Scene

AXION_NAMESPACE_END