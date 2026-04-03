#pragma once
#include "Axion/Common/Common.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp> // For value_ptr
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/matrix_access.hpp>
#include <glm/gtx/matrix_decompose.hpp>

AXION_NAMESPACE_BEGIN

// Mathematics library powered by GLM
namespace Math {

typedef glm::vec4  Vec4;
typedef glm::vec3  Vec3;
typedef glm::ivec3 iVec3;
typedef glm::vec2  Vec2;
typedef glm::ivec2 iVec2;
typedef glm::mat4  Mat4;
typedef glm::mat3  Mat3;
typedef glm::quat  Quat;

const float PI      = 3.14159265359f;
const float PI_HALF = 1.57079632679f;
const float PI_2    = 6.28318530718f;

// --- Transformation Wrappers ---
#pragma region Matrix Trans

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
inline void decompose( const Mat4& m, Vec3& scale, Quat& rotation, Vec3& translation, Vec3& skew, Vec4& perspective ) {
    glm::decompose( m, scale, rotation, translation, skew, perspective );
}


inline Mat4 rotate( const Mat4& m, float angle, const Vec3& axis ) {
    return glm::rotate( m, angle, axis );
}

inline Mat4 translate( const Mat4& m, const Vec3& v ) {
    return glm::translate( m, v );
}

inline Mat4 scale( const Mat4& m, float s ) {
    return glm::scale( m, Vec3( s ) );
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
inline Mat4 toMat4( const Quat& q ) {
    return glm::mat4_cast( q );
}

inline Mat4 ortho( float left, float right, float bottom, float top, float nearPlane, float farPlane ) {
    return glm::orthoRH_ZO( left, right, bottom, top, nearPlane, farPlane );
}

} // namespace MTX

#pragma endregion
#pragma region Utils

// --- Utility Helpers ---


inline float length( const Vec3& v ) {
    return glm::length( v );
}


inline Quat quat( const Vec3& eulerRadians ) {
    return glm::quat( eulerRadians );
}

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

template <typename T>
inline T min( const T& a, const T& b ) {
    return glm::min( a, b );
}

template <typename T>
inline T max( const T& a, const T& b ) {
    return glm::max( a, b );
}

template <typename T>
inline float distance( const T& a, const T& b ) {
    return glm::distance( a, b );
}

template <typename T>
inline T clamp( const T& val, const T& minVal, const T& maxVal ) {
    return glm::clamp( val, minVal, maxVal );
}

template <typename T>
inline T normalize( const T& v ) {
    return glm::normalize( v );
}

inline float sin( float x ) {
    return glm::sin( x );
}
inline float asin( float x ) {
    return glm::asin( x );
}

inline float cos( float x ) {
    return glm::cos( x );
}
inline float acos( float x ) {
    return glm::acos( x );
}

inline float tan( float x ) {
    return glm::tan( x );
}
inline float atan( float x ) {
    return glm::atan( x );
}

inline float sqrt( float x ) {
    return glm::sqrt( x );
}
inline float pow( float x, float y ) {
    return glm::pow( x, y );
}
inline float exp( float x ) {
    return glm::exp( x );
}
inline float sqr( float x ) {
    return x * x;
}

inline float lerp( float a, float b, float t ) {
    return glm::mix( a, b, t );
}

template <typename T>
inline float dot( const T& a, const T& b ) {
    return glm::dot( a, b );
}

template <typename T>
inline T cross( const T& a, const T& b ) {
    return glm::cross( a, b );
}

#pragma endregion
// --- Bounding Volumes ---

#pragma region BVs

struct AABB {
    Vec3 min = { std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max() };
    Vec3 max = { -std::numeric_limits<float>::max(), -std::numeric_limits<float>::max(), -std::numeric_limits<float>::max() };

    void merge( const Vec3& point ) {
        min = Math::min( min, point ); // Asumiendo que tienes min/max vec3 en tu math
        max = Math::max( max, point );
    }

