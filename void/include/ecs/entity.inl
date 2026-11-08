#ifdef __clang__
#pragma once
#include "entity.h"
#include "world.h"
#endif

namespace VoidEngine::ECS
{

// =========================================================
//
//                       ** Entity **
//
// =========================================================

template <typename T>
void Entity::set(T &&data)
{
    world->set<T>(eid, std::forward<T>(data));
}

template <typename T>
void Entity::add_component()
{
    world->add_component<T>(eid);
}

template <typename T>
void Entity::assign_component(T &&data)
{
    world->assign_component<T>(eid, std::forward<T>(data));
}

template <typename T>
void Entity::assign_relationship(EntityId target_eid, T &&data)
{
    world->assign_relationship<T>(eid, target_eid, std::forward<T>(data));
}

template <typename T>
void Entity::remove()
{
    world->remove_type<T>(eid);
}

template <typename T>
void Entity::add_relationship(EntityId target_eid)
{
    world->add_relation<T>(eid, target_eid);
}

template <typename T>
void Entity::add_tag()
{
    world->add_tag<T>(eid);
}
} // namespace VoidEngine::ECS
