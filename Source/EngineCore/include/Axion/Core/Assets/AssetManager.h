#pragma once
#include <Axion/Core/Assets/Handle.h>
#include <Axion/Core/Assets/Material.h>
#include <Axion/Core/Assets/Mesh.h>
#include <Axion/Core/Assets/Texture.h>

AXION_NAMESPACE_BEGIN

namespace Core::Assets {

/**
 * @brief Bitmask flags to configure the mesh import process.
 * These flags control post-processing steps and additional resource loading.
 */
enum MeshImportFlags : uint
{
    MeshImportNone            = 1 << 0,
    MeshImportLoadMaterials   = 1 << 1,
    MeshImportLoadTextures    = 1 << 2,
    MeshImportLoadAnimations  = 1 << 3,
    MeshImportComputeTangents = 1 << 4,
    MeshImportComputeBounds   = 1 << 5,
};

AXION_ENUM_CLASS_FLAG_OPERATORS( MeshImportFlags );

/**
 * @brief Centralized manager for CPU-side Assets.
 *
 * The AssetManager handles the lifecycle (creation, storage, retrieval, and destruction)
 * of core assets like Meshes, Textures, and Materials.
 *
 * It uses a Handle-based system with Generational indices to prevent "Dangling Pointer"
 * issues when accessing assets that may have been unloaded.
 */
class AssetManager
{
public:
    AssetManager();
    ~AssetManager();
    AssetManager( const AssetManager& )            = delete;
    AssetManager& operator=( const AssetManager& ) = delete;

    // ------------------------------------------------------------------------
    // Mesh Management
    // ------------------------------------------------------------------------

    /**
     * @brief Imports a mesh from a file on the disk.
     *
     * Automatically detects the file format based on extension (.obj, .ply, .gltf).
     * If a mesh with the same name already exists, returns the existing handle.
     *
     * @param name Unique identifier for the mesh in the asset registry.
     * @param filepath Absolute or relative path to the asset file.
     * @param flags Configuration flags for post-processing (Default: Compute Bounds & Tangents).
     * @return A valid MeshHandle on success, or an invalid handle on failure.
     */
    MeshHandle importMesh( const std::string& name,
                           const std::string& filepath,
                           MeshImportFlags    flags = MeshImportComputeBounds | MeshImportComputeTangents );

    /**
     * @brief Creates a new mesh manually from raw vertex and index buffers.
     *
     * @param name Unique identifier for the mesh.
     * @param vertices Vector of vertex data (position, normal, uv, etc.).
     * @param indices Vector of indices defining the triangles.
     * @return Handle to the newly created mesh.
     */
    MeshHandle createMesh( const std::string&         name,
                           const std::vector<Vertex>& vertices,
                           const std::vector<uint>&   indices = {} );

    // ------------------------------------------------------------------------
    // Procedural Primitives
    // ------------------------------------------------------------------------

    /**
     * @brief Generates a procedural Quad (Square) mesh.
     * Useful for UI, billboards, or debug planes.
     *
     * @param name Name of the asset. If empty, an internal name is generated.
     * @param subdivisions Number of tessellation steps (0 = 2 triangles).
     * @return Handle to the procedural mesh.
     */
    MeshHandle createQuad( const std::string& name, uint subdivisions = 0 );

    /**
     * @brief Generates a procedural Unit Cube mesh.
     * Includes hard normals and UVs for each face.
     *
     * @param name Name of the asset.
     * @return Handle to the procedural mesh.
     */
    MeshHandle createCube( const std::string& name );

    /**
     * @brief Generates a procedural Sphere (UV Sphere).
     *
     * @param name Name of the asset.
     * @param segments Number of horizontal and vertical subdivisions (Resolution).
     * @return Handle to the procedural mesh.
     */
    MeshHandle createSphere( const std::string& name, uint segments = 32 );

    // ------------------------------------------------------------------------
    // Access & Lifecycle
    // ------------------------------------------------------------------------

    /**
     * @brief Retrieves a read-only pointer to the mesh data.
     *
     * @warning The returned pointer is managed by the AssetManager.
     * Do NOT delete it manually. Do not store this pointer long-term,
     * as resizing the internal pool might invalidate it (unless stable storage is used).
     *
     * @param handle The handle obtained during creation/import.
     * @return Const pointer to the Mesh, or nullptr if the handle is invalid or stale.
     */
    const Mesh* getMesh( MeshHandle handle ) const;

    /**
     * @brief Unloads a mesh and frees its memory.
     *
     * Increments the generation counter for the slot, invalidating any
     * existing handles that point to this asset.
     *
     * @param handle The handle of the mesh to delete.
     */
    void deleteMesh( MeshHandle handle );

    // TextureHandle importTexture( const std::string& name, const std::string& filepath );

    /**
     * @brief Checks if a handle points to a valid, live asset.
     *
     * Verifies that the ID exists and that the generation matches the current
     * asset version (protects against accessing deleted/reused slots).
     *
     * @param handle The handle to check.
     * @return True if valid.
     */
    bool isValid( MeshHandle handle ) const;

    /**
     * @brief Gets the total number of active meshes currently loaded.
     * @return Count of active meshes.
     */
    uint getMeshCount() const;

private:
    struct Impl;
    std::unique_ptr<Impl> _impl;
};

} // namespace Core::Assets

AXION_NAMESPACE_END