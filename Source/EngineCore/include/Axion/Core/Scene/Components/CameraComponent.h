#pragma once
#include <Axion/Common/Common.h>
#include <Axion/Common/Math.h>

AXION_NAMESPACE_BEGIN

namespace Core::Scene {

class CameraComponent
{
public:
    enum class ProjectionType
    {
        Perspective  = 0,
        Orthographic = 1
    };

    CameraComponent()                         = default;
    CameraComponent( const CameraComponent& ) = default;

    // --- Getters ---
    ProjectionType getProjectionType() const { return _projectionType; }

    float getFOV() const { return _FOV; }
    float getNearPlane() const { return _nearPlane; }
    float getFarPlane() const { return _farPlane; }
    float getOrthoSize() const { return _orthoSize; }
    bool  isPrimary() const { return _primary; }
    bool  isPhysical() const { return _usePhysical; }
    float getExposureCompensation() const { return _exposureCompensation; }
    float getAperture() const { return _aperture; }
    float getShutterSpeed() const { return _shutterSpeed; }
    float getISO() const { return _ISO; }

    void setProjectionType( ProjectionType type ) {
        _projectionType = type;
        _isDirty        = true;
    }
    void setFOV( float fov ) {
        _FOV     = fov;
        _isDirty = true;
    }
    void setNearPlane( float nearPlane ) {
        _nearPlane = nearPlane;
        _isDirty   = true;
    }
    void setFarPlane( float farPlane ) {
        _farPlane = farPlane;
        _isDirty  = true;
    }
    void setOrthoSize( float orthoSize ) {
        _orthoSize = orthoSize;
        _isDirty   = true;
    }

    void setPrimary( bool primary ) { _primary = primary; }
    void setUsePhysical( bool usePhysical ) { _usePhysical = usePhysical; }
    void setExposureCompensation( float exposureCompensation ) { _exposureCompensation = exposureCompensation; }
    void setAperture( float aperture ) { _aperture = aperture; }
    void setShutterSpeed( float shutterSpeed ) { _shutterSpeed = shutterSpeed; }
    void setISO( float ISO ) { _ISO = ISO; }

    Math::Mat4 getProjection( Extent2D resolution ) const {
        // Recalculate if properties changed OR if the viewport resolution changed
        if ( _isDirty || _lastResolution.width != resolution.width || _lastResolution.height != resolution.height )
        {
            // Explicit cast to avoid integer division
            float aspectRatio = static_cast<float>( resolution.width ) / static_cast<float>( resolution.height );

            if ( _projectionType == ProjectionType::Perspective )
            {
                _projectionMatrix = Math::MTX::perspective( Math::radians( _FOV ), aspectRatio, _nearPlane, _farPlane );
            } else
            {
                float height = _orthoSize;
                float width  = _orthoSize * aspectRatio;

                _projectionMatrix = Math::MTX::ortho( -width, width, -height, height, _nearPlane, _farPlane );
            }

            // Cache the state
            _lastResolution = resolution;
            _isDirty        = false;
        }

        return _projectionMatrix;
    }

    float getEV100() const {
        if ( _usePhysical )
        {
            // Lagarde's Magic Formula
            // EV = log2( (N^2 * 100) / (S * t) )
            return std::log2f( ( _aperture * _aperture * 100.0f ) / ( _ISO * _shutterSpeed ) );
        } else
        {
            const float standardAutoExposureEV = 9.7f;
            return standardAutoExposureEV - _exposureCompensation;
        }
    }

private:
    ProjectionType _projectionType = ProjectionType::Perspective;

    float _FOV       = 60.0f; // Deg
    float _nearPlane = 0.01f;
    float _farPlane  = 1000.0f;
    float _orthoSize = 10.0f;

    bool _primary = true;

    bool  _usePhysical          = false;
    float _exposureCompensation = 0.0f;

    //  Physically Based
    float _aperture     = 16.0f;         // f-stop (f/1.8, f/16...) ->  Depth of Field
    float _shutterSpeed = 1.0f / 125.0f; // Seconds (1/60, 1/1000...) -> Motion Blur
    float _ISO          = 100.0f;        // Sensitivity -> Noise

    mutable Math::Mat4 _projectionMatrix = Math::Mat4( 1.0f );
    mutable Extent2D   _lastResolution   = { 0, 0 };
    mutable bool       _isDirty          = true;
};

} // namespace Core::Scene

AXION_NAMESPACE_END