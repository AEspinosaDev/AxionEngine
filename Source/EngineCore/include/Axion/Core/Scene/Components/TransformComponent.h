#pragma once
#include <Axion/Common/Defines.h>
#include <Axion/Common/Math.h>

AXION_NAMESPACE_BEGIN

namespace Core::Scene {
struct TransformComponent {

    Math::Vec3 translation = { 0.0f, 0.0f, 0.0f };
    Math::Quat rotation    = { 1.0f, 0.0f, 0.0f, 0.0f };
    Math::Vec3 scale       = { 1.0f, 1.0f, 1.0f };

    TransformComponent()                            = default;
    TransformComponent( const TransformComponent& ) = default;
    TransformComponent( const Math::Vec3& t )
        : translation( t ) {}

    Math::Mat4 getMatrix() const {
        Math::Mat4 rotationMat = Math::MTX::toMat4( rotation );

        return Math::MTX::translate( Math::Mat4( 1.0f ), translation ) *
               rotationMat *
               Math::MTX::scale( Math::Mat4( 1.0f ), scale );
    }

    // Returns the local forward vector in world space
    Math::Vec3 forward() const {
        return rotation * Math::Vec3( 0.0f, 0.0f, -1.0f );
    }

    // Returns the local up vector in world space
    Math::Vec3 up() const {
        return rotation * Math::Vec3( 0.0f, 1.0f, 0.0f );
    }

    // Returns the local right vector in world space
    Math::Vec3 right() const {
        return rotation * Math::Vec3( 1.0f, 0.0f, 0.0f );
    }

    void translate( const Math::Vec3& offset ) {
        translation += offset;
    }
    void position( const Math::Vec3& pos ) {
        translation = pos;
    }
    // Rotates the entity by multiplying the current rotation by a delta quaternion.
    // Order: rotation * delta applies the rotation in Local Space.
    void rotate( const Math::Quat& deltaRotation ) {
        rotation = rotation * deltaRotation;
        rotation = Math::normalize( rotation );
    }

    // Helper to rotate using Euler angles (in radians), converted internally to Quat
    void rotate( const Math::Vec3& eulerAngles ) {
        Math::Quat delta = Math::Quat( eulerAngles );
        rotation         = rotation * delta;
    }

    // Updates Translation, Rotation and Scale from a World Matrix.
    // NOTE: This uses glm::decompose under the hood. It requires the matrix to be orthogonal.
    void fromMatrix( const Math::Mat4& matrix ) {
        Math::Vec3 skew;
        Math::Vec4 perspective;

        Math::MTX::decompose( matrix, scale, rotation, translation, skew, perspective );

        rotation = Math::normalize( rotation );
    }

    // Orients the transform to look at a specific target position.
    // This calculates the rotation needed so that the forward vector (-Z) points to 'target'.
    void lookAt( const Math::Vec3& target, const Math::Vec3& worldUp = { 0.0f, 1.0f, 0.0f } ) {
        Math::Mat4 viewMat  = Math::MTX::lookAt( translation, target, worldUp );
        Math::Mat4 worldMat = Math::MTX::inverse( viewMat );

        // Note: This effectively keeps the current translation and sets scale to 1.0
        // (because lookAt generates a pure rotation/translation matrix).
        fromMatrix( worldMat );
    }
};

} // namespace Core::Scene

AXION_NAMESPACE_END