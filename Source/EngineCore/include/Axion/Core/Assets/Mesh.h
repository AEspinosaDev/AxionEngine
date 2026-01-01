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
    [[nodiscard]] const std::vector<Vertex>&  getVertices() const { return _vertices; }
    [[nodiscard]] const std::vector<uint>&    getIndices() const { return _indices; }
    [[nodiscard]] uint                        getVertexCount() const { return (uint)_vertices.size(); }
    [[nodiscard]] uint                        getIndexCount() const { return (uint)_indices.size(); }

private:
    friend class AssetManager;

    explicit Mesh( std::string name, std::vector<Vertex>&& verts, std::vector<uint>&& inds, bool computeBounds = true )
        : _name( std::move( name ) )
        , _vertices( std::move( verts ) )
        , _indices( std::move( inds ) ) {
        if ( computeBounds )
            calculateBounds();
    }
    explicit Mesh( std::string name )
        : _name( std::move( name ) ) {}

    std::string          _name;
    std::vector<Vertex>  _vertices;
    std::vector<uint>    _indices;
    Math::AABB           _aabb {};
    Math::BoundingSphere _boundingSphere {};

    void calculateBounds() {
        _aabb = Math::AABB();
        for ( const auto& v : _vertices )
            _aabb.merge( v.position );

        _boundingSphere.center = _aabb.getCenter();
        _boundingSphere.radius = Math::distance( _aabb.min, _aabb.max ) * 0.5f; // Fast approx
    }
};

} // namespace Core::Assets

AXION_NAMESPACE_END