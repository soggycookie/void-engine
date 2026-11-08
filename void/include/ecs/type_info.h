#pragma once
#include "internal_component_id.h"

namespace VoidEngine::ECS
{
class World;

using CtorHook = void (*)(void *dst);
using CopyCtorHook = void (*)(void *dst, const void *src);
using MoveCtorHook = void (*)(void *dst, void *src);
using DtorHook = void (*)(void *src);

using AddEventHook = void (*)(const void *src, const World *world);
using RemoveEventHook = void (*)(const void *src, const World *world);
using SetEventHook = void (*)(const void *dst, const World *world);

struct TypeHook
{
    void (*ctor)(void *dst);
    void (*copy_ctor)(void *dst, const void *src);
    void (*move_ctor)(void *dst, void *src);
    void (*dtor)(void *src);

    void (*on_add)(const void *src, const World *world);
    void (*on_remove)(const void *src, const World *world);
    void (*on_set)(const void *dst, const World *world);

    TypeHook() : ctor(nullptr), copy_ctor(nullptr), move_ctor(nullptr), dtor(nullptr),
                on_add(nullptr), on_remove(nullptr), on_set(nullptr)
    {
    }
};

enum TypeMetaFlag : uint32_t
{
    COMPONENT_TYPE = 0x01,
    TAG_TYPE = 0x02,
    RELATION_TYPE = 0x04,
    SINGLETON_TYPE = 0x08,
    DATA_TYPE = 0x10,
    EXCLUSIVE_TYPE = 0x20,
    BIT_DATA_TYPE = 0x40,
    ALLOW_DUPLICATE = 0x80,
    TRIVIAL_DATA = 0x100,
};
using TypeMetaFlags = uint32_t;

struct TypeMetadata
{
    EntityId tid;
    uint32_t alignment;
    uint32_t size;
    TypeHook hook;
    TypeMetaFlags flags;

    TypeMetadata() : tid(ENTITY_INVALID_ID), alignment(0), size(0), flags(0), hook()
    {
    }

    bool has_data() const { return (flags & DATA_TYPE) == DATA_TYPE; }

    bool is_exclusive() const { return (flags & EXCLUSIVE_TYPE) == (EXCLUSIVE_TYPE); }

    bool is_data_a_bit() const
    {
        return (flags & (DATA_TYPE | BIT_DATA_TYPE)) == (DATA_TYPE | BIT_DATA_TYPE);
    }

    bool is_relation() const { return (flags & RELATION_TYPE) == RELATION_TYPE; }
    bool is_component() const { return (flags & COMPONENT_TYPE) == COMPONENT_TYPE; }
    bool is_tag() const { return (flags & TAG_TYPE) == TAG_TYPE; }
    bool is_singleton() const { return (flags & SINGLETON_TYPE) == SINGLETON_TYPE; }
    bool is_trivial() const { return (flags & TRIVIAL_DATA) == TRIVIAL_DATA; }
};

template <typename T>
struct TypeMetaBuilder
{
    TypeMetadata type_meta;
    World *world;
    EntityId eid;

    TypeMetaBuilder(World *world) : type_meta(), world(world), eid(0)
    {
        using C = std::remove_cvref_t<T>;
        static_assert(std::is_same_v<T, C>, "Type cannot be ref or const or both!");
        type_meta.size = sizeof(T);
        type_meta.alignment = alignof(T);
        type_meta.tid = 0;
    }

    TypeMetaBuilder<T> &ctor(CtorHook ctor);
    TypeMetaBuilder<T> &copy_ctor(CopyCtorHook cctor);
    TypeMetaBuilder<T> &move_ctor(MoveCtorHook mctor);
    TypeMetaBuilder<T> &dtor(DtorHook dtor);
    TypeMetaBuilder<T> &singleton();

    TypeMetaBuilder<T> &add_event(AddEventHook e);
    TypeMetaBuilder<T> &remove_event(RemoveEventHook e);
    TypeMetaBuilder<T> &set_event(SetEventHook e);

    TypeMetaBuilder<T> &id(EntityId eid);

    void register_type();

private:
    friend class World;

    TypeMetaBuilder<T> &exclusive();
    TypeMetaBuilder<T> &has_data();
    TypeMetaBuilder<T> &component();
    TypeMetaBuilder<T> &tag();
    TypeMetaBuilder<T> &relation();
    TypeMetaBuilder<T> &non_trivial();

    // TypeInfoBuilder<T>& Relationship(EntityId relationId, EntityId targetId);
    //
    // TypeInfoBuilder<T>& Relationship(EntityId targetId);
};
} // namespace VoidEngine::ECS
