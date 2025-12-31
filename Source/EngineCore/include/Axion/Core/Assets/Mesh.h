#pragma once
#include <Axion/Common/Defines.h>
#include <Axion/Common/Math.h>

AXION_NAMESPACE_BEGIN

namespace Core::Assets {

// Canonical Vertex
// In the future it would be nice to have a CustomVertex definition
// Aligned to 16 32Bytes values (Perfect for GPU)
struct Vertex {
    Math::Vec3 position;
    Math::Vec3 normal;
    Math::Vec2 uv;
    Math::Vec4 tangent;
    Math::Vec4 color;
};

// struct Vertex2{
//     std::set<>
// }

struct Mesh {
    std::string name;

    std::vector<Vertex> vertices;
    std::vector<uint>   indices;

    Math::AABB           aabb;
    Math::BoundingSphere boundingSphere;

    void calculateBounds() {
        aabb = Math::AABB();
        for ( const auto& v : vertices )
            aabb.merge( v.position );

        boundingSphere.center = aabb.getCenter();
        boundingSphere.radius = Math::distance( aabb.min, aabb.max ) * 0.5f; // Fast approx
    }
};

} // namespace Core::Assets

AXION_NAMESPACE_END