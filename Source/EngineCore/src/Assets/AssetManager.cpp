#include "Axion/Core/Assets/AssetManager.h"
#include "Axion/Common/Logging.h"
#include "Loaders/Loaders.h"
#include <filesystem>
#include <mutex>
#include <queue>

AXION_NAMESPACE_BEGIN

namespace Core::Assets {

template <typename T>
struct AssetRecord {
    std::unique_ptr<T> asset      = nullptr;
    ushort             generation = 0;
    bool               active     = false;
};

struct AssetManager::Impl {

    Impl() {}
    ~Impl() {}

    std::vector<AssetRecord<Mesh>>              meshes;
    std::unordered_map<std::string, MeshHandle> meshHandles;
    std::queue<uint>                            meshFreeIndices;

    // std::vector<AssetRecord<Texture>>  textures;
    // std::unordered_map<std::string, TextureHandle>  textureHandles;
    // std::queue<uint>                            textureFreeIndices;

    // std::vector<AssetRecord<Material>> materials;
    // std::unordered_map<std::string, MaterialHandle> materialHandles;
    // std::queue<uint>                            materialFreeIndices;

    std::mutex mutex;

    MeshHandle addMesh( Mesh&& mesh, const std::string& key = "" ) {

        uint id         = UINT32_MAX;
        uint generation = 0;

        if ( !meshFreeIndices.empty() )
        {
            id = meshFreeIndices.front();
            meshFreeIndices.pop();

            auto& slot  = meshes[id];
            slot.active = true;
            slot.asset  = std::make_unique<Mesh>( std::move( mesh ) ); // Overwrite old data

            generation = slot.generation;
        } else
        {
            id         = static_cast<uint>( meshes.size() );
            generation = 0;

            AssetRecord<Mesh> newSlot;
            newSlot.asset      = std::make_unique<Mesh>( std::move( mesh ) );
            newSlot.generation = 0;
            newSlot.active     = true;

            meshes.push_back( std::move( newSlot ) );
        }

        MeshHandle handle { id, generation };

        if ( !key.empty() )
        {
            meshHandles[key] = handle;
        }

        return handle;
    }

