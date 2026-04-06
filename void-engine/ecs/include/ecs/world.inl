#include "ecs_type.h"
#include "query.h"
#include <type_traits>
#ifdef __clang__
#pragma once
#include "world.h"
#endif

namespace ECS
{
template <typename T>
TypeInfoBuilder<T> World::Component()
{
    // TypeInfo* ti = new (m_wAllocator.Init(sizeof(TypeInfo))) TypeInfo();
    static_assert(!std::is_reference_v<T> && !std::is_const_v<T>);
    static_assert(std::is_destructible_v<T>);
    static_assert(std::is_default_constructible_v<T>);
    static_assert(sizeof(T) != 1);

    TypeInfoBuilder<T> tiBuilder(this);
    tiBuilder.Component();

    tiBuilder.Ctor([](void *dest) { new (dest) T(); });

    if constexpr (std::is_move_constructible_v<T>)
    {
        tiBuilder.MoveCtor([](void *dest, void *src)
                           { new (dest) T(std::move(*PTR_CAST(src, T))); });
    }
    else if constexpr (!std::is_trivially_constructible_v<T>)
    {
        tiBuilder.CopyCtor([](void *dest, const void *src)
                           { new (dest) T(*PTR_CAST(src, T)); });
    }

    if constexpr (!std::is_trivially_destructible_v<T>)
    {
        tiBuilder.Dtor(
            [](void *src)
            {
                T *c = PTR_CAST(src, T);
                c->~T();
            });
    }

#ifdef ECS_DEBUG
    tiBuilder.AddEvent(
        [](const void *src, const World *world)
        {
            std::cout << "Add component " << GetComponentName<T>() << std::endl;
        });

    tiBuilder.RemoveEvent(
        [](const void *src, const World *world)
        {
            std::cout << "Remove component " << GetComponentName<T>()
                      << std::endl;
        });

    tiBuilder.SetEvent(
        [](const void *src, const World *world)
        {
            std::cout << "Set component " << GetComponentName<T>() << std::endl;
        });
#endif // ECS_DEBUG

    return tiBuilder;
}

template <typename T>
TypeInfoBuilder<T> World::Tag()
{
    // TypeInfo* ti = new (m_wAllocator.Init(sizeof(TypeInfo))) TypeInfo();

    static_assert(!std::is_reference_v<T> && !std::is_const_v<T>);
    static_assert(std::is_destructible_v<T>);
    static_assert(std::is_trivially_constructible_v<T>);

    TypeInfoBuilder<T> tiBuilder(this);
    tiBuilder.Tag();

    assert(tiBuilder.ti.size == 1 && "Tag can not have data");

#ifdef ECS_DEBUG
    tiBuilder.AddEvent(
        [](const void *src, const World *world)
        { std::cout << "Add tag " << GetComponentName<T>() << std::endl; });

    tiBuilder.RemoveEvent(
        [](const void *src, const World *world)
        { std::cout << "Remove tag " << GetComponentName<T>() << std::endl; });
#endif // ECS_DEBUG
    return tiBuilder;
}

template <typename T>
TypeInfoBuilder<T> World::Relation()
{
    static_assert(!std::is_reference_v<T> && !std::is_const_v<T>);
    static_assert(std::is_destructible_v<T>);
    static_assert(std::is_default_constructible_v<T>);

    TypeInfoBuilder<T> tiBuilder(this);
    tiBuilder.Relation();

    if (tiBuilder.ti.size > 1)
    {
        tiBuilder.HasData();

        tiBuilder.Ctor([](void *dest) { new (dest) T(); });

        if constexpr (std::is_move_constructible_v<T>)
        {
            tiBuilder.MoveCtor([](void *dest, void *src)
                               { new (dest) T(std::move(*PTR_CAST(src, T))); });
        }
        else if constexpr (!std::is_trivially_constructible_v<T>)
        {
            tiBuilder.CopyCtor([](void *dest, const void *src)
                               { new (dest) T(*PTR_CAST(src, T)); });
        }

        if constexpr (!std::is_trivially_destructible_v<T>)
        {
            tiBuilder.Dtor(
                [](void *src)
                {
                    T *c = PTR_CAST(src, T);
                    c->~T();
                });
        }
    }
#ifdef ECS_DEBUG
    tiBuilder.AddEvent(
        [](const void *src, const World *world)
        {
            std::cout << "Add relation " << GetComponentName<T>() << std::endl;
        });

    tiBuilder.RemoveEvent(
        [](const void *src, const World *world)
        {
            std::cout << "Remove relation " << GetComponentName<T>()
                      << std::endl;
        });

    tiBuilder.SetEvent(
        [](const void *src, const World *world)
        {
            std::cout << "Set relation " << GetComponentName<T>() << std::endl;
        });
#endif // ECS_DEBUG
    return tiBuilder;
}

template <typename T>
void World::AddComponent(EntityId eId)
{
    static_assert(!std::is_reference_v<T> && !std::is_const_v<T>);
    AddComponent(eId, ComponentTypeId<T>::Id());
}

template <typename T>
void World::AssignComponent(EntityId eId, T &&data)
{
    static_assert(!std::is_reference_v<T> && !std::is_const_v<T>);
    AssignComponent(eId, ComponentTypeId<T>::Id(), &data);
}

template <typename T>
void World::AssignComponent(EntityId eId, const T &data)
{
    static_assert(!std::is_reference_v<T> && !std::is_const_v<T>);
    AssignComponent(eId, ComponentTypeId<T>::Id(), &data);
}

template <typename T>
void World::AssignRelationship(EntityId eId, EntityId targetId, T &&data)
{
    static_assert(!std::is_reference_v<T> && !std::is_const_v<T>);
    AssignRelationship(eId, ComponentTypeId<T>::Id(), targetId, &data);
}

template <typename T>
void World::AssignRelationship(EntityId eId, EntityId targetId, const T &data)
{
    static_assert(!std::is_reference_v<T> && !std::is_const_v<T>);
    AssignRelationship(eId, ComponentTypeId<T>::Id(), targetId, &data);
}

template <typename T>
void World::AddRelationship(EntityId eId, EntityId targetId)
{
    static_assert(!std::is_reference_v<T> && !std::is_const_v<T>);
    AddRelationship(eId, ComponentTypeId<T>::Id(), targetId);
}

template <typename T>
void World::AddTag(EntityId eId)
{
    static_assert(!std::is_reference_v<T> && !std::is_const_v<T>);
    AddTag(eId, ComponentTypeId<T>::Id());
}

template <typename T>
void World::RemoveComponent(EntityId eId)
{
    static_assert(!std::is_reference_v<T> && !std::is_const_v<T>);
    RemoveComponent(eId, ComponentTypeId<T>::Id());
}

template <typename T>
bool World::HasComponent(EntityId eId)
{
    static_assert(!std::is_reference_v<T> && !std::is_const_v<T>);
    return HasComponent(eId, ComponentTypeId<T>::Id());
}

template <typename T>
void World::Set(EntityId eId, T &&c)
{
    static_assert(!std::is_reference_v<T> && !std::is_const_v<T>);
    Set(eId, ComponentTypeId<std::decay_t<T>>::Id(), &c);
}

template <typename T>
T &World::Get(EntityId eId)
{
    static_assert(!std::is_reference_v<T>);
    void *data = Get(eId, ComponentTypeId<std::decay_t<T>>::Id());

    T &component = *PTR_CAST(data, std::decay_t<T>);

    return component;
}

template <typename T>
T &World::GetSingleton()
{
    static_assert(!std::is_reference_v<T>);
    // In non-relationship type, cId = eId

    return Get<T>(ComponentTypeId<std::decay_t<T>>::Id());
}

template <typename... T>
QueryBuilder<T...> World::CreateQuery()
{
    return QueryBuilder<T...>(this);
}

template <typename... T>
SystemBuilder<T...> World::CreateSystem()
{
    return SystemBuilder<T...>(this);
}

} // namespace ECS
