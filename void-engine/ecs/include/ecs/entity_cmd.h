#pragma once
#include "ds/world_allocator.h"
#include "ecs_type.h"

namespace ECS
{
class World;

class IEntityCommand
{
public:
    template <typename Component>
    IEntityCommand &AddComponent();

    template <typename Component>
    IEntityCommand &AddTag();

    template <typename First>
    IEntityCommand &AddRelationship(EntityId second);

    template <typename Component>
    IEntityCommand &RemoveComponent();

    /*template<typename FirstComponent, typename... Components>
    EntityMutator& AddComponents(const FirstComponent& f, const Components&...
    c);

    template<typename FirstComponent, typename... Components>
    EntityMutator& RemoveComponents();*/

    template <typename Component>
    IEntityCommand &Set(Component &&c);

    template <typename Component>
    Component &Get();

protected:
    virtual void AddComponentImpl(EntityId id) = 0;
    virtual void AddTagImpl(EntityId id) = 0;
    virtual void AddPairImpl(EntityId first, EntityId second) = 0;
    virtual void RemoveComponentImpl(EntityId id) = 0;
    virtual void *GetImpl(EntityId id) = 0;
    virtual void SetImpl(EntityId id, void *data) = 0;
};


} // namespace ECS
