#pragma once
#include <Axion/Core/Assets/Common.h>
#include <Axion/Core/Assets/Handle.h>
#include <Axion/Core/Assets/Material.h>
#include <Axion/Core/Assets/Mesh.h>
#include <Axion/Core/Assets/Texture.h>

AXION_NAMESPACE_BEGIN

namespace Core::Assets {

/**
 * @brief Centralized manager for CPU-side Assets.
 *
 * The AssetManager handles the lifecycle (creation, storage, retrieval, and destruction)
 * of core assets like Meshes, Textures, and Materials using a Generational Handle system.
 */
class AssetManager
{
public:
    AssetManager();
    ~AssetManager();
    AssetManager( const AssetManager& )            = delete;
    AssetManager& operator=( const AssetManager& ) = delete;

    // Forward declarations of nested builders
    class MeshBuilder;
    class TextureBuilder;
    class MaterialBuilder;

    // -------------------------------------------------------------------------
    // ENTRY POINTS
    // -------------------------------------------------------------------------

    /// @brief Starts the fluent construction of a CPU Mesh.
    /// @param name Debug name for the resource.
    MeshBuilder mesh( StringView name );

    /// @brief Starts the fluent construction of a CPU Texture.
    /// @param name Debug name for the resource.
    TextureBuilder texture( StringView name );

    /// @brief Starts the fluent construction of a CPU Material.
    /// @param name Debug name for the resource.
    MaterialBuilder material( StringView name );

    // -------------------------------------------------------------------------
    // RUNTIME ACCESS
    // -------------------------------------------------------------------------

    /// @brief Retrieves the raw Mesh pointer associated with a handle.
    /// @return Pointer to Mesh or nullptr if handle is invalid/dead.
    const Mesh* getMesh( MeshHandle handle ) const;

    /// @brief Destroys the mesh and frees memory immediately.
    void deleteMesh( MeshHandle handle );

    /// @brief Checks if a mesh handle points to a live asset.
    [[nodiscard]] bool isValid( MeshHandle handle ) const;

    /// @brief Retrieves the raw Texture pointer associated with a handle.
    /// @return Pointer to Texture or nullptr if handle is invalid/dead.
    const Texture* getTexture( TextureHandle handle ) const;

    /// @brief Destroys the texture and frees memory immediately.
    void deleteTexture( TextureHandle handle );

    /// @brief Checks if a texture handle points to a live asset.
    [[nodiscard]] bool isValid( TextureHandle handle ) const;

    /// @brief Retrieves the raw Material pointer associated with a handle.
    /// @return Pointer to Material or nullptr if handle is invalid/dead.
    template <typename T>
    T* getMaterial( MaterialHandle handle ) {
        Material* mat = getMaterialBase( handle );
        return static_cast<T*>( mat );
    }

    Material* getMaterialBase( MaterialHandle handle ) const;

    /// @brief Destroys the material and frees memory immediately.
    void deleteMaterial( MaterialHandle handle );

    /// @brief Checks if a material handle points to a live asset.
    [[nodiscard]] bool isValid( MaterialHandle handle ) const;

    // -------------------------------------------------------------------------
    // LIFECYCLE & UTILS
    // -------------------------------------------------------------------------

    /// @brief Returns the total number of mesh slots occupied.
    [[nodiscard]] u32 getMeshCount() const;

    /// @brief Returns the total number of texture slots occupied.
    [[nodiscard]] u32 getTextureCount() const;

    /// @brief Returns the total number of material slots occupied.
    [[nodiscard]] u32 getMaterialCount() const;

    void                           notifyMaterialDirty( MaterialHandle handle, bool dirty = true );
    std::pair<const byte*, size_t> getMaterialDirtyLUT() const;

private:
    MeshHandle    importMesh( StringView name, StringView filepath, MeshImportFlags flags );
    MeshHandle    createMesh( StringView                  name,
                              const STLW::Vector<Vertex>& vertices,
                              const STLW::Vector<u32>&    indices  = {},
                              Graphics::PrimitiveTopology topology = Graphics::PrimitiveTopology::TriangleList );
    MeshHandle    createQuad( StringView name, u32 subdivisions = 0, bool asMeshlet = false );
    MeshHandle    createCube( StringView name, bool asMeshlet = false );
    MeshHandle    createSphere( StringView name, u32 segments = 32, bool asMeshlet = false );
    TextureHandle importTexture( StringView name, StringView filepath, TextureImportFlags flags );
    TextureHandle createTexture( StringView                                                   name,
                                 const Extent3D&                                              size,
                                 const std::variant<STLW::Vector<byte>, STLW::Vector<float>>& pixels,
                                 const u32                                                    channels,
                                 const TextureFormat                                          format,
                                 const TexturePrecision                                       precision,
                                 const TextureType                                            type,
                                 const SamplerDesc&                                           samplerDesc = {} );

