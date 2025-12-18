#pragma once
#include "Axion/Common/Defines.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp> // For value_ptr

AXION_NAMESPACE_BEGIN

// Mathematics library glm
namespace Math {

typedef glm::vec4  Vec4;
typedef glm::vec3  Vec3;
typedef glm::ivec3 iVec3;
typedef glm::vec2  Vec2;
typedef glm::ivec2 iVec2;
typedef glm::mat4  Mat4;
typedef glm::mat3  Mat3;

// --- Transformation Wrappers ---

namespace MTX {

/// @brief Creates a View Matrix (Camera).
/// @param eye Position of the camera.
/// @param center Point the camera is looking at.
/// @param up Up vector of the world (usually 0,1,0).
inline Mat4 lookAt( const Vec3& eye, const Vec3& center, const Vec3& up ) {
    return glm::lookAt( eye, center, up );
}

/// @brief Creates a Perspective Projection Matrix.
/// Automatically uses the correct depth range [0, 1] for DX12/Vulkan.
/// @param fovY Field of View in radians.
/// @param aspect Aspect Ratio (width / height).
/// @param zNear Near clipping plane.
/// @param zFar Far clipping plane.
inline Mat4 perspective( float fovY, float aspect, float zNear, float zFar ) {
    // Use perspectiveRH_ZO:
    // RH: Right Handed (Standard coordinate system)
    // ZO: Zero to One (Depth range 0..1 for DX12/Vulkan)
    // This prevents having to remap depth later.
    auto m = glm::perspectiveRH_ZO( fovY, aspect, zNear, zFar );

    // Note: If you eventually port to Vulkan, you might need to flip the Y-axis here
    // because Vulkan's clip space Y is inverted compared to OpenGL/DX style.
    // m[1][1] *= -1;

    return m;
}

inline Mat4 rotate( const Mat4& m, float angle, const Vec3& axis ) {
    return glm::rotate( m, angle, axis );
}

inline Mat4 translate( const Mat4& m, const Vec3& v ) {
    return glm::translate( m, v );
}

inline Mat4 scale( const Mat4& m, const Vec3& v ) {
    return glm::scale( m, v );
}

inline Mat4 identity() {
    return Mat4( 1.0f );
}

inline Mat4 transpose( const Mat4& m ) {
    return glm::transpose( m );
}

inline Mat4 inverse( const Mat4& m ) {
    return glm::inverse( m );
}

} // namespace MTX

// --- Utility Helpers ---

/// @brief Converts degrees to radians.
inline float radians( float degrees ) {
    return glm::radians( degrees );
}

/// @brief Converts radians to degrees.
inline float degrees( float radians ) {
    return glm::degrees( radians );
}

// --- Data Accessors (Useful for PushConstants/Uniforms) ---

/// @brief Returns a raw pointer to the matrix data (float*).
inline const float* value_ptr( const Mat4& m ) {
    return glm::value_ptr( m );
}

/// @brief Returns a raw pointer to the vector data (float*).
inline const float* value_ptr( const Vec3& v ) {
    return glm::value_ptr( v );
}

} // namespace Math

AXION_NAMESPACE_END