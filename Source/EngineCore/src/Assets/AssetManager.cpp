#include "Axion/Core/Assets/AssetManager.h"
#include "Axion/Common/Logging.h"
#include "Loaders/Loaders.h"
#include <cmath>
#include <filesystem>
#include <mutex>
#include <queue>

AXION_NAMESPACE_BEGIN

namespace Core::Assets {

#pragma region Impl

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

    std::vector<AssetRecord<Texture>>              textures;
    std::unordered_map<std::string, TextureHandle> textureHandles;
    std::queue<uint>                               textureFreeIndices;

    std::vector<AssetRecord<Material>>              materials;
    std::unordered_map<std::string, MaterialHandle> materialHandles;
    std::queue<uint>                                materialFreeIndices;
    std::vector<uchar>                              dirtyMaterialLUT;

    std::mutex mutex;
    std::mutex dirtyMutex;

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

        auto deletedName = slot.asset->getName();

        if ( !deletedName.empty() )
        {
            meshHandles.erase( deletedName );
        }

        slot.asset.reset();
        slot.active = false;

        // will now have (handle.gen < slot.gen), causing isValid check to fail.
        slot.generation++;

        meshFreeIndices.push( handle.id );

        AXION_LOG_INFO( Logger::Module::Core, "Deleted Mesh ID: {} [{}]", handle.id, deletedName );
    }
    TextureHandle addTexture( Texture&& texture, const std::string& key = "" ) {

        uint id         = UINT32_MAX;
        uint generation = 0;

        if ( !textureFreeIndices.empty() )
        {
            id = textureFreeIndices.front();
            textureFreeIndices.pop();

            auto& slot  = textures[id];
            slot.active = true;
            slot.asset  = std::make_unique<Texture>( std::move( texture ) ); // Overwrite old data

            generation = slot.generation;
        } else
        {
            id         = static_cast<uint>( textures.size() );
            generation = 0;

            AssetRecord<Texture> newSlot;
            newSlot.asset      = std::make_unique<Texture>( std::move( texture ) );
            newSlot.generation = 0;
            newSlot.active     = true;

            textures.push_back( std::move( newSlot ) );
        }

        TextureHandle handle { id, generation };

        if ( !key.empty() )
        {
            textureHandles[key] = handle;
        }

        return handle;
    }

    void removeTexture( TextureHandle handle ) {
        if ( handle.id >= textures.size() )
            return;

        auto& slot = textures[handle.id];

        // Only delete if generation matches (security check)
        if ( !slot.active || slot.generation != handle.generation )
        {
            AXION_LOG_WARN( Logger::Module::Core, "Attempted to delete invalid or outdated Texture Handle ID: {}", handle.id );
            return;
        }

        auto deletedName = slot.asset->getName();

        if ( !deletedName.empty() )
        {
            textureHandles.erase( deletedName );
        }

        slot.asset.reset();
        slot.active = false;

        // will now have (handle.gen < slot.gen), causing isValid check to fail.
        slot.generation++;

        textureFreeIndices.push( handle.id );

        AXION_LOG_INFO( Logger::Module::Core, "Deleted Texture ID: {} [{}]", handle.id, deletedName );
    }

    MaterialHandle addMaterial( std::unique_ptr<Material> mat, const std::string& key = "" ) {
        uint id         = UINT32_MAX;
        uint generation = 0;

        if ( !materialFreeIndices.empty() )
        {
            id = materialFreeIndices.front();
            materialFreeIndices.pop();

            auto& slot  = materials[id];
            slot.active = true;
            slot.asset  = std::move( mat );
            generation  = slot.generation; // Keep existing generation count

            dirtyMaterialLUT[id] = true;

        } else
        {
            id         = static_cast<uint>( materials.size() );
            generation = 0;

            AssetRecord<Material> newSlot;
            newSlot.asset      = std::move( mat );
            newSlot.generation = 0;
            newSlot.active     = true;

            materials.push_back( std::move( newSlot ) );
            dirtyMaterialLUT.push_back( true );
        }

        MaterialHandle handle { id, generation };

        if ( !key.empty() )
        {
            materialHandles[key] = handle;
        }

        return handle;
    }

    void removeMaterial( MaterialHandle handle ) {
        if ( handle.id >= materials.size() )
            return;

        auto& slot = materials[handle.id];

        if ( !slot.active || slot.generation != handle.generation )
        {
            AXION_LOG_WARN( Logger::Module::Core, "Attempted to delete invalid Material Handle: {}", handle.id );
            return;
        }

        auto deletedName = slot.asset->getName();
        if ( !deletedName.empty() )
        {
            materialHandles.erase( deletedName );
        }

        // Reset
        slot.asset.reset();
        slot.active = false;
        slot.generation++; // Increment generation to invalidate old handles

        materialFreeIndices.push( handle.id );

        AXION_LOG_INFO( Logger::Module::Core, "Deleted Material ID: {}", handle.id );
    }
};

