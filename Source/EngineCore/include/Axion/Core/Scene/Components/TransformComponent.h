#pragma once
#include <Axion/Common/Common.h>
#include <Axion/Common/Math.h>

AXION_NAMESPACE_BEGIN

namespace Core::Scene {

class TransformComponent
{
public:
    TransformComponent()                            = default;
    TransformComponent( const TransformComponent& ) = default;
    TransformComponent( const Math::Vec3& t )
        : _translation( t ) {}

    const Math::Vec3& getTranslation() const { return _translation; }
    const Math::Quat& getRotation() const { return _rotation; }
    const Math::Vec3& getScale() const { return _scale; }

    void setTranslation( const Math::Vec3& t ) {
        _translation = t;
        _isDirty     = true;
    }
    void setRotation( const Math::Quat& r ) {
        _rotation = r;
        _isDirty  = true;
    }
    void setScale( const Math::Vec3& s ) {
        _scale   = s;
        _isDirty = true;
    }

    const Math::Mat4& getMatrix() const {
        if ( _isDirty )
        {
            Math::Mat4 rotationMat = Math::MTX::toMat4( _rotation );

            _worldMatrix = Math::MTX::translate( Math::Mat4( 1.0f ), _translation ) *
                           rotationMat *
                           Math::MTX::scale( Math::Mat4( 1.0f ), _scale );

            _isDirty = false;
        }
        return _worldMatrix;
    }

    // Returns the local forward vector in world space
    Math::Vec3 forward() const {
        return _rotation * Math::Vec3( 0.0f, 0.0f, -1.0f );
    }

    // Returns the local up vector in world space
    Math::Vec3 up() const {
        return _rotation * Math::Vec3( 0.0f, 1.0f, 0.0f );
    }

    // Returns the local right vector in world space
    Math::Vec3 right() const {
        return _rotation * Math::Vec3( 1.0f, 0.0f, 0.0f );
    }

    void translate( const Math::Vec3& offset ) {
        _translation += offset;
        _isDirty = true;
    }

    void position( const Math::Vec3& pos ) {
        _translation = pos;
        _isDirty     = true;
    }

    void scaleBy( const Math::Vec3& scaleFactor ) {
        _scale *= scaleFactor;
        _isDirty = true;
    }

    void scaleUniform( float factor ) {
        _scale *= factor;
        _isDirty = true;
    }

    // Rotates the entity by multiplying the current rotation by a delta quaternion.
    // Order: rotation * delta applies the rotation in Local Space.
    void rotate( const Math::Quat& deltaRotation ) {
        _rotation = _rotation * deltaRotation;
        _rotation = Math::normalize( _rotation );
        _isDirty  = true;
    }

    // Helper to rotate using Euler angles (in radians), converted internally to Quat
    void rotate( const Math::Vec3& eulerAngles ) {
        Math::Quat delta = Math::Quat( eulerAngles );
        _rotation        = _rotation * delta;
        _isDirty         = true;
    }

    // Updates Translation, Rotation and Scale from a World Matrix.
    // NOTE: This uses glm::decompose under the hood. It requires the matrix to be orthogonal.
    void fromMatrix( const Math::Mat4& matrix ) {
        Math::Vec3 skew;
        Math::Vec4 perspective;

        Math::MTX::decompose( matrix, _scale, _rotation, _translation, skew, perspective );

        _rotation = Math::normalize( _rotation );
        _isDirty  = true;
    }

    // Orients the transform to look at a specific target position.
    // This calculates the rotation needed so that the forward vector (-Z) points to 'target'.
    void lookAt( const Math::Vec3& target, const Math::Vec3& worldUp = { 0.0f, 1.0f, 0.0f } ) {
        Math::Mat4 viewMat  = Math::MTX::lookAt( _translation, target, worldUp );
        Math::Mat4 worldMat = Math::MTX::inverse( viewMat );

        // Note: This effectively keeps the current translation and sets scale to 1.0
        // (because lookAt generates a pure rotation/translation matrix).
        fromMatrix( worldMat );
    }

private:
    Math::Vec3 _translation = { 0.0f, 0.0f, 0.0f };
    Math::Quat _rotation    = { 1.0f, 0.0f, 0.0f, 0.0f };
    Math::Vec3 _scale       = { 1.0f, 1.0f, 1.0f };

    mutable Math::Mat4 _worldMatrix = Math::Mat4( 1.0f );
    mutable bool       _isDirty     = true;
};

} // namespace Core::Scene

AXION_NAMESPACE_END