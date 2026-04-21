#pragma once
#include "ecs_type.h"

namespace ECS
{
class World;

enum class ComponentAddMode
{
    ASSIGN_CONST_TYPE,
    ASSIGN_MUT_TYPE,
    ADD_TYPE
};

enum class EntityCommandMode
{
    SPAWN,
    PATCH
};

struct AddCommand
{
    EntityId cId;
    void *data;
    ComponentAddMode type;
    TypeInfo *typeInfo;
};

struct RemoveCommand
{
    EntityId cId;
};

// =========================================================
//
//                    ** EntityCommand **
//
//  A structure stores building or updating steps
//  of an entity.
//
//  Mainly use to avoid multiple archetype move operations
//
// =========================================================

struct EntityCommand
{
    EntityCommand() = default;

    EntityCommand(EntityCommand &&other)
    {
        eId = other.eId;
        addCmds = std::move(other.addCmds);
        removeCmds = std::move(other.removeCmds);
        mode = other.mode;
    }

    EntityCommand &operator=(EntityCommand &&other)
    {
        eId = other.eId;
        addCmds = std::move(other.addCmds);
        removeCmds = std::move(other.removeCmds);
        mode = other.mode;

        return *this;
    }

    void Add(World *world, EntityId cId);

    void Assign(World *world, EntityId cId, void *data);

    void Assign(World *world, EntityId cId, const void *data);
    void Remove(World *world, EntityId cId);

    void AddCmdsSort();
    void RemoveCmdsSort();

    void Id(EntityId id);

    EntityId eId;
    Store<AddCommand> addCmds;
    Store<RemoveCommand> removeCmds;
    EntityCommandMode mode;
};

// =========================================================
//
//                    ** EntityMutator **
//
// Common interface for EntityBuilder and EntityPatcher
//
// =========================================================
template <typename Derived>
class EntityMutator
{
public:
    EntityMutator(World *world) : m_world(world) {}

    template <typename T>
    Derived &AddComponent()
    {
        static_assert(!std::is_reference_v<T> && !std::is_const_v<T>);
        m_patch.Add(m_world, ComponentTypeId<T>::Id());
        return Self();
    }

    template <typename T>
    Derived &AddTag()
    {
        static_assert(!std::is_reference_v<T> && !std::is_const_v<T>);
        m_patch.Add(m_world, ComponentTypeId<T>::Id());
        return Self();
    }

    template <typename T>
    Derived &AddRelationship(EntityId targetId)
    {
        static_assert(!std::is_reference_v<T> && !std::is_const_v<T>);
        m_patch.Add(m_world,
                    MakeRelationship(ComponentTypeId<T>::Id(), targetId));
        return Self();
    }

    template <typename T>
    Derived &AssignComponent(T &&data)
    {
        static_assert(!std::is_reference_v<T> && !std::is_const_v<T>);
        m_patch.Assign(m_world, ComponentTypeId<T>::Id(), &data);
        return Self();
    }

    template <typename T>
    Derived &AssignComponent(const T &data)
    {
        static_assert(!std::is_reference_v<T> && !std::is_const_v<T>);
        m_patch.Assign(m_world, ComponentTypeId<T>::Id(), &data);
        return Self();
    }

    Derived &AddComponent(EntityId cId)
    {
        m_patch.Add(m_world, cId);
        return Self();
    }

    Derived &AddTag(EntityId cId)
    {
        m_patch.Add(m_world, cId);
        return Self();
    }

    Derived &AddRelationship(EntityId cId, EntityId targetId)
    {
        m_patch.Add(m_world, MakeRelationship(cId, targetId));
        return Self();
    }

    Derived &AssignComponent(EntityId cId, void *data)
    {
        m_patch.Assign(m_world, cId, &data);
        return Self();
    }

    Derived &AssignComponent(EntityId cId, const void *data)
    {
        m_patch.Assign(m_world, cId, &data);
        return Self();
    }

private:
    Derived &Self() { return *PTR_CAST(this, Derived); }

protected:
    EntityCommand m_patch;
    World *m_world;
};

// =========================================================
//
//                   ** EntityPatcher **
//
//  Lazily update entity instead of multiple single steps
//
// =========================================================

class EntityPatcher : public EntityMutator<EntityPatcher>
{
public:
    EntityPatcher(World *world, EntityId eId)
        : EntityMutator<EntityPatcher>(world)
    {
        m_patch.Id(eId);
        m_patch.mode = EntityCommandMode::PATCH;
    }

    EntityPatcher(EntityPatcher &&other) = default;

    EntityPatcher &operator=(EntityPatcher &&other) = default;

    template <typename T>
    EntityPatcher &RemoveComponent()
    {
        static_assert(!std::is_reference_v<T> && !std::is_const_v<T>);
        m_patch.Remove(m_world, ComponentTypeId<T>::Id());
        return *this;
    }

    template <typename T>
    EntityPatcher &RemoveRelationship(EntityId targetId)
    {
        static_assert(!std::is_reference_v<T> && !std::is_const_v<T>);
        m_patch.Remove(m_world,
                       MakeRelationship(ComponentTypeId<T>::Id(), targetId));
        return *this;
    }

    void Flush();
};

// =========================================================
//
//                       ** Entity **
//
// A class wrap around EntityId and world pointer.
//
// Serve as a convenient way to interact with entity without
// having to operate through world directly.
//
// =========================================================

class Entity
{
public:
    explicit Entity(World *world, EntityId id) : m_id(id), m_world(world) {}

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

// =========================================================
//
//                    ** EntityBuilder **
//
// Spawn entity lazily until all the components are defined
//
// =========================================================

class EntityBuilder : public EntityMutator<EntityBuilder>
{
public:
    EntityBuilder(World *world) : EntityMutator<EntityBuilder>(world)
    {
        m_patch.mode = EntityCommandMode::SPAWN;
    }

    EntityBuilder(EntityBuilder &&other) = default;

    EntityBuilder &operator=(EntityBuilder &&other) = default;

    EntityBuilder &Id(EntityId eId);
    EntityBuilder &Name(const char *name);
    EntityBuilder &ChildOf(EntityId parentId);

    Entity Build();
};

} // namespace ECS
