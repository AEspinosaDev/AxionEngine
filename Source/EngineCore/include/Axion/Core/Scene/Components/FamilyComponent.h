#pragma once
#include <Axion/Common/Common.h>
#include <Axion/Core/ECS/Entity.h>

AXION_NAMESPACE_BEGIN

namespace Core::Scene {

class FamilyComponent
{
public:
    FamilyComponent()                         = default;
    FamilyComponent( const FamilyComponent& ) = default;

    inline ECS::EntityID getParent() const { return parent; }
    inline ECS::EntityID getFirstChild() const { return firstChild; }
    inline ECS::EntityID getNextSibling() const { return nextSibling; }
    inline ECS::EntityID getPrevSibling() const { return prevSibling; }

    inline void setParent( ECS::EntityID id ) { parent = id; }
    inline void setFirstChild( ECS::EntityID id ) { firstChild = id; }
    inline void setNextSibling( ECS::EntityID id ) { nextSibling = id; }
    inline void setPrevSibling( ECS::EntityID id ) { prevSibling = id; }

    inline bool hasParent() const { return parent != ECS::NULL_ENTITY; }
    inline bool hasChildren() const { return firstChild != ECS::NULL_ENTITY; }

private:
    ECS::EntityID parent      = ECS::NULL_ENTITY;
    ECS::EntityID firstChild  = ECS::NULL_ENTITY;
    ECS::EntityID nextSibling = ECS::NULL_ENTITY;
    ECS::EntityID prevSibling = ECS::NULL_ENTITY;
};

} // namespace Core::Scene

AXION_NAMESPACE_END