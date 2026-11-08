#include "ecs_type.h"
#include "query.h"
#ifdef __clang__
#pragma once
#include "world.h"
#endif

namespace VoidEngine::ECS
{
template <typename T>
TypeMetaBuilder<T> World::component()
{
    // TypeInfo* ti = new (m_wAllocator.Init(sizeof(TypeInfo))) TypeInfo();
    using C = std::remove_cvref_t<T>;
    static_assert(std::is_same_v<T, C>, "Type cannot be ref or const or both!");
    static_assert(std::is_destructible_v<T>);
    static_assert(std::is_default_constructible_v<T>);
    static_assert(sizeof(T) != 1);

    TypeMetaBuilder<T> builder(this);
    builder.component();

    // if constexpr (!std::is_trivially_default_constructible_v<T>)
    // {
    builder.ctor([](void *dst) { new (dst) T(); });
    // }

    if constexpr (!std::is_trivially_copyable_v<T>)
    {
        if constexpr (std::is_move_constructible_v<T>)
        {
            builder.move_ctor([](void *dst, void *src) { new (dst) T(std::move(*SCAST(T *, src))); });
        }
        if constexpr (std::is_copy_constructible_v<T>)
        {
            builder.copy_ctor([](void *dst, const void *src) { new (dst) T(*SCAST(const T *, src)); });
        }
        static_assert(std::is_copy_constructible_v<T> || std::is_move_constructible_v<T>,
                      "Type must be trivially copyable or copy/move constructible");
        if constexpr (!std::is_trivially_destructible_v<T>)
        {
            builder.dtor(
                [](void *src)
                {
                    T *c = SCAST(T *, src);
                    c->~T();
                });
        }
    }

#ifdef ECS_DEBUG
    tiBuilder.AddEvent([](const void *src, const World *world)
                       { std::cout << "Add component " << GetComponentName<T>() << std::endl; });

    tiBuilder.RemoveEvent(
        [](const void *src, const World *world)
        { std::cout << "Remove component " << GetComponentName<T>() << std::endl; });

    tiBuilder.SetEvent([](const void *src, const World *world)
                       { std::cout << "Set component " << GetComponentName<T>() << std::endl; });
#endif // ECS_DEBUG

    return builder;
}

template <typename T>
TypeMetaBuilder<T> World::tag()
{
    // TypeInfo* ti = new (m_wAllocator.Init(sizeof(TypeInfo))) TypeInfo();
    using C = std::remove_cvref_t<T>;
    static_assert(std::is_same_v<T, C>, "Type cannot be ref or const or both!");
    static_assert(std::is_empty_v<T>);
    static_assert(std::is_trivial_v<T>);
    static_assert(std::is_trivially_default_constructible_v<T>);
    static_assert(std::is_trivially_copyable_v<T>);
    static_assert(std::is_trivially_destructible_v<T>);
    static_assert(std::is_standard_layout_v<T>);
    static_assert(sizeof(T) == 1);
    TypeMetaBuilder<T> builder(this);
    builder.tag();

#ifdef ECS_DEBUG
    tiBuilder.AddEvent([](const void *src, const World *world)
                       { std::cout << "Add tag " << GetComponentName<T>() << std::endl; });

    tiBuilder.RemoveEvent([](const void *src, const World *world)
                          { std::cout << "Remove tag " << GetComponentName<T>() << std::endl; });
#endif // ECS_DEBUG
    return builder;
}

template <typename T>
TypeMetaBuilder<T> World::relation()
{
    using C = std::remove_cvref_t<T>;
    static_assert(std::is_same_v<T, C>, "Type cannot be ref or const or both!");
    static_assert(std::is_destructible_v<T>);
    static_assert(std::is_default_constructible_v<T>);

    TypeMetaBuilder<T> builder(this);
    builder.relation();

    if constexpr (sizeof(T) > 1)
    {
        builder.has_data();

        // if constexpr (!std::is_trivially_default_constructible_v<T>)
        // {
            builder.ctor([](void *dst) { new (dst) T(); });
        // }

        if constexpr (!std::is_trivially_copyable_v<T>)
        {
            if constexpr (std::is_move_constructible_v<T>)
            {
                builder.move_ctor([](void *dst, void *src)
                                  { new (dst) T(std::move(*SCAST(T *, src))); });
            }
            if constexpr (std::is_copy_constructible_v<T>)
            {
                builder.copy_ctor([](void *dst, const void *src)
                                  { new (dst) T(*SCAST(const T *, src)); });
            }
            static_assert(std::is_copy_constructible_v<T> || std::is_move_constructible_v<T>,
                          "Type must be trivially copyable or copy/move constructible");
            if constexpr (!std::is_trivially_destructible_v<T>)
            {
                builder.dtor(
                    [](void *src)
                    {
                        T *c = SCAST(T *, src);
                        c->~T();
                    });
            }
        }
    }
#ifdef ECS_DEBUG
    tiBuilder.AddEvent([](const void *src, const World *world)
                       { std::cout << "Add relation " << GetComponentName<T>() << std::endl; });

    tiBuilder.RemoveEvent(
        [](const void *src, const World *world)
        { std::cout << "Remove relation " << GetComponentName<T>() << std::endl; });

    tiBuilder.SetEvent([](const void *src, const World *world)
                       { std::cout << "Set relation " << GetComponentName<T>() << std::endl; });
#endif // ECS_DEBUG
    return builder;
}

template <typename T>
void World::add_component(EntityId eid)
{
    using C = std::remove_cvref_t<T>;
    static_assert(std::is_same_v<T, C>, "Type cannot be ref or const or both!");
    add_component(eid, TypeId<T>::id());
}

template <typename T>
void World::assign_component(EntityId eid, T &&data)
{
    using C = std::remove_cvref_t<T>;
    if constexpr (std::is_same_v<T, C>)
    {
        assign_component_move(eid, TypeId<T>::id(), &data);
    }
    else
    {
        assign_component_copy(eid, TypeId<T>::id(), &data);
    }
}

template <typename T>
void World::assign_relationship(EntityId eid, EntityId target_eid, T &&data)
{
    using C = std::remove_cvref_t<T>;
    if constexpr (std::is_same_v<T, C>)
    {
        assign_relationship_move(eid, TypeId<C>::id(), target_eid, &data);
    }
    else
    {
        assign_relationship_copy(eid, TypeId<C>::id(), target_eid, &data);
    }
}

// template <typename T>
// void World::assign_relationship(EntityId eId, EntityId targetId, const T &data)
// {
//     using C = std::remove_cvref_t<T>;
//     static_assert(std::is_same_v<T, C>, "Type cannot be ref or const or both!");
//     assign_relationship(eId, TypeId<C>::Id(), targetId, &data);
// }

template <typename T>
void World::add_relation(EntityId eid, EntityId target_eid)
{
    using C = std::remove_cvref_t<T>;
    static_assert(std::is_same_v<T, C>, "Type cannot be ref or const or both!");
    add_relationship(eid, TypeId<C>::id(), target_eid);
}

template <typename T>
void World::add_tag(EntityId eid)
{
    using C = std::remove_cvref_t<T>;
    static_assert(std::is_same_v<T, C>, "Type cannot be ref or const or both!");
    add_tag(eid, TypeId<C>::id());
}

template <typename T>
void World::remove_type(EntityId eid)
{
    using C = std::remove_cvref_t<T>;
    static_assert(std::is_same_v<T, C>, "Type cannot be ref or const or both!");
    remove_type(eid, TypeId<C>::id());
}

template <typename T>
bool World::has_type(EntityId eid)
{
    using C = std::remove_cvref_t<T>;
    static_assert(std::is_same_v<T, C>, "Type cannot be ref or const or both!");
    return has_type(eid, TypeId<C>::id());
}

template <typename T>
void World::set(EntityId eid, T &&c)
{
    using C = std::remove_cvref_t<T>;
    if constexpr (std::is_same_v<T, C>)
    {
        set_move(eid, TypeId<C>::id(), &c);
    }
    else
    {
        set_copy(eid, TypeId<C>::id(), &c);
    }
}

template <typename T>
T &World::get_mut(EntityId eid)
{
    using C = std::remove_cvref_t<T>;
    static_assert(std::is_same_v<T, C>, "Type cannot be ref or const or both!");
    void *data = get(eid, TypeId<C>::id());
    T &component = *SCAST(C *, data);
    return component;
}

template <typename T>
const T &World::get(EntityId eid) const
{
    using C = std::remove_cvref_t<T>;
    static_assert(std::is_same_v<T, C>, "Type cannot be ref or const or both!");
    void *data = get(eid, TypeId<C>::id());
    T &component = *SCAST(C *, data);

    return component;
}

template <typename T>
T &World::get_singleton()
{
    using C = std::remove_cvref_t<T>;
    static_assert(std::is_same_v<T, C>, "Type cannot be ref or const or both!");
    // In non-relationship type, cId = eId

    return get_mut<T>(TypeId<C>::id());
}

template <typename... T>
QueryBuilder<T...> World::query_builder()
{
    return QueryBuilder<T...>(this);
}

template <typename... T>
SystemBuilder<T...> World::system_builder()
{
    return SystemBuilder<T...>(this);
}

inline uint32_t get_type_temp_id()
{
    static uint32_t s_ss_tid = 1;
    return s_ss_tid++;
}

//NOTE: force user to manually assign id to a type
template <typename T>
void World::register_type_metadata(TypeMetadata &&type_meta, const std::string_view cname)
{
    using C = std::remove_cvref_t<T>;
    static_assert(std::is_same_v<T, C>, "Type cannot be ref or const or both!");
    ENGINE_ASSERT(!is_entity_existing(type_meta.tid) && type_meta.tid != ENTITY_INVALID_ID, "Id already exists!");
    ENGINE_ASSERT(!entity_records.is_existing_skip_version(type_meta.tid) && type_meta.tid != ENTITY_INVALID_ID, "Id already exists!");
    TypeId<T>::set_id(type_meta.tid);
    //TODO: maybe change container type for type info, use another temp id for sparse set is messy here
    uint32_t ss_id = get_type_temp_id();

    uint64_t name_len = cname.size();
    if (name_len >= MaxEntityNameLength)
    {
        name_len = MaxEntityNameLength - 1;
    }
    char name[MaxEntityNameLength];
    std::memcpy(name, cname.data(), name_len);
    name[name_len] = '\0';

    type_metas.push(vallocator, ss_id, std::move(type_meta), true);
    const TypeMetadata* tm = type_metas.get(ss_id);

    type_ids.push_back(vallocator, tm->tid);
    TypeInfo type_info;
    type_info.init(vallocator, tm);
#ifdef VOID_DEBUG
    uint32_t len = std::strlen(name) < 32 ? strlen(name) : 31;
    std::memcpy(type_info.name, name, len);
    type_info.name[len] = '\0';
#endif
    type_infos.insert(vallocator, tm->tid, std::move(type_info));

    auto builder = entity_builder();
    if (type_meta.tid != ENTITY_INVALID_ID)
    {
        builder.id(type_meta.tid);
    }
    builder.name(name);
    if (tm->is_singleton())
    {
        builder.add_component<T>();
    }
    Entity e = builder.build();
}

} // namespace VoidEngine::ECS