AssetManager::AssetManager()
    : _impl( std::make_unique<Impl>() ) {
    AXION_LOG_INFO( Logger::Module::Core, "Asset Manager Created Succesfully" );
}

AssetManager::~AssetManager() {
    AXION_LOG_INFO( Logger::Module::Core, "Destroying Asset Manager" );
}

#pragma endregion
#pragma region Mesh

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

    Loaders::MeshData meshData;
    bool              success = false;

    if ( extension == ".obj" )
    {
        success = Loaders::loadOBJ( filepath, meshData, flags );
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

    Graphics::PrimitiveTopology topology = Graphics::PrimitiveTopology::TriangleList;
    if ( flags & MeshImportAsLines )
    {
        topology = Graphics::PrimitiveTopology::LineList;
    } else if ( flags & MeshImportAsPoints )
    {
        topology = Graphics::PrimitiveTopology::PointList;
    }

    Mesh mesh( meshName,
               std::move( meshData.vertices ),
               std::move( meshData.indices ),
               topology,
               flags & MeshImportComputeBounds );

    if ( !success )
    {
        AXION_LOG_ERROR( Logger::Module::Core, "Failed to import mesh [{}] from [{}]", meshName, filepath );
        return MeshHandle();
    }

    auto handle = _impl->addMesh( std::move( mesh ), meshName );

    AXION_LOG_INFO( Logger::Module::Core, "Imported Mesh ID: {} [{}] from {}", handle.id, meshName, filepath );

    return handle;
}

MeshHandle AssetManager::createMesh( const std::string&          name,
                                     const std::vector<Vertex>&  vertices,
                                     const std::vector<uint>&    indices,
                                     Graphics::PrimitiveTopology topology ) {

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

    Mesh mesh( meshName );
    mesh._vertices = vertices;
    mesh._indices  = indices;
    mesh._topology = topology;

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

    Mesh mesh( meshName );

    const uint  cellsPerSide    = subdivisions + 1;
    const uint  verticesPerSide = cellsPerSide + 1;
    const float step            = 1.0f / (float)cellsPerSide;

    const Math::Vec3 origin = { -0.5f, -0.5f, 0.0f };

    mesh._vertices.reserve( verticesPerSide * verticesPerSide );
    mesh._indices.reserve( cellsPerSide * cellsPerSide * 6 );

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

            mesh._vertices.push_back( vert );
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

            mesh._indices.push_back( bottomLeft );
            mesh._indices.push_back( bottomRight );
            mesh._indices.push_back( topRight );

            mesh._indices.push_back( bottomLeft );
            mesh._indices.push_back( topRight );
            mesh._indices.push_back( topLeft );
        }
    }

    mesh._aabb.min              = { -0.5f, -0.5f, 0.0f };
    mesh._aabb.max              = { 0.5f, 0.5f, 0.0f };
    mesh._boundingSphere.center = { 0.0f, 0.0f, 0.0f };
    mesh._boundingSphere.radius = Math::distance( mesh._aabb.min, mesh._aabb.max ) * 0.5f;

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

    Mesh mesh( name );
    mesh._vertices = {
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
    mesh._indices = {
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

    mesh._aabb.min              = { -0.5f, -0.5f, -0.5f };
    mesh._aabb.max              = { 0.5f, 0.5f, 0.5f };
    mesh._boundingSphere.center = { 0, 0, 0 };
    mesh._boundingSphere.radius = Math::distance( mesh._aabb.min, mesh._aabb.max ) * 0.5f; // approx 0.866

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

    Mesh mesh( meshName );

    const uint  rings   = segments;
    const uint  sectors = segments;
    const float radius  = 0.5f; // Diameter = 1.0

    mesh._vertices.reserve( rings * sectors );
    mesh._indices.reserve( rings * sectors * 6 );

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

            mesh._vertices.push_back( v );
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

            mesh._indices.push_back( curRow + s );
            mesh._indices.push_back( nextRow + s );
            mesh._indices.push_back( nextRow + nextS );

            mesh._indices.push_back( curRow + s );
            mesh._indices.push_back( nextRow + nextS );
            mesh._indices.push_back( curRow + nextS );
        }
    }

    // Bounds
    mesh._aabb.min              = { -radius, -radius, -radius };
    mesh._aabb.max              = { radius, radius, radius };
    mesh._boundingSphere.center = { 0, 0, 0 };
    mesh._boundingSphere.radius = radius;

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

