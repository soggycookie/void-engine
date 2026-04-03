#pragma once
#include "ecs_type.h"
#include "id.h"

namespace ECS
{
class World;

enum class AddCmdTypeData
{
    ASSIGN_CONST_TYPE,
    ASSIGN_MUT_TYPE,
    ADD_TYPE
};

struct AddCommand
{
    EntityId cId;
    void *data;
    AddCmdTypeData type;
    TypeInfo *typeInfo;
};

struct RemoveCommand
{
    EntityId cId;
};

enum class CmdMode
{
    ADD_CMD_MODE,
    REMOVE_CMD_MODE,
};

struct EntityDeferredCommand
{
    EntityId id;
    AddCommand addCmd;
    RemoveCommand removeCmds;
    CmdMode mode;
    TypeInfo *typeInfo;
};

struct EntityDesc
{
    EntityDesc() : eId(0), parentId(0), name(nullptr), descComponents() {}

    EntityDesc(EntityId eId, EntityId parentId, const char *name)
        : eId(eId), parentId(parentId), name(name), descComponents()
    {
    }

    void Add(World *world, EntityId cId);

    void Assign(World *world, EntityId cId, void *data);

    void Assign(World *world, EntityId cId, const void *data);

    void Sort();

    EntityId eId;
    EntityId parentId;
    const char *name;
    Store<AddCommand> descComponents;
};

template <typename Derived>
class EntityCommand
{
public:
    template <typename T>
    Derived &AddComponent();

    template <typename T>
    Derived &AddTag();

    template <typename T>
    Derived &AddRelationship(EntityId targetId);

    template <typename T>
    Derived &AssignComponent(T &&data);

    template <typename T>
    Derived &AssignComponent(const T &data);

private:
    Derived &Self() { return *PTR_CAST(this, Derived); }
};

struct EntityPatch
{
    EntityPatch(EntityId eId) : eId(eId) {}

    void Add(World *world, EntityId cId);

    void Assign(World *world, EntityId cId, void *data);

    void Assign(World *world, EntityId cId, const void *data);

    void Remove(World *world, EntityId cId);

    void AddCmdsSort();
    void RemoveCmdsSort();

    EntityId eId;
    Store<AddCommand> addCmds;
    Store<RemoveCommand> removeCmds;
};

///////////////////////////////////////////////////////////////////////
////////////// DO NOT STORE ENTITY PATCHER AS VARIABLE ////////////////
///////////////////////////////////////////////////////////////////////

class EntityPatcher : public EntityCommand<EntityPatcher>
{
public:
    EntityPatcher(World *world, EntityId eId) : m_world(world), m_patch(eId) {}

    EntityPatcher(EntityPatcher &&other) = default;

    EntityPatcher &operator=(EntityPatcher &&other) = default;

    template <typename T>
    EntityPatcher &RemoveComponent()
    {
        static_assert(!std::is_reference_v<T> && !std::is_const_v<T>);
        return RemoveComponentImpl(ComponentTypeId<T>::Id());
    }

    template <typename T>
    EntityPatcher &RemoveRelationship(EntityId targetId)
    {
        static_assert(!std::is_reference_v<T> && !std::is_const_v<T>);
        return RemoveComponentImpl(
            MakeRelationship(ComponentTypeId<T>::Id(), targetId));
    }

    void Flush();

private:
    template <typename Derived>
    friend class EntityCommand;

    EntityPatcher &AddComponentImpl(EntityId cId);
    EntityPatcher &AddTagImpl(EntityId cId);
    EntityPatcher &AddRelationshipImpl(EntityId relationId, EntityId targetId);
    EntityPatcher &AssignComponentImpl(EntityId cId, void *data);
    EntityPatcher &AssignComponentImpl(EntityId cId, const void *data);
    EntityPatcher &RemoveComponentImpl(EntityId cId);

private:
    EntityPatch m_patch;
    World *m_world;
};

class Entity
{
public:
    explicit Entity(EntityId id, World *world) : m_id(id), m_world(world) {}

    virtual ~Entity() = default;

    Entity(Entity &&other) = default;
    Entity &operator=(Entity &&other) = default;

    Entity(const Entity &other) = default;
    Entity &operator=(const Entity &other) = default;

    EntityId GetFullId() const { return m_id; }

    LoEntityId GetLowId() const { return LO_ENTITY_ID(m_id); }

    HiEntityId GetHighId() const { return HI_ENTITY_ID(m_id); }

    GenCount GetGenCount() const { return ENTITY_GEN_COUNT(m_id); }

    // void IncreGenCount()
    //{
    //     m_id = INCRE_GEN_COUNT(m_id);
    // }

    EntityPatcher Patch() { return EntityPatcher(m_world, m_id); }

    template <typename T>
    void SetComponent(T &&data);

    template <typename T>
    void AddComponent();

    template <typename T>
    void AssignComponent(T &&data);

    template <typename T>
    void AssignComponent(const T &data);

    template <typename T>
    void AssignRelationship(EntityId targetId, T &&data);

    template <typename T>
    void AssignRelationship(EntityId targetId, const T &data);

    template <typename T>
    void RemoveComponent();

    template <typename T>
    void AddRelationship(EntityId targetId);

    template <typename T>
    void AddTag();

private:
    EntityId m_id;
    World *m_world;
};

/*
    Entity Builder declaration
    These ecs operations will apply immediately
*/

class EntityBuilder : public EntityCommand<EntityBuilder>
{
public:
    EntityBuilder(World *world) : m_world(world) {}

    EntityBuilder(EntityBuilder &&other) = default;

    EntityBuilder &operator=(EntityBuilder &&other) = default;

    EntityBuilder &Id(EntityId eId);
    EntityBuilder &Name(const char *name);
    EntityBuilder &ChildOf(EntityId parentId);

    Entity Build();

private:
    template <typename Derived>
    friend class EntityCommand;

    EntityBuilder &AddComponentImpl(EntityId cId);
    EntityBuilder &AddTagImpl(EntityId cId);
    EntityBuilder &AddRelationshipImpl(EntityId relationId, EntityId targetId);
    EntityBuilder &AssignComponentImpl(EntityId cId, void *data);
    EntityBuilder &AssignComponentImpl(EntityId cId, const void *data);

private:
    EntityDesc m_desc;
    World *m_world;
};

} // namespace ECS