    template <typename T, typename... Args>
    MaterialHandle createMaterial( StringView name, Args&&... args ) {
        static_assert( std::is_base_of<Material, T>::value,
                       "AssetManager: The type T must derive from Core::Assets::Material" );

        T* newMat = new T( name, std::forward<Args>( args )... );

        return createMaterialAux( name, newMat );
    }
    MaterialHandle createMaterialAux( StringView name, Material* rawPtr );

    struct Impl;
    Memory::OwnerPtr<Impl> _impl = nullptr;

    friend class MeshBuilder;
    friend class TextureBuilder;
    friend class MaterialBuilder;
};

// -----------------------------------------------------------------------------
// BUILDER IMPLEMENTATIONS
// -----------------------------------------------------------------------------

/// @brief Fluent builder for configuring and creating Meshes.
class AssetManager::MeshBuilder
{
public:
    MeshBuilder( AssetManager& m, StringView n )
        : _manager( m )
        , _name( std::move( n ) ) {}

    /// @brief Imports a mesh from a file on disk (OBJ, GLTF, etc).
    MeshHandle import( StringView      path,
                       MeshImportFlags flags = MeshImportComputeBounds | MeshImportComputeTangents ) {
        if ( _asMeshlet )
            flags |= MeshImportAsMeshlet;
        return _manager.importMesh( _name, path, flags );
    }

    /// @brief Generates a procedural Unit Cube.
    MeshHandle createCube() {
        return _manager.createCube( _name, _asMeshlet );
    }

    /// @brief Generates a procedural Sphere (UV Sphere).
    MeshHandle createSphere( u32 segments = 32 ) {
        return _manager.createSphere( _name, segments, _asMeshlet );
    }

    /// @brief Generates a procedural Quad (Square).
    MeshHandle createQuad( u32 subdivisions = 0 ) {
        return _manager.createQuad( _name, subdivisions, _asMeshlet );
    }

    /// @brief Creates a mesh from raw vertex and index data.
    MeshHandle create( const STLW::Vector<Vertex>& vertices, const STLW::Vector<u32>& indices = {} ) {
        return _manager.createMesh( _name, vertices, indices );
    }

    /// @brief Flags the builder to process the geometry into Meshlets.
    MeshBuilder& asMeshlet( bool value = true ) {
        _asMeshlet = value;
        return *this;
    }

private:
    AssetManager& _manager;
    StringView    _name;
    bool          _asMeshlet = false;
};

/// @brief Fluent builder for configuring and creating Textures.
class AssetManager::MaterialBuilder
{
public:
    MaterialBuilder( AssetManager& m, StringView n )
        : _manager( m )
        , _name( std::move( n ) ) {}

    /// @brief Creates a material from a given class and constructor arguments.
    template <typename T, typename... Args>
    MaterialHandle create( Args&&... args ) {
        return _manager.createMaterial<T>( _name, std::forward<Args>( args )... );
    }

private:
    AssetManager& _manager;
    StringView    _name;
};

/// @brief Fluent builder for configuring and creating Textures.
class AssetManager::TextureBuilder
{
public:
    TextureBuilder( AssetManager& m, StringView n )
        : _manager( m )
        , _name( std::move( n ) ) {}

    /// @brief Configures texture as Linear (Non-sRGB).
    TextureBuilder& asLinear() {
        _fmt = TextureFormat::Linear;
        return *this;
    }

    /// @brief Configures texture as HDR (High Dynamic Range, F32).
    TextureBuilder& asHDR() {
        _fmt  = TextureFormat::HDR;
        _prec = TexturePrecision::F32;
        return *this;
    }

    /// @brief Sets the texture type (2D, 3D, CubeMap, etc).
    TextureBuilder& type( TextureType t ) {
        _type = t;
        return *this;
    }

    /// @brief Sets the sampler description for GPU usage hints.
    TextureBuilder& sampler( const SamplerDesc& desc ) {
        _sampler = desc;
        return *this;
    }

    // --- Finalizers ---

    /// @brief Finalizes and imports texture from a file on disk.
    TextureHandle import( StringView         path,
                          TextureImportFlags flags = TextureImportAsGamma | TextureImportForce4Channels ) {
        return _manager.importTexture( _name, path, flags );
    }

    /// @brief Finalizes and creates a texture from raw memory data.
    TextureHandle create( const Extent3D&                                              size,
                          const std::variant<STLW::Vector<byte>, STLW::Vector<float>>& pixels,
                          u32                                                          channels ) {
        return _manager.createTexture( _name, size, pixels, channels, _fmt, _prec, _type, _sampler );
    }

private:
    AssetManager& _manager;
    StringView    _name;

    TextureType      _type    = TextureType::Texture2D;
    TextureFormat    _fmt     = TextureFormat::Gamma;
    TexturePrecision _prec    = TexturePrecision::U8;
    SamplerDesc      _sampler = {};
};

} // namespace Core::Assets

AXION_NAMESPACE_END