#pragma endregion
#pragma region Texture

TextureHandle AssetManager::importTexture( const std::string& name, const std::string& filepath, TextureImportFlags flags ) {
    std::scoped_lock lock( _impl->mutex );

    std::string textureName = name;
    if ( textureName.empty() )
    {
        AXION_LOG_ERROR( Logger::Module::Core, "Invalid Texture name." );
        return TextureHandle();
    }

    if ( _impl->textureHandles.count( textureName ) )
    {
        AXION_LOG_WARN( Logger::Module::Core, "Texture name collision [{}]. Returning existing handle.", textureName );
        return _impl->textureHandles[textureName];
    }

    if ( !std::filesystem::exists( filepath ) )
    {
        AXION_LOG_ERROR( Logger::Module::Core, "File not found: {}", filepath );
        return TextureHandle();
    }

    Loaders::ImageData imageData;
    if ( !Loaders::loadImage( filepath, imageData, flags ) )
    {
        AXION_LOG_ERROR( Logger::Module::Core, "Failed to import texture [{}] from [{}]", textureName, filepath );
        return TextureHandle();
    }

    Texture texture( textureName );

    TextureType textureType = TextureType::Texture2D;
    if ( flags & TextureImportAs3DTexture )
        textureType = TextureType::Texture3D;
    if ( flags & TextureImportAsCubeMap )
        textureType = TextureType::CubeMap;

    TextureFormat textureFormat = TextureFormat::Linear;
    if ( flags & TextureImportAsGamma )
        textureFormat = TextureFormat::Gamma;
    if ( imageData.isHDR )
        textureFormat = TextureFormat::HDR; // HDR overrides Gamma flags

    texture.setData( std::move( imageData.size ),
                     imageData.channels,
                     imageData.isHDR,
                     std::move( imageData.pixels ),
                     textureFormat,
                     imageData.precision,
                     textureType );

    uint8_t mipCount = 1;
    if ( flags & TextureImportGenerateMipmaps )
    {
        uint32_t maxDim = std::max( imageData.size.width, imageData.size.height );
        maxDim          = std::max( maxDim, imageData.size.depth );
        mipCount        = static_cast<uint8_t>( std::floor( std::log2( maxDim ) ) ) + 1;
    }

    texture.setSamplerDesc( { .anisotropic = ( flags & TextureImportAnisotropicFilter ) ? true : false,
                              .mipLevels   = mipCount } );

    auto handle = _impl->addTexture( std::move( texture ), textureName );

    AXION_LOG_INFO( Logger::Module::Core, "Imported Texture ID: {} [{}]", handle.id, textureName );

    return handle;
}

