#pragma once
#include "entity_cmd.h"
#include "id.h"

namespace ECS
{
class World;

/*
    Entity Builder declaration
    These ecs operations will apply immediately
*/

class EntityMutator : public Id, public IEntityCommand
{
protected:
    EntityMutator(EntityId id, World *world) : m_world(world), Id(id) {}

    EntityMutator(LoEntityId lowId, HiEntityId highId, World *world)
        : m_world(world), Id(lowId, highId)
    {
    }

    virtual ~EntityMutator() = default;

    EntityMutator(EntityMutator &&other) = default;
    EntityMutator(const EntityMutator &other) = default;

    EntityMutator &operator=(EntityMutator &&other) = default;
    EntityMutator &operator=(const EntityMutator &other) = default;

protected:
    void AddComponentImpl(EntityId cId) override;
    void AddTagImpl(EntityId cId) override;
    void AddPairImpl(EntityId first, EntityId second) override;
    void RemoveComponentImpl(EntityId cId) override;
    void *GetImpl(EntityId cId) override;
    void SetImpl(EntityId cId, void *data) override;

protected:
    World *m_world;
};

/*
    Entity declaration
*/

class Entity : public EntityMutator
{
public:
    explicit Entity(EntityId id, World *world) : EntityMutator(id, world) {}

    virtual ~Entity() = default;

    Entity(Entity &&other) = default;
    Entity &operator=(Entity &&other) = default;

    Entity(const Entity &other) = default;
    Entity &operator=(const Entity &other) = default;
};
} // namespace ECS
