
#include "MeshUtils.h"

AXION_NAMESPACE_BEGIN

namespace Core::Assets::Loaders {

void computeTangents( std::vector<Vertex>& vertices, const std::vector<uint>& indices ) {

    std::vector<Math::Vec3> tempTangents( vertices.size(), { 0, 0, 0 } );
    std::vector<Math::Vec3> tempBitangents( vertices.size(), { 0, 0, 0 } );

    for ( size_t i = 0; i < indices.size(); i += 3 )
    {
        uint i0 = indices[i];
        uint i1 = indices[i + 1];
        uint i2 = indices[i + 2];

        auto tb = Math::computeTriangleTangent(
            vertices[i0].position,
            vertices[i1].position,
            vertices[i2].position,
            vertices[i0].uv,
            vertices[i1].uv,
            vertices[i2].uv );

        // (Weighted by area implicitly)
        tempTangents[i0] += tb.tangent;
        tempTangents[i1] += tb.tangent;
        tempTangents[i2] += tb.tangent;

        tempBitangents[i0] += tb.bitangent;
        tempBitangents[i1] += tb.bitangent;
        tempBitangents[i2] += tb.bitangent;
    }

    for ( size_t i = 0; i < vertices.size(); ++i )
    {
        Math::Vec3& n = vertices[i].normal;
        Math::Vec3& t = tempTangents[i];
        Math::Vec3& b = tempBitangents[i];

        // Gram-Schmidt Orthogonalization:
        // t = normalize(t - n * dot(n, t));
        Math::Vec3 t_ortho = Math::normalize( t - n * Math::dot( n, t ) );

        // Calculate Handedness (W)
        // Check if cross(n, t) has the same direction as b
        Math::Vec3 n_cross_t = Math::cross( n, t_ortho );
        float      w         = ( Math::dot( n_cross_t, b ) < 0.0f ) ? -1.0f : 1.0f;

        // Store Final Result
        vertices[i].tangent = { t_ortho.x, t_ortho.y, t_ortho.z, w };
    }
}
} // namespace Core::Assets::Loaders

AXION_NAMESPACE_END