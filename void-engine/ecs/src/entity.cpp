#include "entity.h"
#include "ecs_type.h"
#include "internal_component.h"
#include "internal_component_id.h"
#include "world.h"
#include <algorithm>

namespace ECS
{
// =========================================================
//
//                    ** EntityBuilder **
//
// =========================================================

EntityBuilder &EntityBuilder::Id(EntityId eId)
{
    m_patch.Id(eId);
    return *this;
}

EntityBuilder &EntityBuilder::Name(const char *name)
{
    if (!name)
    {
        return *this;
    }

    ComponentRecord &cr = m_world->m_componentRecordIndex[EcsNameId];
    TypeInfo *ti = cr.typeInfo;

    AddCommand cmd;

    size_t nameLen = std::strlen(name);
    if (std::strlen(name) >= EcsNameLength)
    {
        nameLen = EcsNameLength - 1;
    }

    void *temp = m_world->m_wAllocator.Alloc(ti->size);

    EcsName eName;
    std::memcpy(eName.name, name, nameLen);
    eName.name[nameLen] = '\0';

    if (ti->hook.moveCtor)
    {
        ti->hook.moveCtor(temp, &eName);
    }
    else if (ti->hook.copyCtor)
    {
        ti->hook.copyCtor(temp, &eName);
    }
    else
    {
        std::memcpy(temp, &eName, ti->size);
    }

    cmd.data = temp;
    cmd.type = ComponentAddMode::ASSIGN_MUT_TYPE;

    cmd.cId = EcsNameId;
    cmd.typeInfo = ti;

    m_patch.addCmds.Add(m_world->m_wAllocator, std::move(cmd));

    return *this;
}

EntityBuilder &EntityBuilder::ChildOf(EntityId parentId)
{
    m_patch.Add(m_world, MakeRelationship(EcsChildOfId, parentId));
    return *this;
}

Entity EntityBuilder::Build()
{
    m_world->ResolveEntityCommand(m_patch);
    m_patch.addCmds.Destroy(m_world->m_wAllocator);
    m_patch.removeCmds.Destroy(m_world->m_wAllocator);

    return Entity(m_world, m_patch.eId);
}

// =========================================================
//
//                    ** EntityCommand **
//
// =========================================================

void EntityCommand::Add(World *world, EntityId cId)
{
    // USE LOW ENTITY ID BECAUSE RELATIONSHIP CRECORD MIGHT NOT BE AVAILABLE AT
    // THAT POINT YET
    assert(world);
    ComponentRecord &cr = world->m_componentRecordIndex[LO_ENTITY_ID(cId)];
    TypeInfo *ti = cr.typeInfo;

    AddCommand cmd;
    cmd.cId = cId;
    cmd.data = nullptr;
    cmd.type = ComponentAddMode::ADD_TYPE;
    cmd.typeInfo = ti;

    addCmds.Add(world->m_wAllocator, std::move(cmd));
}

void EntityCommand::Assign(World *world, EntityId cId, void *data)
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
    cmd.type = ComponentAddMode::ASSIGN_MUT_TYPE;
    cmd.typeInfo = ti;

    addCmds.Add(world->m_wAllocator, std::move(cmd));
}

void EntityCommand::Assign(World *world, EntityId cId, const void *data)
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
    cmd.type = ComponentAddMode::ASSIGN_CONST_TYPE;
    cmd.typeInfo = ti;

    addCmds.Add(world->m_wAllocator, std::move(cmd));
}

void EntityCommand::Remove(World *world, EntityId cId)
{
    RemoveCommand cmd;
    cmd.cId = cId;

    removeCmds.Add(world->m_wAllocator, std::move(cmd));
}

void EntityCommand::AddCmdsSort()
{
    std::sort(addCmds.data, addCmds.data + addCmds.count,
              [](const AddCommand &a, const AddCommand &b)
              { return a.cId < b.cId; });
}

void EntityCommand::RemoveCmdsSort()
{
    std::sort(removeCmds.data, removeCmds.data + removeCmds.count,
              [](const RemoveCommand &a, const RemoveCommand &b)
              { return a.cId < b.cId; });
}

void EntityCommand::Id(EntityId id) { eId = id; }

// =========================================================
//
//                   ** EntityPatcher **
//
// =========================================================

void EntityPatcher::Flush()
{
    m_world->ResolveEntityCommand(m_patch);
    m_patch.addCmds.Destroy(m_world->m_wAllocator);
    m_patch.removeCmds.Destroy(m_world->m_wAllocator);
}

} // namespace ECS
