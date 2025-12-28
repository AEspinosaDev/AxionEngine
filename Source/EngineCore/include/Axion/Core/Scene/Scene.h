#pragma once
#include <Axion/Core/ECS/Entity.h>
#include <Axion/Core/ECS/Registry.h>

AXION_NAMESPACE_BEGIN

namespace Core::Scene {

class Entity; 

class Scene
{
public:
    Scene();
    ~Scene();

    Entity createEntity( const std::string& name = std::string() );
    void   destroyEntity( ECS::EntityID entity );

    void onUpdate( float dt );

    // void onViewportResize( uint32_t width, uint32_t height );

    // Backend interop
    ECS::Registry& getRegistry() { return _registry; }

private:
    ECS::Registry _registry;
    //Entities childs, and their childs

    // uint32_t _viewportWidth  = 0;
    // uint32_t _viewportHeight = 0;

    friend class Entity; 
};

} // namespace Core::Scene

AXION_NAMESPACE_END