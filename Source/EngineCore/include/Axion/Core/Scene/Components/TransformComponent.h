#pragma once
#include <Axion/Common/Defines.h>
#include <Axion/Common/Math.h>

AXION_NAMESPACE_BEGIN

namespace Core::Scene {

struct TransformComponent {

    Math::Vec3 translation = { 0.0f, 0.0f, 0.0f };
    Math::Vec3 rotation    = { 0.0f, 0.0f, 0.0f };
    Math::Vec3 scale       = { 1.0f, 1.0f, 1.0f };

    TransformComponent()                            = default;
    TransformComponent( const TransformComponent& ) = default;
    TransformComponent( const Math::Vec3& t )
        : translation( t ) {}

    Math::Mat4 getMatrix() const {
        Math::Mat4 rotationMat = Math::MTX::toMat4( Math::quat( rotation ) );

        return Math::MTX::translate( Math::Mat4( 1.0f ), translation ) *
               rotationMat *
               Math::MTX::scale( Math::Mat4( 1.0f ), scale );
    }
};

} // namespace Core::Scene

AXION_NAMESPACE_END