    Vec3 getCenter() const { return ( min + max ) * 0.5f; }
    Vec3 getExtents() const { return ( max - min ) * 0.5f; }
};

struct BoundingSphere {
    Vec3  center = { 0, 0, 0 };
    float radius = 0.0f;
};

struct Frustum {
    // x, y, z = Normal
    // w = Origin Dist (D)
    // Order: Left, Right, Bottom, Top, Near, Far
    Vec4 planes[6];
};

inline Frustum createFrustumFromMatrix( const glm::mat4& viewProj ) {
    Frustum frustum;

    // Gribb-Hartmann Extraction

    // 1. LEFT Plane:   row4 + row1
    frustum.planes[0] = glm::row( viewProj, 3 ) + glm::row( viewProj, 0 );
    // 2. RIGHT Plane:  row4 - row1
    frustum.planes[1] = glm::row( viewProj, 3 ) - glm::row( viewProj, 0 );
    // 3. BOTTOM Plane: row4 + row2
    frustum.planes[2] = glm::row( viewProj, 3 ) + glm::row( viewProj, 1 );
    // 4. TOP Plane:    row4 - row2
    frustum.planes[3] = glm::row( viewProj, 3 ) - glm::row( viewProj, 1 );

    // 5. NEAR Plane:   row4 + row3 (OpenGL/GLM default -1..1)
    // Vulkan/DX con clip 0..1 puro, only row3,
    frustum.planes[4] = glm::row( viewProj, 3 ) + glm::row( viewProj, 2 );
    // 6. FAR Plane:    row4 - row3
    frustum.planes[5] = glm::row( viewProj, 3 ) - glm::row( viewProj, 2 );

    for ( int i = 0; i < 6; ++i )
    {
        glm::vec3 normal = glm::vec3( frustum.planes[i] );
        float     length = glm::length( normal );

        frustum.planes[i] /= length;
    }

    return frustum;
}

#pragma endregion
#pragma region Algorithms

struct TangentBinormal {
    Vec3 tangent;
    Vec3 bitangent;
};

inline TangentBinormal computeTriangleTangent(
    const Vec3& p1,
    const Vec3& p2,
    const Vec3& p3,
    const Vec2& uv1,
    const Vec2& uv2,
    const Vec2& uv3 ) {
    Vec3 edge1    = p2 - p1;
    Vec3 edge2    = p3 - p1;
    Vec2 deltaUV1 = uv2 - uv1;
    Vec2 deltaUV2 = uv3 - uv1;

    float det = ( deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y );
    float f   = ( abs( det ) < 1e-6f ) ? 0.0f : 1.0f / det;

    TangentBinormal result;
    result.tangent.x = f * ( deltaUV2.y * edge1.x - deltaUV1.y * edge2.x );
    result.tangent.y = f * ( deltaUV2.y * edge1.y - deltaUV1.y * edge2.y );
    result.tangent.z = f * ( deltaUV2.y * edge1.z - deltaUV1.y * edge2.z );

    result.bitangent.x = f * ( -deltaUV2.x * edge1.x + deltaUV1.x * edge2.x );
    result.bitangent.y = f * ( -deltaUV2.x * edge1.y + deltaUV1.x * edge2.y );
    result.bitangent.z = f * ( -deltaUV2.x * edge1.z + deltaUV1.x * edge2.z );

    return result;
}

inline Vec3 kelvinToRGB( float k ) {
    float temp = clamp( k, 1000.0f, 40000.0f ) / 100.0f;

    float r, g, b;

    if ( temp <= 66.0f )
    {
        r = 255.0f;
    } else
    {
        r = temp - 60.0f;
        r = 329.698727446f * std::pow( r, -0.1332047592f );
        r = clamp( r, 0.0f, 255.0f );
    }

    if ( temp <= 66.0f )
    {
        g = temp;
        g = 99.4708025861f * std::log( g ) - 161.1195681661f;
        g = clamp( g, 0.0f, 255.0f );
    } else
    {
        g = temp - 60.0f;
        g = 288.1221695283f * std::pow( g, -0.0755148492f );
        g = clamp( g, 0.0f, 255.0f );
    }

    if ( temp >= 66.0f )
    {
        b = 255.0f;
    } else
    {
        if ( temp <= 19.0f )
        {
            b = 0.0f;
        } else
        {
            b = temp - 10.0f;
            b = 138.5177312231f * std::log( b ) - 305.0447927307f;
            b = clamp( b, 0.0f, 255.0f );
        }
    }

    Math::Vec3 sRGB = { r / 255.0f, g / 255.0f, b / 255.0f };

    // 3. IMPORTANTÍSIMO: Convertir de sRGB a Linear Space
    // Los motores PBR trabajan en Linear. Si no haces esto,
    // la luz se verá "lavada" y incorrecta matemáticamente.
    Math::Vec3 linearColor;
    linearColor.x = std::pow( sRGB.x, 2.2f );
    linearColor.y = std::pow( sRGB.y, 2.2f );
    linearColor.z = std::pow( sRGB.z, 2.2f );

    return linearColor;
}

} // namespace Math

AXION_NAMESPACE_END