    void removeMesh( MeshHandle handle ) {
        if ( handle.id >= meshes.size() )
            return;

        auto& slot = meshes[handle.id];

        // Only delete if generation matches (security check)
        if ( !slot.active || slot.generation != handle.generation )
        {
            AXION_LOG_WARN( Logger::Module::Core, "Attempted to delete invalid or outdated Mesh Handle ID: {}", handle.id );
            return;
        }

        auto deletedName = slot.asset->name;

        if ( !slot.asset->name.empty() )
        {
            meshHandles.erase( slot.asset->name );
        }

        slot.asset.reset();
        slot.active = false;

        // will now have (handle.gen < slot.gen), causing isValid check to fail.
        slot.generation++;

        meshFreeIndices.push( handle.id );

        AXION_LOG_INFO( Logger::Module::Core, "Deleted Mesh ID: {} [{}]", handle.id, deletedName );
    }
};

AssetManager::AssetManager()
    : _impl( std::make_unique<Impl>() ) {
    AXION_LOG_INFO( Logger::Module::Core, "Asset Manager Created Succesfully" );
}

AssetManager::~AssetManager() {
    AXION_LOG_INFO( Logger::Module::Core, "Destroying Asset Manager" );
}
MeshHandle AssetManager::importMesh( const std::string& name, const std::string& filepath, MeshImportFlags flags ) {
    std::scoped_lock lock( _impl->mutex );

    std::string meshName = name;
    if ( meshName.empty() )
    {
        AXION_LOG_ERROR( Logger::Module::Core, "Invalid Mesh name, returning invalid Handle" );
        return MeshHandle();
    }

    if ( _impl->meshHandles.count( meshName ) )
    {
        AXION_LOG_WARN( Logger::Module::Core, "Mesh name collision [{}]. Returning existing handle.", meshName );
        return _impl->meshHandles[meshName];
    }

    std::filesystem::path path( filepath );
    if ( !std::filesystem::exists( path ) )
    {
        AXION_LOG_ERROR( Logger::Module::Core, "File not found: {}", filepath );
        return MeshHandle();
    }

    std::string extension = path.extension().string();
    std::transform( extension.begin(), extension.end(), extension.begin(), ::tolower );

    Mesh mesh;
    mesh.name    = meshName;
    bool success = false;

    if ( extension == ".obj" )
    {
        success = Loaders::loadOBJ( filepath, mesh, flags );
    } else if ( extension == ".ply" )
    {
        // TODO: Implement loadPLY in Loaders.h/cpp
        // success = Loaders::loadPLY( filepath, mesh, flags );
        AXION_LOG_WARN( Logger::Module::Core, "PLY Loader not implemented yet [{}]", filepath );
    } else if ( extension == ".gltf" || extension == ".glb" )
    {
        // TODO: Implement loadGLTF using cgltf or tinygltf
        // success = Loaders::loadGLTF( filepath, mesh, flags );
        AXION_LOG_WARN( Logger::Module::Core, "GLTF Loader not implemented yet [{}]", filepath );
    } else
    {
        AXION_LOG_ERROR( Logger::Module::Core, "Unsupported file format [{}] for file: {}", extension, filepath );
        return MeshHandle();
    }

    if ( !success )
    {
        AXION_LOG_ERROR( Logger::Module::Core, "Failed to import mesh [{}] from [{}]", meshName, filepath );
        return MeshHandle();
    }

    auto handle = _impl->addMesh( std::move( mesh ), meshName );

    AXION_LOG_INFO( Logger::Module::Core, "Imported Mesh ID: {} [{}] from {}", handle.id, meshName, filepath );

    return handle;
}

MeshHandle AssetManager::createMesh( const std::string&         name,
                                     const std::vector<Vertex>& vertices,
                                     const std::vector<uint>&   indices ) {

    std::scoped_lock lock( _impl->mutex );
    std::string      meshName = name;
    if ( meshName.empty() )
    {
        AXION_LOG_ERROR( Logger::Module::Core, "Invalid Mesh name, returning invalid Handle" );
        return MeshHandle(); // Invalid
    }

    if ( !name.empty() && _impl->meshHandles.count( name ) )
    {
        AXION_LOG_WARN( Logger::Module::Core, "Mesh name collision [{}]. Returning existing handle.", name );
        return _impl->meshHandles[name];
    }

    Mesh mesh;
    mesh.name     = meshName;
    mesh.vertices = vertices;
    mesh.indices  = indices;

    mesh.calculateBounds();

    auto handle = _impl->addMesh( std::move( mesh ), name );

    AXION_LOG_INFO( Logger::Module::Core, "Created Mesh ID: {} [{}]", handle.id, name );

    return handle;
}

MeshHandle AssetManager::createQuad( const std::string& name, uint subdivisions ) {
    std::scoped_lock lock( _impl->mutex );
    std::string      meshName = name;
    if ( meshName.empty() )
    {
        meshName = "__internal_quad_subdiv_" + std::to_string( subdivisions );
    }

    if ( !name.empty() && _impl->meshHandles.count( name ) )
    {
        AXION_LOG_WARN( Logger::Module::Core, "Mesh name collision [{}]. Returning existing handle.", name );
        return _impl->meshHandles[name];
    }

    Mesh mesh;
    mesh.name = meshName;

    const uint  cellsPerSide    = subdivisions + 1;
    const uint  verticesPerSide = cellsPerSide + 1;
    const float step            = 1.0f / (float)cellsPerSide;

    const Math::Vec3 origin = { -0.5f, -0.5f, 0.0f };

    mesh.vertices.reserve( verticesPerSide * verticesPerSide );
    mesh.indices.reserve( cellsPerSide * cellsPerSide * 6 );

    for ( uint y = 0; y < verticesPerSide; ++y )
    {
        for ( uint x = 0; x < verticesPerSide; ++x )
        {
            float u = x * step;
            float v = y * step;

            Vertex vert;

            vert.position = {
                origin.x + u,
                origin.y + v,
                0.0f };

            vert.normal = { 0.0f, 0.0f, 1.0f };

            vert.uv = { u, v };

            vert.tangent = { 1.0f, 0.0f, 0.0f, 1.0f };

            vert.color = { 1.0f, 1.0f, 1.0f, 1.0f };

            mesh.vertices.push_back( vert );
        }
    }

    for ( uint y = 0; y < cellsPerSide; ++y )
    {
        for ( uint x = 0; x < cellsPerSide; ++x )
        {
            uint bottomLeft  = y * verticesPerSide + x;
            uint bottomRight = bottomLeft + 1;
            uint topLeft     = ( y + 1 ) * verticesPerSide + x;
            uint topRight    = topLeft + 1;

            mesh.indices.push_back( bottomLeft );
            mesh.indices.push_back( bottomRight );
            mesh.indices.push_back( topRight );

            mesh.indices.push_back( bottomLeft );
            mesh.indices.push_back( topRight );
            mesh.indices.push_back( topLeft );
        }
    }

    mesh.aabb.min              = { -0.5f, -0.5f, 0.0f };
    mesh.aabb.max              = { 0.5f, 0.5f, 0.0f };
    mesh.boundingSphere.center = { 0.0f, 0.0f, 0.0f };
    mesh.boundingSphere.radius = Math::distance( mesh.aabb.min, mesh.aabb.max ) * 0.5f;

    auto handle = _impl->addMesh( std::move( mesh ), name );

    AXION_LOG_INFO( Logger::Module::Core, "Created Quad ID: {} [{}] with {} subdivisions", handle.id, name, subdivisions );

    return handle;
}

MeshHandle AssetManager::createCube( const std::string& name ) {
    std::scoped_lock lock( _impl->mutex );

    if ( !name.empty() && _impl->meshHandles.count( name ) )
    {
        AXION_LOG_WARN( Logger::Module::Core, "Mesh name collision [{}]. Returning existing handle.", name );
        return _impl->meshHandles[name];
    }

    Mesh mesh;
    mesh.name     = name;
    mesh.vertices = {
        // ------------------------------------------------------------------
        // FRONT FACE (Z+) -> Normal (0, 0, 1) | Tangent (1, 0, 0)
        // ------------------------------------------------------------------
        { { -0.5f, -0.5f, 0.5f }, { 0, 0, 1 }, { 0, 1 }, { 1, 0, 0, 1 }, { 1, 1, 1, 1 } }, // 0: BL
        { { 0.5f, -0.5f, 0.5f }, { 0, 0, 1 }, { 1, 1 }, { 1, 0, 0, 1 }, { 1, 1, 1, 1 } },  // 1: BR
        { { 0.5f, 0.5f, 0.5f }, { 0, 0, 1 }, { 1, 0 }, { 1, 0, 0, 1 }, { 1, 1, 1, 1 } },   // 2: TR
        { { -0.5f, 0.5f, 0.5f }, { 0, 0, 1 }, { 0, 0 }, { 1, 0, 0, 1 }, { 1, 1, 1, 1 } },  // 3: TL

        // ------------------------------------------------------------------
        // BACK FACE (Z-) -> Normal (0, 0, -1) | Tangent (-1, 0, 0)
        // ------------------------------------------------------------------
        { { 0.5f, -0.5f, -0.5f }, { 0, 0, -1 }, { 0, 1 }, { -1, 0, 0, 1 }, { 1, 1, 1, 1 } },  // 4: BL (desde atrás)
        { { -0.5f, -0.5f, -0.5f }, { 0, 0, -1 }, { 1, 1 }, { -1, 0, 0, 1 }, { 1, 1, 1, 1 } }, // 5: BR
        { { -0.5f, 0.5f, -0.5f }, { 0, 0, -1 }, { 1, 0 }, { -1, 0, 0, 1 }, { 1, 1, 1, 1 } },  // 6: TR
        { { 0.5f, 0.5f, -0.5f }, { 0, 0, -1 }, { 0, 0 }, { -1, 0, 0, 1 }, { 1, 1, 1, 1 } },   // 7: TL

        // ------------------------------------------------------------------
        // LEFT FACE (X-) -> Normal (-1, 0, 0) | Tangent (0, 0, 1)
        // ------------------------------------------------------------------
        { { -0.5f, -0.5f, -0.5f }, { -1, 0, 0 }, { 0, 1 }, { 0, 0, 1, 1 }, { 1, 1, 1, 1 } }, // 8
        { { -0.5f, -0.5f, 0.5f }, { -1, 0, 0 }, { 1, 1 }, { 0, 0, 1, 1 }, { 1, 1, 1, 1 } },  // 9
        { { -0.5f, 0.5f, 0.5f }, { -1, 0, 0 }, { 1, 0 }, { 0, 0, 1, 1 }, { 1, 1, 1, 1 } },   // 10
        { { -0.5f, 0.5f, -0.5f }, { -1, 0, 0 }, { 0, 0 }, { 0, 0, 1, 1 }, { 1, 1, 1, 1 } },  // 11

        // ------------------------------------------------------------------
        // RIGHT FACE (X+) -> Normal (1, 0, 0) | Tangent (0, 0, -1)
        // ------------------------------------------------------------------
        { { 0.5f, -0.5f, 0.5f }, { 1, 0, 0 }, { 0, 1 }, { 0, 0, -1, 1 }, { 1, 1, 1, 1 } },  // 12
        { { 0.5f, -0.5f, -0.5f }, { 1, 0, 0 }, { 1, 1 }, { 0, 0, -1, 1 }, { 1, 1, 1, 1 } }, // 13
        { { 0.5f, 0.5f, -0.5f }, { 1, 0, 0 }, { 1, 0 }, { 0, 0, -1, 1 }, { 1, 1, 1, 1 } },  // 14
        { { 0.5f, 0.5f, 0.5f }, { 1, 0, 0 }, { 0, 0 }, { 0, 0, -1, 1 }, { 1, 1, 1, 1 } },   // 15

        // ------------------------------------------------------------------
        // TOP FACE (Y+) -> Normal (0, 1, 0) | Tangent (1, 0, 0)
        // ------------------------------------------------------------------
        { { -0.5f, 0.5f, 0.5f }, { 0, 1, 0 }, { 0, 1 }, { 1, 0, 0, 1 }, { 1, 1, 1, 1 } },  // 16
        { { 0.5f, 0.5f, 0.5f }, { 0, 1, 0 }, { 1, 1 }, { 1, 0, 0, 1 }, { 1, 1, 1, 1 } },   // 17
        { { 0.5f, 0.5f, -0.5f }, { 0, 1, 0 }, { 1, 0 }, { 1, 0, 0, 1 }, { 1, 1, 1, 1 } },  // 18
        { { -0.5f, 0.5f, -0.5f }, { 0, 1, 0 }, { 0, 0 }, { 1, 0, 0, 1 }, { 1, 1, 1, 1 } }, // 19

        // ------------------------------------------------------------------
        // BOTTOM FACE (Y-) -> Normal (0, -1, 0) | Tangent (1, 0, 0)
        // ------------------------------------------------------------------
        { { -0.5f, -0.5f, -0.5f }, { 0, -1, 0 }, { 0, 1 }, { 1, 0, 0, 1 }, { 1, 1, 1, 1 } }, // 20
        { { 0.5f, -0.5f, -0.5f }, { 0, -1, 0 }, { 1, 1 }, { 1, 0, 0, 1 }, { 1, 1, 1, 1 } },  // 21
        { { 0.5f, -0.5f, 0.5f }, { 0, -1, 0 }, { 1, 0 }, { 1, 0, 0, 1 }, { 1, 1, 1, 1 } },   // 22
        { { -0.5f, -0.5f, 0.5f }, { 0, -1, 0 }, { 0, 0 }, { 1, 0, 0, 1 }, { 1, 1, 1, 1 } }   // 23
    };

    // Indices (Counter-Clockwise - CCW)
    mesh.indices = {
        0, 1, 2, 2, 3, 0, // Front
        4,
        5,
        6,
        6,
        7,
        4, // Back
        8,
        9,
        10,
        10,
        11,
        8, // Left
        12,
        13,
        14,
        14,
        15,
        12, // Right
        16,
        17,
        18,
        18,
        19,
        16, // Top
        20,
        21,
        22,
        22,
        23,
        20 // Bottom
    };

    mesh.aabb.min              = { -0.5f, -0.5f, -0.5f };
    mesh.aabb.max              = { 0.5f, 0.5f, 0.5f };
    mesh.boundingSphere.center = { 0, 0, 0 };
    mesh.boundingSphere.radius = Math::distance( mesh.aabb.min, mesh.aabb.max ) * 0.5f; // approx 0.866

    auto handle = _impl->addMesh( std::move( mesh ), name );

    AXION_LOG_INFO( Logger::Module::Core, "Created Cube ID: {} [{}]", handle.id, name );

    return handle;
}

MeshHandle AssetManager::createSphere( const std::string& name, uint segments ) {
    std::scoped_lock lock( _impl->mutex );

    std::string meshName = name;
    if ( meshName.empty() )
    {
        meshName = "__internal_sphere_seg_" + std::to_string( segments );
    }

    if ( _impl->meshHandles.count( meshName ) )
    {
        AXION_LOG_WARN( Logger::Module::Core, "Mesh name collision [{}]. Returning existing handle.", meshName );
        return _impl->meshHandles[meshName];
    }

    Mesh mesh;
    mesh.name = meshName;

    const uint  rings   = segments;
    const uint  sectors = segments;
    const float radius  = 0.5f; // Diameter = 1.0

    mesh.vertices.reserve( rings * sectors );
    mesh.indices.reserve( rings * sectors * 6 );

    const float R = 1.0f / (float)( rings - 1 );
    const float S = 1.0f / (float)( sectors - 1 );

    for ( uint r = 0; r < rings; ++r )
    {
        for ( uint s = 0; s < sectors; ++s )
        {
            float y = Math::sin( -Math::PI_HALF + Math::PI * r * R );
            float x = Math::cos( 2 * Math::PI * s * S ) * Math::sin( Math::PI * r * R );
            float z = Math::sin( 2 * Math::PI * s * S ) * Math::sin( Math::PI * r * R );

            Vertex v;
            v.position = { x * radius * 2.0f, y * radius * 2.0f, z * radius * 2.0f }; // Scale to diameter 1

            // For a unit sphere at origin, normal is just the normalized position
            v.normal = Math::normalize( v.position );

            v.uv = { s * S, r * R };

            // Calculate Tangent
            // Tangent is perpendicular to Normal and Up(0,1,0), pointing roughly East
            // Or simpler: derivative with respect to texture coordinate U (longitude)
            // T = (-sin(theta)sin(phi), 0, cos(theta)sin(phi))
            v.tangent.x = -Math::sin( 2 * Math::PI * s * S );
            v.tangent.y = 0.0f;
            v.tangent.z = Math::cos( 2 * Math::PI * s * S );
            v.tangent.w = 1.0f; // Handedness

            Math::Vec3 t3 = { v.tangent.x, v.tangent.y, v.tangent.z };
            t3            = Math::normalize( t3 );
            v.tangent     = { t3.x, t3.y, t3.z, 1.0f };

            v.color = { 1.0f, 1.0f, 1.0f, 1.0f };

            mesh.vertices.push_back( v );
        }
    }

    // Indices
    for ( uint r = 0; r < rings - 1; ++r )
    {
        for ( uint s = 0; s < sectors - 1; ++s )
        {
            uint curRow  = r * sectors;
            uint nextRow = ( r + 1 ) * sectors;

            uint nextS = ( s + 1 );

            mesh.indices.push_back( curRow + s );
            mesh.indices.push_back( nextRow + s );
            mesh.indices.push_back( nextRow + nextS );

            mesh.indices.push_back( curRow + s );
            mesh.indices.push_back( nextRow + nextS );
            mesh.indices.push_back( curRow + nextS );
        }
    }

    // Bounds
    mesh.aabb.min              = { -radius, -radius, -radius };
    mesh.aabb.max              = { radius, radius, radius };
    mesh.boundingSphere.center = { 0, 0, 0 };
    mesh.boundingSphere.radius = radius;

    auto handle = _impl->addMesh( std::move( mesh ), meshName );
    AXION_LOG_INFO( Logger::Module::Core, "Created Sphere ID: {} [{}]", handle.id, meshName );

    return handle;
}

const Mesh* AssetManager::getMesh( MeshHandle handle ) const {
    if ( !handle.isValid() || handle.id >= _impl->meshes.size() )
        return nullptr;

    const auto& slot = _impl->meshes[handle.id];

    // GENERATION CHECK: Vital security feature
    if ( !slot.active || slot.generation != handle.generation )
    {
        // Handle is pointing to a slot that has been deleted and potentially reused
        return nullptr;
    }

    return slot.asset.get();
}

void AssetManager::deleteMesh( MeshHandle handle ) {
    std::scoped_lock lock( _impl->mutex );
    _impl->removeMesh( handle );
}
bool AssetManager::isValid( MeshHandle handle ) const {
    std::scoped_lock lock( _impl->mutex );

    if ( !handle.isValid() || handle.id >= _impl->meshes.size() )
        return false;

    const auto& slot = _impl->meshes[handle.id];

    if ( !slot.active || slot.generation != handle.generation )
        return false;

    return true;
}
uint AssetManager::getMeshCount() const {
    return _impl->meshes.size();
}
} // namespace Core::Assets
AXION_NAMESPACE_END