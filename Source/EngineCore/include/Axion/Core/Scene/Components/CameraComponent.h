#pragma once
#include <Axion/Common/Common.h>
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
    float farPlane  = 1000.0f;
    float orthoSize = 10.0f;

    bool primary = true;

    bool  usePhysical          = false;
    float exposureCompensation = 0.0f;
    // --- Physically Based --
    float aperture     = 16.0f;         // f-stop (f/1.8, f/16...) ->  Depth of Field
    float shutterSpeed = 1.0f / 125.0f; // Seconds (1/60, 1/1000...) -> Motion Blur
    float ISO          = 100.0f;        // Sensibilidad -> Noise

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

    float getEV100() const {
        if ( usePhysical )
        {
            // Lagarde's Magic Formula
            // EV = log2( (N^2 * 100) / (S * t) )
            return std::log2f( ( aperture * aperture * 100.0f ) / ( ISO * shutterSpeed ) );
        } else
        {
            const float standardAutoExposureEV = 9.7f;
            return standardAutoExposureEV - exposureCompensation;
        }
    }
};

} // namespace Core::Scene

AXION_NAMESPACE_END