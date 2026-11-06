#pragma once
#include <Axion/Common/Math.h>
#include <Axion/Core/Assets/Handle.h>
#include <algorithm>

AXION_NAMESPACE_BEGIN

namespace Core::Scene {

/**
 * @brief Many copies of a mesh simulated on the GPU, placed relative to the transform of the entity.
 * It holds its own mesh and material: it doesn't need a MeshComponent.
 */
class ParticleSystemComponent
{
public:
    /// @brief Simulation the GPU runs on the particles.
    enum class Behavior : u32
    {
        Orbitals = 0 ///< Particles orbit around the origin of the entity.
        //TBD...
    };

    ParticleSystemComponent()                                 = default;
    ParticleSystemComponent( const ParticleSystemComponent& ) = default;

    ParticleSystemComponent( const Assets::MeshHandle&     meshHandle,
                             const Assets::MaterialHandle& materialHandle,
                             u32                           count )
        : _mesh( meshHandle )
        , _material( materialHandle )
        , _count( count ) {
    }

    const Assets::MeshHandle&     getMesh() const { return _mesh; }
    const Assets::MaterialHandle& getMaterial() const { return _material; }

    u32               getCount() const { return _count; }
    u32               getAtomicity() const { return _atomicity; }
    u32               getSeed() const { return _seed; }
    Behavior          getBehavior() const { return _behavior; }
    float             getSize() const { return _size; }
    float             getSizeVariation() const { return _sizeVariation; }
    float             getSpeed() const { return _speed; }
    const Math::Vec3& getExtent() const { return _extent; }
    bool              isVisible() const { return _visible; }
    bool              isUpdating() const { return _update; }
    bool              facesCamera() const { return _faceCamera; }

    /// @brief Changes with the settings only read when the particles are seeded: the GPU has to seed them again.
    u32 getSeedingVersion() const { return _seedingVersion; }

    /// @brief Particles with their own state in the GPU buffer: the rest share it, in groups of atomicity.
    u32 getSimulatedCount() const { return ( _count + _atomicity - 1 ) / _atomicity; }

    void setMesh( const Assets::MeshHandle& meshHandle ) { _mesh = meshHandle; }
    void setMaterial( const Assets::MaterialHandle& materialHandle ) { _material = materialHandle; }
    void setCount( u32 count ) { _count = count; }
    void setAtomicity( u32 atomicity ) { _atomicity = std::max( atomicity, 1u ); }
    void setSeed( u32 seed ) {
        if ( _seed != seed )
            _seedingVersion++;
        _seed = seed;
    }
    void setBehavior( Behavior behavior ) {
        if ( _behavior != behavior )
            _seedingVersion++;
        _behavior = behavior;
    }
    void setSize( float size ) { _size = size; }
    void setSizeVariation( float sizeVariation ) {
        sizeVariation = std::max( sizeVariation, 1.0f );
        if ( _sizeVariation != sizeVariation )
            _seedingVersion++;
        _sizeVariation = sizeVariation;
    }
    void setSpeed( float speed ) { _speed = speed; }
    void setExtent( const Math::Vec3& extent ) {
        if ( _extent != extent )
            _seedingVersion++;
        _extent = extent;
    }
    void setVisible( bool visible ) { _visible = visible; }
    void setUpdating( bool update ) { _update = update; }
    void setFaceCamera( bool faceCamera ) { _faceCamera = faceCamera; }

private:
    Assets::MeshHandle     _mesh;
    Assets::MaterialHandle _material;

    u32        _count     = 0;
    u32        _atomicity = 1;                 ///< Particles sharing one simulated state. 1 = each one has its own.
    u32        _seed      = 0;                 ///< Seed of the initial state.
    Behavior   _behavior  = Behavior::Orbitals;
    float      _size      = 1.0f;              ///< Uniform scale of the mesh.
    float      _speed     = 1.0f;              ///< Base speed multiplier.
    Math::Vec3 _extent    = { 1.0f, 1.0f, 1.0f }; ///< Volume the particles live in, in entity space.

    float _sizeVariation = 1.0f; ///< Each particle is seeded between size / variation and size * variation. 1 = all the same.

    u32 _seedingVersion = 0; ///< See getSeedingVersion.

    bool _visible    = true;  ///< Drawn this frame.
    bool _update     = true;  ///< Simulated this frame.
    bool _faceCamera = false; ///< Billboard.
};

} // namespace Core::Scene

AXION_NAMESPACE_END
