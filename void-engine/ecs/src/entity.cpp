#include "entity.h"
#include "world.h"

namespace ECS
{
    void EntityMutator::AddComponentImpl(EntityId cId)
    {
        m_world->AddComponent(m_id, cId);
    }

    void EntityMutator::AddTagImpl(EntityId cId)
    {
        m_world->AddTag(m_id, cId);
    }

    void EntityMutator::AddPairImpl(EntityId first, EntityId second)
    {
        m_world->AddRelationship(m_id, first, second);
    }

    void EntityMutator::RemoveComponentImpl(EntityId cId)
    {
        m_world->RemoveComponent(m_id, cId);
    }

    void* EntityMutator::GetImpl(EntityId cId)
    {
        return m_world->Get(m_id, cId);
    }

    void EntityMutator::SetImpl(EntityId cId, void* data)
    {
        m_world->Set(m_id, cId, data);
    }

}