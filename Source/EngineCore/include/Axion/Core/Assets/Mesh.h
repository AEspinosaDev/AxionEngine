#pragma once
#include <Axion/Common/Graphics/Common.h>
#include <Axion/Common/Math.h>
#include <Axion/Common/Memory/Pointers/OwnerPtr.h>
#include <Axion/Common/Memory/Pointers/SharedPtr.h>
#include <Axion/Core/Assets/Common.h>



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
    u32 vertexOffset;
    u32 vertexCount;
    u32 triangleOffset;
    u32 triangleCount;

    // Culling bounds
    float center[3];
    float radius;

    // Cone culling for backface rejection
    float coneApex[3];
    s8    coneAxis[3];
    s8    coneCutoff;
};

// Meshlet data
struct MeshletData {
    STLW::Vector<Meshlet> meshlets;
    STLW::Vector<u32>     vertexIndices;    // Points to the original Vertex buffer
    STLW::Vector<byte>    primitiveIndices; // Local indices (0-63) for triangles
};

struct GeometryData {
    STLW::Vector<Vertex> vertices;
    STLW::Vector<u32>    indices;
    // Meshlet data is optional and only generated for meshes that meet certain criteria.
    Memory::OwnerPtr<MeshletData> meshlets = nullptr;
};

class AssetManager;

class Mesh
{
public:
    Mesh()                  = delete;
    Mesh( const Mesh& )     = delete;
    Mesh( Mesh&& ) noexcept = default;

    [[nodiscard]] const String64&             getName() const { return _name; }
    [[nodiscard]] const Math::AABB&           getAABB() const { return _aabb; }
    [[nodiscard]] const Math::BoundingSphere& getBoundingSphere() const { return _boundingSphere; }

    [[nodiscard]] const STLW::Vector<Vertex>& getVertices() const { return _geoData->vertices; }
    [[nodiscard]] const STLW::Vector<u32>&    getIndices() const { return _geoData->indices; }
    [[nodiscard]] const MeshletData*          getMeshletData() const { return hasMeshlets() ? _geoData->meshlets.get() : nullptr; }

    [[nodiscard]] u32                         getVertexCount() const { return _geoData ? (u32)_geoData->vertices.size() : 0; }
    [[nodiscard]] u32                         getIndexCount() const { return _geoData ? (u32)_geoData->indices.size() : 0; }
    [[nodiscard]] Graphics::PrimitiveTopology getTopology() const { return _topology; }
    [[nodiscard]] bool                        needsAS() const { return _needsAS; }
    [[nodiscard]] bool                        hasMeshlets() const { return _geoData && _geoData->meshlets != nullptr; }

    std::shared_ptr<GeometryData> getGeometryDataRef() const { return _geoData; }

private:
    friend class AssetManager;

    explicit Mesh( StringView                  name,
                   STLW::Vector<Vertex>&&      verts,
                   STLW::Vector<u32>&&         inds,
                   Graphics::PrimitiveTopology topology      = Graphics::PrimitiveTopology::TriangleList,
                   bool                        computeBounds = true )
        : _name( name )
        , _topology( topology ) {

        _geoData           = std::make_shared<GeometryData>();
        _geoData->vertices = std::move( verts );
        _geoData->indices  = std::move( inds );

        if ( computeBounds )
            calculateBounds();
    }
    explicit Mesh( StringView                  name,
                   STLW::Vector<Vertex>&&      verts,
                   MeshletData&&               meshletData,
                   Graphics::PrimitiveTopology topology      = Graphics::PrimitiveTopology::TriangleList,
                   bool                        computeBounds = true )
        : _name( name )
        , _topology( topology ) {

        _geoData           = std::make_shared<GeometryData>();
        _geoData->vertices = std::move( verts );
        _geoData->meshlets = Memory::makeOwned<MeshletData>( std::move( meshletData ) );

        if ( computeBounds )
            calculateBounds();
    }
    explicit Mesh( StringView name )
        : _name( name )
        , _geoData( std::make_shared<GeometryData>() ) {}

    String64                      _name;
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