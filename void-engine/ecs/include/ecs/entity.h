#pragma once
#include "ecs_type.h"
#include "entity_cmd.h"
#include "id.h"
#include <type_traits>

namespace ECS
{
class World;

struct EntityDesc
{
    struct DescEntry
    {
        EntityId cId;
        void *data;
    };

    EntityDesc() : eId(0), parentId(0), name(nullptr), bulkComponents() {}

    EntityDesc(EntityId eId, EntityId parentId, const char *name)
        : eId(eId), parentId(parentId), name(name), bulkComponents()
    {
    }

    template <typename T>
    void Add(WorldAllocator &wAllocator, void *data)
    {
        static_assert(!std::is_reference_v<T> && !std::is_const_v<T>);
        Add(wAllocator, ComponentTypeId<T>::Id(), data);
    }

    void Add(WorldAllocator &wAllocator, EntityId cId, void *data);

    void Sort();

    EntityId eId;
    EntityId parentId;
    const char *name;
    Store<DescEntry> bulkComponents;
};

/*
    Entity Builder declaration
    These ecs operations will apply immediately
*/

class EntityBuilder : public Id, public IEntityCommand
{
protected:
    EntityBuilder(EntityId id, World *world) : m_world(world), Id(id) {}

    EntityBuilder(LoEntityId lowId, HiEntityId highId, World *world)
        : m_world(world), Id(lowId, highId)
    {
    }

    virtual ~EntityBuilder() = default;

    EntityBuilder(EntityBuilder &&other) = default;
    EntityBuilder(const EntityBuilder &other) = default;

    EntityBuilder &operator=(EntityBuilder &&other) = default;
    EntityBuilder &operator=(const EntityBuilder &other) = default;

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

class Entity : public EntityBuilder
{
public:
    explicit Entity(EntityId id, World *world) : EntityBuilder(id, world) {}

    virtual ~Entity() = default;

    Entity(Entity &&other) = default;
    Entity &operator=(Entity &&other) = default;

    Entity(const Entity &other) = default;
    Entity &operator=(const Entity &other) = default;
};
} // namespace ECS