TextureHandle AssetManager::createTexture( const std::string&                      name,
                                           const Extent3D&                         size,
                                           const std::variant<std::vector<uchar>,
                                                              std::vector<float>>& pixels,
                                           const uint                              channels,
                                           const TextureFormat                     format,
                                           const TexturePrecision                  precision,
                                           const TextureType                       type,
                                           const SamplerDesc&                      samplerDesc ) {
    return TextureHandle();
}

const Texture* AssetManager::getTexture( TextureHandle handle ) const {
    if ( !handle.isValid() || handle.id >= _impl->textures.size() )
        return nullptr;

    const auto& slot = _impl->textures[handle.id];

    // GENERATION CHECK: Vital security feature
    if ( !slot.active || slot.generation != handle.generation )
    {
        // Handle is pointing to a slot that has been deleted and potentially reused
        return nullptr;
    }

    return slot.asset.get();
}
void AssetManager::deleteTexture( TextureHandle handle ) {
    std::scoped_lock lock( _impl->mutex );
    _impl->removeTexture( handle );
}
bool AssetManager::isValid( TextureHandle handle ) const {
    std::scoped_lock lock( _impl->mutex );

    if ( !handle.isValid() || handle.id >= _impl->textures.size() )
        return false;

    const auto& slot = _impl->textures[handle.id];

    if ( !slot.active || slot.generation != handle.generation )
        return false;

    return true;
}

uint AssetManager::getTextureCount() const {
    return _impl->textures.size();
}

#pragma endregion
#pragma region Material
MaterialHandle AssetManager::createMaterialAux( const std::string& name, Material* rawPtr ) {
    if ( !rawPtr )
        return {};

    std::unique_ptr<Material> matPtr( rawPtr );

    std::scoped_lock lock( _impl->mutex );

    auto handle = _impl->addMaterial( std::move( matPtr ), name );

    auto& slot = _impl->materials[handle.id];

    if ( slot.asset )
        slot.asset->setOwner( this, handle );

    notifyMaterialDirty( handle );

    return handle;
}

Material* AssetManager::getMaterialBase( MaterialHandle handle ) const {
    if ( !handle.isValid() || handle.id >= _impl->materials.size() )
        return nullptr;

    const auto& slot = _impl->materials[handle.id];

    if ( !slot.active || slot.generation != handle.generation )
    {
        return nullptr;
    }

    return slot.asset.get();
}
void AssetManager::deleteMaterial( MaterialHandle handle ) {
    std::scoped_lock lock( _impl->mutex );
    _impl->removeMaterial( handle );
}
bool AssetManager::isValid( MaterialHandle handle ) const {
    std::scoped_lock lock( _impl->mutex );

    if ( !handle.isValid() || handle.id >= _impl->materials.size() )
        return false;

    const auto& slot = _impl->materials[handle.id];

    if ( !slot.active || slot.generation != handle.generation )
        return false;

    return true;
}

uint AssetManager::getMaterialCount() const {
    return _impl->materials.size();
}

void AssetManager::notifyMaterialDirty( MaterialHandle handle, bool dirty ) {
    std::lock_guard<std::mutex> lock( _impl->dirtyMutex );
    _impl->dirtyMaterialLUT[handle.id] = dirty;
}
// o(1) complexity, fast and only one wait
std::pair<const uchar*, size_t> AssetManager::getMaterialDirtyLUT() const {
    std::lock_guard<std::mutex> lock( _impl->dirtyMutex );

    if ( _impl->dirtyMaterialLUT.empty() )
        return { nullptr, 0 };

    return { _impl->dirtyMaterialLUT.data(), _impl->dirtyMaterialLUT.size() };
}

#pragma endregion

#pragma region Entry

AssetManager::MeshBuilder AssetManager::mesh( const std::string& name ) {
    return AssetManager::MeshBuilder( *this, name );
}

AssetManager::TextureBuilder AssetManager::texture( const std::string& name ) {
    return AssetManager::TextureBuilder( *this, name );
}

AssetManager::MaterialBuilder AssetManager::material( const std::string& name ) {
    return AssetManager::MaterialBuilder( *this, name );
}
} // namespace Core::Assets
AXION_NAMESPACE_END