
#include "MeshUtils.h"

AXION_NAMESPACE_BEGIN

namespace Core::Assets::Loaders {

void computeTangents( std::vector<Vertex>& vertices, const std::vector<u32>& indices ) {

    std::vector<Math::Vec3> tempTangents( vertices.size(), { 0, 0, 0 } );
    std::vector<Math::Vec3> tempBitangents( vertices.size(), { 0, 0, 0 } );

    for ( size_t i = 0; i < indices.size(); i += 3 )
    {
        u32 i0 = indices[i];
        u32 i1 = indices[i + 1];
        u32 i2 = indices[i + 2];

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
MeshletData cookMeshlets( const std::vector<Vertex>& vertices, const std::vector<u32>& indices ) {
    Assets::MeshletData meshletData;

    // Hardware optimal settings for DX12 / Vulkan
    const size_t maxVertices  = 64;
    const size_t maxTriangles = 126;
    const float  coneWeight   = 0.0f;

    size_t maxMeshlets = meshopt_buildMeshletsBound( indices.size(), maxVertices, maxTriangles );

    std::vector<meshopt_Meshlet> rawMeshlets( maxMeshlets );
    meshletData.vertexIndices.resize( maxMeshlets * maxVertices );
    meshletData.primitiveIndices.resize( maxMeshlets * maxTriangles * 3 + 3 );

    size_t meshletCount = meshopt_buildMeshlets(
        rawMeshlets.data(),
        meshletData.vertexIndices.data(),
        meshletData.primitiveIndices.data(),
        indices.data(),
        indices.size(),
        &vertices[0].position.x,
        vertices.size(),
        sizeof( Core::Assets::Vertex ),
        maxVertices,
        maxTriangles,
        coneWeight );

    rawMeshlets.resize( meshletCount );

    if ( meshletCount > 0 )
    {
        const meshopt_Meshlet& last = rawMeshlets.back();
        meshletData.vertexIndices.resize( last.vertex_offset + last.vertex_count );
        meshletData.primitiveIndices.resize( last.triangle_offset + ( ( last.triangle_count * 3 + 3 ) & ~3 ) );
    }

    meshletData.meshlets.reserve( meshletCount );
    for ( const auto& m : rawMeshlets )
    {
        Core::Assets::Meshlet outMeshlet = {};
        outMeshlet.vertexOffset          = m.vertex_offset;
        outMeshlet.vertexCount           = m.vertex_count;
        outMeshlet.triangleOffset        = m.triangle_offset;
        outMeshlet.triangleCount         = m.triangle_count;

        meshopt_Bounds bounds = meshopt_computeMeshletBounds(
            &meshletData.vertexIndices[m.vertex_offset],
            &meshletData.primitiveIndices[m.triangle_offset],
            m.triangle_count,
            &vertices[0].position.x,
            vertices.size(),
            sizeof( Core::Assets::Vertex ) );

        outMeshlet.center[0] = bounds.center[0];
        outMeshlet.center[1] = bounds.center[1];
        outMeshlet.center[2] = bounds.center[2];
        outMeshlet.radius    = bounds.radius;

        outMeshlet.coneApex[0] = bounds.cone_apex[0];
        outMeshlet.coneApex[1] = bounds.cone_apex[1];
        outMeshlet.coneApex[2] = bounds.cone_apex[2];

        outMeshlet.coneAxis[0] = bounds.cone_axis_s8[0];
        outMeshlet.coneAxis[1] = bounds.cone_axis_s8[1];
        outMeshlet.coneAxis[2] = bounds.cone_axis_s8[2];
        outMeshlet.coneCutoff  = bounds.cone_cutoff_s8;

        meshletData.meshlets.push_back( outMeshlet );
    }

    return meshletData;
}
} // namespace Core::Assets::Loaders

AXION_NAMESPACE_END