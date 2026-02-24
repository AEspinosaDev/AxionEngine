#pragma once
#include <Axion/Common/Graphics/Defines.h>
#include <Axion/Common/Math.h>
#include <Axion/Core/Render/Defines.h>

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

// Meshlet metadata matching the GPU structure (Aligned to 48 bytes)
struct Meshlet {
    uint vertexOffset;
    uint vertexCount;
    uint triangleOffset;
    uint triangleCount;

    // Culling bounds
    float center[3];
    float radius;

    // Cone culling for backface rejection
    float  coneApex[3];
    int8_t coneAxis[3];
    int8_t coneCutoff;
};

// Meshlet data
struct MeshletData {
    std::vector<Meshlet> meshlets;
    std::vector<uint>    vertexIndices;    // Points to the original Vertex buffer
    std::vector<uchar>   primitiveIndices; // Local indices (0-63) for triangles
};

struct GeometryData {
    std::vector<Vertex> vertices;
    std::vector<uint>   indices;
    // Meshlet data is optional and only generated for meshes that meet certain criteria.
    std::unique_ptr<MeshletData> meshlets = nullptr;
};

class AssetManager;

class Mesh
{
public:
    Mesh()                  = delete;
    Mesh( const Mesh& )     = delete;
    Mesh( Mesh&& ) noexcept = default;

    [[nodiscard]] const std::string&          getName() const { return _name; }
    [[nodiscard]] const Math::AABB&           getAABB() const { return _aabb; }
    [[nodiscard]] const Math::BoundingSphere& getBoundingSphere() const { return _boundingSphere; }

    [[nodiscard]] const std::vector<Vertex>& getVertices() const { return _geoData->vertices; }
    [[nodiscard]] const std::vector<uint>&   getIndices() const { return _geoData->indices; }
    [[nodiscard]] const MeshletData*         getMeshletData() const { return hasMeshlets() ? _geoData->meshlets.get() : nullptr; }

    [[nodiscard]] uint                        getVertexCount() const { return _geoData ? (uint)_geoData->vertices.size() : 0; }
    [[nodiscard]] uint                        getIndexCount() const { return _geoData ? (uint)_geoData->indices.size() : 0; }
    [[nodiscard]] Graphics::PrimitiveTopology getTopology() const { return _topology; }
    [[nodiscard]] bool                        needsAS() const { return _needsAS; }
    [[nodiscard]] bool                        hasMeshlets() const { return _geoData && _geoData->meshlets != nullptr; }

    std::shared_ptr<GeometryData> getGeometryDataRef() const { return _geoData; }

private:
    friend class AssetManager;

    explicit Mesh( std::string                 name,
                   std::vector<Vertex>&&       verts,
                   std::vector<uint>&&         inds,
                   Graphics::PrimitiveTopology topology      = Graphics::PrimitiveTopology::TriangleList,
                   bool                        computeBounds = true )
        : _name( std::move( name ) )
        , _topology( topology ) {

        _geoData           = std::make_shared<GeometryData>();
        _geoData->vertices = std::move( verts );
        _geoData->indices  = std::move( inds );

        if ( computeBounds )
            calculateBounds();
    }
    explicit Mesh( std::string                 name,
                   std::vector<Vertex>&&       verts,
                   MeshletData&&               meshletData,
                   Graphics::PrimitiveTopology topology      = Graphics::PrimitiveTopology::TriangleList,
                   bool                        computeBounds = true )
        : _name( std::move( name ) )
        , _topology( topology ) {

        _geoData           = std::make_shared<GeometryData>();
        _geoData->vertices = std::move( verts );
        _geoData->meshlets = std::make_unique<MeshletData>( std::move( meshletData ) );

        if ( computeBounds )
            calculateBounds();
    }
    explicit Mesh( std::string name )
        : _name( std::move( name ) )
        , _geoData( std::make_shared<GeometryData>() ) {}

    std::string                   _name;
    std::shared_ptr<GeometryData> _geoData;
    Math::AABB                    _aabb {};
    Math::BoundingSphere          _boundingSphere {};

    // Topology
    Graphics::PrimitiveTopology _topology = Graphics::PrimitiveTopology::TriangleList;

    // Needs AS
    bool _needsAS = false;

    void calculateBounds() {
        _aabb = Math::AABB();
        for ( const auto& v : _geoData->vertices )
            _aabb.merge( v.position );

        _boundingSphere.center = _aabb.getCenter();
        _boundingSphere.radius = Math::distance( _aabb.min, _aabb.max ) * 0.5f; // Fast approx
    }
};

} // namespace Core::Assets

AXION_NAMESPACE_END