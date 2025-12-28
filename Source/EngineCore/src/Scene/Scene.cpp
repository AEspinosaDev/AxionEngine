#include <Axion/Core/Scene/Components/TagComponent.h>
#include <Axion/Core/Scene/Components/TransformComponent.h>
#include <Axion/Core/Scene/Entity.h>
#include <Axion/Core/Scene/Scene.h>

AXION_NAMESPACE_BEGIN

namespace Core::Scene {

Scene::Scene() {}
Scene::~Scene() {}

Entity Scene::createEntity( const std::string& name ) {

    ECS::EntityID id     = _registry.createEntity();
    Entity        entity = { id, this };

    entity.addComponent<TransformComponent>();
    auto& tag = entity.addComponent<TagComponent>();
    tag.tag   = name.empty() ? "Entity" : name;

    return entity;
}

void Scene::destroyEntity( ECS::EntityID entity ) {
    _registry.destroyEntity( entity );
}

void Scene::onUpdate( float dt ) {
    // PhysicsSystem::Update(_registry, dt);
    // ScriptSystem::Update(_registry, dt);
}

// void Scene::onViewportResize( uint32_t width, uint32_t height ) {
//     _viewportWidth  = width;
//     _viewportHeight = height;

//     // Aquí iteraríamos sobre todas las cámaras para actualizar su aspect ratio
//     // auto& cameras = _registry.view<CameraComponent>();
//     // ...
// }

} // namespace Core::Scene

AXION_NAMESPACE_END