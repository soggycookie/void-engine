
#include "ecs_type.h"
#ifdef __clang__
#pragma once
#include "entity.h"
#include "world.h"
#endif

namespace ECS
{
template <typename T>
void Entity::SetComponent(T &&data)
{
    m_world->Set<T>(m_id, std::forward<T>(data));
}

template <typename T>
void Entity::AddComponent()
{
    m_world->AddComponent<T>(m_id);
}

template <typename T>
void Entity::AssignComponent(T &&data)
{
    m_world->AssignComponent<T>(m_id, std::forward<T>(data));
}

template <typename T>
void Entity::AssignComponent(const T &data)
{
    m_world->AssignComponent<T>(m_id, std::forward<T>(data));
}

template <typename T>
void Entity::AssignRelationship(EntityId targetId, T &&data)
{
    m_world->AssignRelationship<T>(m_id, targetId, std::forward<T>(data));
}

template <typename T>
void Entity::AssignRelationship(EntityId targetId, const T &data)
{
    m_world->AssignRelationship<T>(m_id, targetId, std::forward<T>(data));
}

template <typename T>
void Entity::RemoveComponent()
{
    m_world->RemoveComponent<T>(m_id);
}

template <typename T>
void Entity::AddRelationship(EntityId targetId)
{
    m_world->AddRelationship<T>(m_id, targetId);
}

template <typename T>
void Entity::AddTag()
{
    m_world->AddTag<T>(m_id);
}
} // namespace ECS
