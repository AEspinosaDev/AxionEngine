#pragma once
#include <Axion/Common/Common.h>
#include <Axion/Common/Math.h>
#include <Axion/Core/Assets/Handle.h>

AXION_NAMESPACE_BEGIN

namespace Core::Scene {

class EnvironmentComponent
{
public:
    enum class Type : byte
    {
        Global = 0,
        Volume = 1,
    };

    enum class SkyType : byte
    {
        Constant   = 0,
        Procedural = 1, // Passes needed {Atmosphere Compute, Atmosphere -> Screen}
        HDRi       = 2, // Passes needed {HDRI -> Cubemap, cubemap -> irradiance SH}
    };

    EnvironmentComponent()                              = default;
    EnvironmentComponent( const EnvironmentComponent& ) = default;

    Type              getType() const { return _type; }
    const Math::AABB& getAABB() const { return _aabb; }
    float             getBlendDistance() const { return _blendDistance; }
    u32               getPriority() const { return _priority; }

    SkyType                      getSkyType() const { return _skyType; }
    const Math::Vec3&            getSkyColor() const { return _skyColor; }
    const Math::Vec3&            getGroundColor() const { return _groundColor; }
    float                        getIntensity() const { return _intensity; }
    const Assets::TextureHandle& getHdriHandle() const { return _hdriHandle; }

    bool isActive() const { return _active; }

    void setType( Type type ) { _type = type; }
    void setAABB( const Math::AABB& aabb ) { _aabb = aabb; }
    void setBlendDistance( float blendDistance ) { _blendDistance = blendDistance; }
    void setPriority( u32 priority ) { _priority = priority; }

    void setSkyType( SkyType skyType ) { _skyType = skyType; }
    void setSkyColor( const Math::Vec3& skyColor ) { _skyColor = skyColor; }
    void setGroundColor( const Math::Vec3& groundColor ) { _groundColor = groundColor; }
    void setIntensity( float intensity ) { _intensity = intensity; }
    void setHdriHandle( const Assets::TextureHandle& hdriHandle ) { _hdriHandle = hdriHandle; }

    void setActive( bool active ) { _active = active; }

private:
    Type       _type = Type::Global;
    Math::AABB _aabb; // In case its volume type
    float      _blendDistance = 0.0f;
    u32        _priority      = 0;

    SkyType               _skyType     = SkyType::Constant; // For now, only constant supported
    Math::Vec3            _skyColor    = { 0.1f, 0.2f, 0.7f };
    Math::Vec3            _groundColor = { 0.3f, 0.3f, 0.3f };
    float                 _intensity   = 1.0f;
    Assets::TextureHandle _hdriHandle;

    bool _active = true;
};

} // namespace Core::Scene

AXION_NAMESPACE_END