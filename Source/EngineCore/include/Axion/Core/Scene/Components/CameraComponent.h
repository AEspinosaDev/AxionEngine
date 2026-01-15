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

    float FOV       = 60.0f; // Deg
    float nearPlane = 0.01f;
    float farPlane  = 10.0f;
    float orthoSize = 10.0f;

    bool primary = true;

    CameraComponent()                         = default;
    CameraComponent( const CameraComponent& ) = default;

    Math::Mat4 getProjection( Extent2D resolution ) const {
        // 1. Casteo explícito para evitar división entera
        float aspectRatio = static_cast<float>( resolution.width ) / static_cast<float>( resolution.height );

        if ( projectionType == ProjectionType::Perspective )
        {
            return Math::MTX::perspective( Math::radians( FOV ), aspectRatio, nearPlane, farPlane );
        } else
        {
            float height = orthoSize;
            float width  = orthoSize * aspectRatio;

            return Math::MTX::ortho( -width, width, -height, height, nearPlane, farPlane );
        }
    }
};

} // namespace Core::Scene

AXION_NAMESPACE_END