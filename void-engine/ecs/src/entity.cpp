#include "entity.h"
#include "ecs_type.h"
#include "world.h"
#include <algorithm>

namespace ECS
{
//////////////////////////////// Entity Desc //////////////////////////////////

void EntityDesc::Assign(World *world, EntityId cId, void *data)
{
    // USE LOW ENTITY ID BECAUSE RELATIONSHIP CRECORD MIGHT NOT BE AVAILABLE AT
    // THAT POINT YET

    assert(world);
    ComponentRecord &cr = world->m_componentRecordIndex[LO_ENTITY_ID(cId)];
    TypeInfo *ti = cr.typeInfo;

    void *temp = world->m_wAllocator.Alloc(ti->size);

    if (ti->hook.moveCtor)
    {
        ti->hook.moveCtor(temp, data);
    }
    else if (ti->hook.copyCtor)
    {
        ti->hook.copyCtor(temp, data);
    }
    else
    {
        std::memcpy(temp, data, ti->size);
    }

    AddCommand cmd;
    cmd.cId = cId;
    cmd.data = temp;
    cmd.type = AddCmdTypeData::ASSIGN_MUT_TYPE;
    cmd.typeInfo = ti;

    descComponents.Add(world->m_wAllocator, std::move(cmd));
}

void EntityDesc::Assign(World *world, EntityId cId, const void *data)
{
    // USE LOW ENTITY ID BECAUSE RELATIONSHIP CRECORD MIGHT NOT BE AVAILABLE AT
    // THAT POINT YET
    assert(world);
    ComponentRecord &cr = world->m_componentRecordIndex[LO_ENTITY_ID(cId)];
    TypeInfo *ti = cr.typeInfo;

    void *temp = world->m_wAllocator.Alloc(ti->size);

    if (ti->hook.copyCtor)
    {
        ti->hook.copyCtor(temp, data);
    }
    else
    {
        std::memcpy(temp, data, ti->size);
    }

    AddCommand cmd;
    cmd.cId = cId;
    cmd.data = temp;
    cmd.type = AddCmdTypeData::ASSIGN_CONST_TYPE;
    cmd.typeInfo = ti;

    descComponents.Add(world->m_wAllocator, std::move(cmd));
}

void EntityDesc::Add(World *world, EntityId cId)
{
    // USE LOW ENTITY ID BECAUSE RELATIONSHIP CRECORD MIGHT NOT BE AVAILABLE AT
    // THAT POINT YET

    assert(world);
    ComponentRecord &cr = world->m_componentRecordIndex[LO_ENTITY_ID(cId)];
    TypeInfo *ti = cr.typeInfo;

    AddCommand cmd;
    cmd.cId = cId;
    cmd.data = nullptr;
    cmd.type = AddCmdTypeData::ADD_TYPE;
    cmd.typeInfo = ti;

    descComponents.Add(world->m_wAllocator, std::move(cmd));
}

void EntityDesc::Sort()
{
    std::sort(descComponents.store, descComponents.store + descComponents.count,
              [](const AddCommand &a, const AddCommand &b)
              { return a.cId < b.cId; });
}
/////////////////////////////// Entity Builder ////////////////////////////////

EntityBuilder &EntityBuilder::Id(EntityId eId)
{
    m_desc.eId = eId;
    return *this;
}
EntityBuilder &EntityBuilder::Name(const char *name)
{
    m_desc.name = name;
    return *this;
}
EntityBuilder &EntityBuilder::ChildOf(EntityId parentId)
{
    m_desc.parentId = parentId;
    return *this;
}

Entity EntityBuilder::Build() { return m_world->ResolveEntityDesc(m_desc); }

EntityBuilder &EntityBuilder::AddComponentImpl(EntityId cId)
{
    m_desc.Add(m_world, cId);
    return *this;
}
EntityBuilder &EntityBuilder::AddTagImpl(EntityId cId)
{
    m_desc.Add(m_world, cId);
    return *this;
}
EntityBuilder &EntityBuilder::AddRelationshipImpl(EntityId relationId,
                                                  EntityId targetId)
{
    m_desc.Add(m_world, MakeRelationship(relationId, targetId));
    return *this;
}
EntityBuilder &EntityBuilder::AssignComponentImpl(EntityId cId, void *data)
{
    m_desc.Assign(m_world, cId, data);
    return *this;
}
EntityBuilder &EntityBuilder::AssignComponentImpl(EntityId cId,
                                                  const void *data)
{
    m_desc.Assign(m_world, cId, data);
    return *this;
}

///////////////////////////////// EntityPatch ////////////////////////////////

void EntityPatch::Add(World *world, EntityId cId)
{
    // USE LOW ENTITY ID BECAUSE RELATIONSHIP CRECORD MIGHT NOT BE AVAILABLE AT
    // THAT POINT YET
    assert(world);
    ComponentRecord &cr = world->m_componentRecordIndex[LO_ENTITY_ID(cId)];
    TypeInfo *ti = cr.typeInfo;

    AddCommand cmd;
    cmd.cId = cId;
    cmd.data = nullptr;
    cmd.type = AddCmdTypeData::ADD_TYPE;
    cmd.typeInfo = ti;

    addCmds.Add(world->m_wAllocator, std::move(cmd));
}

void EntityPatch::Assign(World *world, EntityId cId, void *data)
{
    // USE LOW ENTITY ID BECAUSE RELATIONSHIP CRECORD MIGHT NOT BE AVAILABLE AT
    // THAT POINT YET
    assert(world);
    ComponentRecord &cr = world->m_componentRecordIndex[LO_ENTITY_ID(cId)];
    TypeInfo *ti = cr.typeInfo;

    void *temp = world->m_wAllocator.Alloc(ti->size);

    if (ti->hook.moveCtor)
    {
        ti->hook.moveCtor(temp, data);
    }
    else if (ti->hook.copyCtor)
    {
        ti->hook.copyCtor(temp, data);
    }
    else
    {
        std::memcpy(temp, data, ti->size);
    }

    AddCommand cmd;
    cmd.cId = cId;
    cmd.data = temp;
    cmd.type = AddCmdTypeData::ASSIGN_MUT_TYPE;
    cmd.typeInfo = ti;

    addCmds.Add(world->m_wAllocator, std::move(cmd));
}

void EntityPatch::Assign(World *world, EntityId cId, const void *data)
{
    // USE LOW ENTITY ID BECAUSE RELATIONSHIP CRECORD MIGHT NOT BE AVAILABLE AT
    // THAT POINT YET
    assert(world);
    ComponentRecord &cr = world->m_componentRecordIndex[LO_ENTITY_ID(cId)];
    TypeInfo *ti = cr.typeInfo;

    void *temp = world->m_wAllocator.Alloc(ti->size);

    if (ti->hook.copyCtor)
    {
        ti->hook.copyCtor(temp, data);
    }
    else
    {
        std::memcpy(temp, data, ti->size);
    }

    AddCommand cmd;
    cmd.cId = cId;
    cmd.data = temp;
    cmd.type = AddCmdTypeData::ASSIGN_CONST_TYPE;
    cmd.typeInfo = ti;

    addCmds.Add(world->m_wAllocator, std::move(cmd));
}

void EntityPatch::Remove(World *world, EntityId cId)
{
    RemoveCommand cmd;
    cmd.cId = cId;

    removeCmds.Add(world->m_wAllocator, std::move(cmd));
}

void EntityPatch::AddCmdsSort()
{
    std::sort(addCmds.store, addCmds.store + addCmds.count,
              [](const AddCommand &a, const AddCommand &b)
              { return a.cId < b.cId; });
}

void EntityPatch::RemoveCmdsSort()
{
    std::sort(removeCmds.store, removeCmds.store + removeCmds.count,
              [](const RemoveCommand &a, const RemoveCommand &b)
              { return a.cId < b.cId; });
}
//////////////////////////////// EntityPatcher //////////////////////////////

void EntityPatcher::Flush()
{
    m_world->PatchEntity(m_patch);
    m_patch.addCmds.Destroy(m_world->m_wAllocator);
    m_patch.removeCmds.Destroy(m_world->m_wAllocator);
}

EntityPatcher &EntityPatcher::AddComponentImpl(EntityId cId)
{
    m_patch.Add(m_world, cId);
    return *this;
}
EntityPatcher &EntityPatcher::AddTagImpl(EntityId cId)
{
    m_patch.Add(m_world, cId);
    return *this;
}
EntityPatcher &EntityPatcher::AddRelationshipImpl(EntityId relationId,
                                                  EntityId targetId)
{
    m_patch.Add(m_world, MakeRelationship(relationId, targetId));
    return *this;
}
EntityPatcher &EntityPatcher::AssignComponentImpl(EntityId cId, void *data)
{
    m_patch.Assign(m_world, cId, data);
    return *this;
}
EntityPatcher &EntityPatcher::AssignComponentImpl(EntityId cId,
                                                  const void *data)
{
    m_patch.Assign(m_world, cId, data);
    return *this;
}

EntityPatcher &EntityPatcher::RemoveComponentImpl(EntityId cId)
{
    m_patch.Remove(m_world, cId);
    return *this;
}
} // namespace ECS
