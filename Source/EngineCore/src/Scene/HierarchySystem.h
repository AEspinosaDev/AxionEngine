#pragma once
#include <Axion/Core/Scene/Scene.h>

AXION_NAMESPACE_BEGIN

namespace Core::Scene {

class HierarchySystem
{
public:
    void update( Scene& scene ) {
        auto& registry = scene.getRegistry();

        auto rootView = registry.view<TransformComponent>();
        for ( auto entity : rootView )
        {
            auto& transform = rootView.get<TransformComponent>( entity );
            if ( /* transform is dirty */ )
            {
                transform.localMatrix = Math::createTransformMatrix( transform.translation, transform.rotation, transform.scale );
                transform.worldMatrix = transform.localMatrix;
            }
        }

        for ( ECS::EntityID entity : _depthSortedEntities )
        {
            auto& transform = registry.get<TransformComponent>( entity );
            auto& family    = registry.get<FamilyComponent>( entity );

            // Recalcular matriz local si es dirty...
            // transform.localMatrix = ...

            // Componer matriz global
            if ( family.hasParent() )
            {
                const auto& parentTransform = registry.get<TransformComponent>( family.getParent() );
                transform.worldMatrix = parentTransform.worldMatrix * transform.localMatrix;
            } else
            {
                transform.worldMatrix = transform.localMatrix;
            }
        }
    }

    void rebuildDepthSortedArray( Scene& scene );

private:
    STLW::Vector<ECS::EntityID> _depthSortedEntities;
};

} // namespace Core::Scene

AXION_NAMESPACE_END