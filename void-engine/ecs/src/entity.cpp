#include "entity.h"
#include "world.h"
#include <algorithm>

namespace ECS
{
//////////////////////////////// Entity Desc //////////////////////////////////

void EntityDesc::Add(WorldAllocator &wAllocator, EntityId cId, void *data)
{
    bulkComponents.Add(wAllocator, DescEntry{cId, data});
}

void EntityDesc::Sort()
{
    std::sort(bulkComponents.store, bulkComponents.store + bulkComponents.count,
              [](const DescEntry &a, const DescEntry &b)
              { return a.cId < b.cId; });
}
/////////////////////////////// Entity Builder ////////////////////////////////

void EntityBuilder::AddComponentImpl(EntityId cId)
{
    m_world->AddComponent(m_id, cId);
}

void EntityBuilder::AddTagImpl(EntityId cId) { m_world->AddTag(m_id, cId); }

void EntityBuilder::AddPairImpl(EntityId first, EntityId second)
{
    m_world->AddRelationship(m_id, first, second);
}

void EntityBuilder::RemoveComponentImpl(EntityId cId)
{
    m_world->RemoveComponent(m_id, cId);
}

void *EntityBuilder::GetImpl(EntityId cId) { return m_world->Get(m_id, cId); }

void EntityBuilder::SetImpl(EntityId cId, void *data)
{
    m_world->Set(m_id, cId, data);
}

} // namespace ECS
