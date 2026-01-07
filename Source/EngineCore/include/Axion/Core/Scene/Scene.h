#pragma once
#include <Axion/Core/Assets/AssetManager.h>
#include <Axion/Core/ECS/Entity.h>
#include <Axion/Core/ECS/Registry.h>

#include <Axion/Core/Scene/Components/LightComponent.h>
#include <Axion/Core/Scene/Components/MeshComponent.h>
#include <Axion/Core/Scene/Components/TagComponent.h>
#include <Axion/Core/Scene/Components/TransformComponent.h>
#include <Axion/Core/Scene/Components/CameraComponent.h>

AXION_NAMESPACE_BEGIN

namespace Core::Scene {

class Entity;

class Scene
{
public:
    Scene( const std::string& name, Assets::AssetManager* assets );
    Scene( const Scene& ) = delete;
    ~Scene();

    Entity createEntity( const std::string& name = std::string() );
    void   destroyEntity( ECS::EntityID entity );

    void                        update( float dt );
    const Assets::AssetManager* assets() const { return _assets; }
    const ECS::Registry&        getRegistry() const { return _registry; }

    std::string getName() const { return _name; }
    void        setName( const std::string& name ) { _name = name; }

private:
    // Backend interop
    ECS::Registry& registry() { return _registry; }

    std::string           _name;
    ECS::Registry         _registry;
    Assets::AssetManager* _assets;

    friend class Entity;
};

} // namespace Core::Scene

AXION_NAMESPACE_END