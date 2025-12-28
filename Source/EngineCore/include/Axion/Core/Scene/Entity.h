#pragma once
#include <Axion/Core/Scene/Scene.h>

AXION_NAMESPACE_BEGIN

namespace Core::Scene {

class Entity
{
public:
    Entity() = default;
    Entity( ECS::EntityID handle, Scene* scene )
        : _entityHandle( handle )
        , _scene( scene ) {}

    template <typename T, typename... Args>
    T& addComponent( Args&&... args ) {
        return _scene->getRegistry().addComponent<T>( _entityHandle, std::forward<Args>( args )... );
    }

    template <typename T>
    T& getComponent() {
        return _scene->getRegistry().getComponent<T>( _entityHandle );
    }

    template <typename T>
    void removeComponent() {
        _scene->getRegistry().removeComponent<T>( _entityHandle );
    }

    template <typename T>
    bool hasComponent() {
        return _scene->getRegistry().hasComponent<T>( _entityHandle );
    }

    operator bool() const { return _entityHandle != ECS::NULL_ENTITY; }
    operator ECS::EntityID() const { return _entityHandle; }

private:
    ECS::EntityID _entityHandle = ECS::NULL_ENTITY;
    Scene*        _scene        = nullptr;
};

} // namespace Core::Scene

AXION_NAMESPACE_END