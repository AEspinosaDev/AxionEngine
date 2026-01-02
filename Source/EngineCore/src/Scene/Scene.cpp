
#include <Axion/Core/Scene/Entity.h>
#include <Axion/Core/Scene/Scene.h>

AXION_NAMESPACE_BEGIN

namespace Core::Scene {

Scene::Scene( const std::string& name, Assets::AssetManager* assets )
    : _name( name )
    , _assets( assets ) {
    AXION_LOG_INFO( Logger::Module::Core, "Scene [{}] Created Succesfully", _name );
}
Scene::~Scene() {
    AXION_LOG_INFO( Logger::Module::Core, "Destroying Scene [{}]", _name );
}

Entity Scene::createEntity( const std::string& name ) {

    ECS::EntityID id     = _registry.createEntity();
    Entity        entity = { id, this };

    entity.addComponent<TransformComponent>();
    auto& tag = entity.addComponent<TagComponent>();
    tag.tag   = name.empty() ? "Entity" : name;

    AXION_LOG_INFO( Logger::Module::Core, "Created Entity [{}] ID: {}", name, id );

    return entity;
}

void Scene::destroyEntity( ECS::EntityID entity ) {
    AXION_LOG_INFO( Logger::Module::Core, "Destroyed Entity ID: {}", entity );
    _registry.destroyEntity( entity );
}

void Scene::update( float dt ) {
    // PhysicsSystem::Update(_registry, dt);
    // ScriptSystem::Update(_registry, dt);
}

} // namespace Core::Scene

AXION_NAMESPACE_END