#pragma once
#include "ecs_type.h"

namespace VoidEngine::ECS
{
class World;

enum TypeAddMode
{
    ASSIGN_CONST_TYPE,
    ASSIGN_MUT_TYPE,
    ADD_TYPE
};

enum EntityCommandMode : uint8_t
{
    ENTITY_SPAWN,
    ENTITY_PATCH
};

struct AddCommand
{
    EntityId tid;
    const TypeMetadata *type_meta;
    void *data;
    TypeAddMode type;
};

struct RemoveCommand
{
    EntityId tid;
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
    EntityCommand() : eid(ENTITY_INVALID_ID), add_count(0), del_count(0)
    {
    }

    EntityCommand(EntityCommand &&other) noexcept
        : eid(other.eid), mode(other.mode), add_count(other.add_count), del_count(other.del_count)
    {
        std::memcpy(add_cmds, other.add_cmds, sizeof(AddCommand) * other.add_count);
        std::memcpy(del_cmds, other.del_cmds, sizeof(RemoveCommand) * other.del_count);
        other.eid = ENTITY_INVALID_ID;
        other.add_count = 0;
        other.del_count = 0;
    }

    EntityCommand &operator=(EntityCommand &&other) noexcept
    {
        eid = other.eid;
        mode = other.mode;
        add_count = other.add_count;
        del_count = other.del_count;
        std::memcpy(add_cmds, other.add_cmds, sizeof(AddCommand) * other.add_count);
        std::memcpy(del_cmds, other.del_cmds, sizeof(RemoveCommand) * other.del_count);
        other.eid = ENTITY_INVALID_ID;
        other.add_count = 0;
        other.del_count = 0;

        other.eid = ENTITY_INVALID_ID;
        return *this;
    }

    void add(World *world, EntityId tid);
    void assign_move(World *world, EntityId tid, void *data);
    void assign_copy(World *world, EntityId tid, const void *data);
    void remove(World *world, EntityId tid);

    void sort();

    void id(EntityId eid);
    void destroy();

    EntityId eid;
    AddCommand add_cmds[16];
    RemoveCommand del_cmds[16];
    uint8_t add_count;
    uint8_t del_count;
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
    EntityMutator(World *world) : world(world)
    {
    }

    template <typename T>
    Derived &add_component()
    {
        using C = std::remove_cvref_t<T>;
        static_assert(std::is_same_v<T, C>, "Type cannot be ref or const or both!");
        patch.add(world, TypeId<T>::id());
        return self();
    }

    template <typename T>
    Derived &add_tag()
    {
        using C = std::remove_cvref_t<T>;
        static_assert(std::is_same_v<T, C>, "Type cannot be ref or const or both!");
        patch.add(world, TypeId<T>::id());
        return self();
    }

    template <typename T>
    Derived &add_relationship(EntityId target_eid)
    {
        using C = std::remove_cvref_t<T>;
        static_assert(std::is_same_v<T, C>, "Type cannot be ref or const or both!");
        patch.add(world, make_relationship(TypeId<T>::id(), target_eid));
        return self();
    }

    template <typename T>
    Derived &assign_component(T &&data)
    {
        using C = std::remove_cvref_t<T>;
        if constexpr (std::is_same_v<T, C>)
        {
            patch.assign_move(world, TypeId<T>::id(), &data);
        }
        else
        {
            patch.assign_copy(world, TypeId<T>::id(), &data);
        }
        return self();
    }

    // Derived &AddComponent(EntityId cId)
    // {
    //     m_patch.add(m_world, cId);
    //     return self();
    // }
    //
    // Derived &AddTag(EntityId cId)
    // {
    //     m_patch.add(m_world, cId);
    //     return self();
    // }
    //
    // Derived &AddRelationship(EntityId cId, EntityId targetId)
    // {
    //     m_patch.add(m_world, make_relationship(cId, targetId));
    //     return self();
    // }
    //
    // Derived &AssignComponent(EntityId cId, void *data)
    // {
    //     m_patch.assign_move(m_world, cId, &data);
    //     return self();
    // }
    //
    // Derived &AssignComponent(EntityId cId, const void *data)
    // {
    //     m_patch.assign_move(m_world, cId, &data);
    //     return self();
    // }

private:
    Derived &self()
    {
        return *RCAST(Derived *, this);
    }

protected:
    EntityCommand patch;
    World *world;
};

// =========================================================
//
//                   ** EntityPatcher **
//
//  Bulk update entity instead of multiple single steps
//
// =========================================================

class EntityPatcher : public EntityMutator<EntityPatcher>
{
public:
    EntityPatcher(World *world, EntityId eid) : EntityMutator<EntityPatcher>(world)
    {
        patch.id(eid);
        patch.mode = EntityCommandMode::ENTITY_PATCH;
    }

    EntityPatcher(EntityPatcher &&other) = default;
    EntityPatcher &operator=(EntityPatcher &&other) = default;

    template <typename T>
    EntityPatcher &remove_type()
    {
        using C = std::remove_cvref_t<T>;
        static_assert(std::is_same_v<T, C>, "Type cannot be ref or const or both!");
        patch.remove(world, TypeId<T>::id());
        return *this;
    }

    // template <typename T>
    // EntityPatcher &RemoveRelationship(EntityId target_eid)
    // {
    //     using C = std::remove_cvref_t<T>;
    //     static_assert(std::is_same_v<T, C>, "Type cannot be ref or const or both!");
    //     patch.remove(world, make_relationship(TypeId<T>::id(), target_eid));
    //     return *this;
    // }

    void build();
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
    explicit Entity(World *world, EntityId id) : eid(id), world(world)
    {
    }

    ~Entity() = default;

    Entity(Entity &&other) = default;
    Entity &operator=(Entity &&other) = default;

    Entity(const Entity &other) = default;
    Entity &operator=(const Entity &other) = default;

    EntityId get_eid() const
    {
        return eid;
    }

    EntityPatcher patch()
    {
        return EntityPatcher(world, eid);
    }

    template <typename T>
    void set(T &&data);

    template <typename T>
    void add_component();

    template <typename T>
    void assign_component(T &&data);

    template <typename T>
    void assign_relationship(EntityId target_eid, T &&data);

    template <typename T>
    void remove();

    template <typename T>
    void add_relationship(EntityId target_eid);

    template <typename T>
    void add_tag();

private:
    EntityId eid;
    World *world;
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
    EntityBuilder(World *world);
    EntityBuilder(EntityBuilder &&other) = default;
    EntityBuilder &operator=(EntityBuilder &&other) = default;

    EntityBuilder &id(EntityId eId);
    EntityBuilder &name(const char *name);
    EntityBuilder &parent(EntityId parentId);

    Entity build();
};

} // namespace VoidEngine::ECS
