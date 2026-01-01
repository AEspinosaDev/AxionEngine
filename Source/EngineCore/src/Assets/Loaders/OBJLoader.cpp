#include "Loaders.h"
#include <Axion/Common/Helpers.h>
#include <functional>
#include <tiny_obj_loader.h>

AXION_NAMESPACE_BEGIN

namespace Core::Assets::Loaders {

struct IndexHasher {
    std::size_t operator()( const tinyobj::index_t& index ) const {
        std::size_t seed = 0;
        Helpers::hashCombine( seed, index.vertex_index );
        Helpers::hashCombine( seed, index.normal_index );
        Helpers::hashCombine( seed, index.texcoord_index );
        return seed;
    }
};

struct IndexEqual {
    bool operator()( const tinyobj::index_t& a, const tinyobj::index_t& b ) const {
        return a.vertex_index == b.vertex_index &&
               a.normal_index == b.normal_index &&
               a.texcoord_index == b.texcoord_index;
    }
};

bool loadOBJ( const std::string& filepath, MeshData& outMesh, MeshImportFlags flags ) {

    tinyobj::attrib_t                attrib;
    std::vector<tinyobj::shape_t>    shapes;
    std::vector<tinyobj::material_t> materials;
    std::string                      warn, err;

    // Fetch materials
    std::string baseDir = filepath.substr( 0, filepath.find_last_of( "/\\" ) + 1 );

    const char* mtlSearchPath         = baseDir.c_str();
    bool        shouldImportMaterials = ( flags & MeshImportLoadMaterials );

    bool ret = tinyobj::LoadObj( &attrib,
                                 &shapes,
                                 shouldImportMaterials ? &materials : nullptr,
                                 &warn,
                                 &err,
                                 filepath.c_str(),
                                 shouldImportMaterials ? mtlSearchPath : nullptr );

    if ( !warn.empty() )
        AXION_LOG_WARN( Logger::Module::Core, "TinyObj Loader Warning [{}]: {}", filepath, warn );

    if ( !err.empty() )
        AXION_LOG_ERROR( Logger::Module::Core, "TinyObj Loader Error [{}]: {}", filepath, err );

    if ( !ret )
        return false;

    outMesh.vertices.clear();
    outMesh.indices.clear();

    size_t totalIndices = 0;
    for ( const auto& shape : shapes )
    {
        totalIndices += shape.mesh.indices.size();
    }
    outMesh.indices.reserve( totalIndices );
    size_t estimatedVertices = attrib.vertices.size() / 3;
    outMesh.vertices.reserve( estimatedVertices );

    // Deduplicación: Key(OBJ Index) -> Value(New Mesh Index)
    std::unordered_map<tinyobj::index_t, uint, IndexHasher, IndexEqual> uniqueVertices;
    uniqueVertices.reserve( estimatedVertices );

    // Merge all shapes into a single Mesh
    for ( const auto& shape : shapes )
    {
        // tinyobjloader triangles by Default
        for ( const auto& index : shape.mesh.indices )
        {
            if ( uniqueVertices.count( index ) == 0 )
            {
                Vertex vertex {};

                // --- POSITION ---
                vertex.position.x = attrib.vertices[3 * index.vertex_index + 0];
                vertex.position.y = attrib.vertices[3 * index.vertex_index + 1];
                vertex.position.z = attrib.vertices[3 * index.vertex_index + 2];

                // --- NORMAL ---
                if ( index.normal_index >= 0 )
                {
                    vertex.normal.x = attrib.normals[3 * index.normal_index + 0];
                    vertex.normal.y = attrib.normals[3 * index.normal_index + 1];
                    vertex.normal.z = attrib.normals[3 * index.normal_index + 2];
                } else
                {
                    vertex.normal = { 0.0f, 1.0f, 0.0f };
                }

                // --- UV ---
                if ( index.texcoord_index >= 0 )
                {
                    vertex.uv.x = attrib.texcoords[2 * index.texcoord_index + 0];
                    vertex.uv.y = 1.0f - attrib.texcoords[2 * index.texcoord_index + 1];
                } else
                {
                    vertex.uv = { 0.0f, 0.0f };
                }

                // --- COLOR ---
                vertex.color = { 1.0f, 1.0f, 1.0f, 1.0f };
                if ( !attrib.colors.empty() )
                {
                    vertex.color.x = attrib.colors[3 * index.vertex_index + 0];
                    vertex.color.y = attrib.colors[3 * index.vertex_index + 1];
                    vertex.color.z = attrib.colors[3 * index.vertex_index + 2];
                }

                // --- TANGENT ---
                vertex.tangent = { 1.0f, 0.0f, 0.0f, 1.0f };

                // Store new Index
                uniqueVertices[index] = static_cast<uint>( outMesh.vertices.size() );
                outMesh.vertices.push_back( vertex );
            }

            outMesh.indices.push_back( uniqueVertices[index] );
        }
    }

    if ( flags & MeshImportComputeTangents )
        computeTangents( outMesh.vertices, outMesh.indices );

    return true;
}

// bool loadOBJ( const std::string& filepath, std::unordered_map<Mesh, Material>& assetMap, MeshImportFlags flags ) {
//     return false;
// }

} // namespace Core::Assets::Loaders

AXION_NAMESPACE_END