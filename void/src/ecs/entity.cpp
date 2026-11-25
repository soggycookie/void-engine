#include "ecs/entity.h"
#include "ecs/ecs_type.h"
#include "ecs/internal_component_id.h"
#include "ecs/world.h"

namespace VoidEngine::ECS
{
// =========================================================
//
//                    ** EntityBuilder **
//
// =========================================================

EntityBuilder::EntityBuilder(World *world) : EntityMutator<EntityBuilder>(world)
{
    ecmd_.mode = EntityCommandMode::ENTITY_SPAWN;
    ecmd_.eid = 0;
    const TypeInfo &cr = world->get_type_infos()[NAME_ID];
    const TypeMetadata *tm = cr.meta_;
    AddCommand cmd;
    cmd.data = nullptr;
    cmd.type = TypeAddMode::ASSIGN_CONST_TYPE;
    cmd.tid = NAME_ID;
    cmd.type_meta = tm;
    ecmd_.add_cmds[ecmd_.add_count++] = cmd;
}

EntityBuilder &EntityBuilder::id(EntityId eId)
{
    ecmd_.id(eId);
    return *this;
}

EntityBuilder &EntityBuilder::name(const char *name)
{
    if (!name)
    {
        return *this;
    }

    const TypeInfo &cr = world_->get_type_infos()[NAME_ID];
    const TypeMetadata *tm = cr.meta_;

    AddCommand &cmd = ecmd_.add_cmds[0];

    uint64_t name_len = std::strlen(name);
    if (std::strlen(name) >= EcsNameLength)
    {
        name_len = EcsNameLength - 1;
    }

    char *temp = SCAST(char *, world_->get_current_scratch().push(tm->alignment, tm->size));

    std::memcpy(temp, name, name_len);
    temp[name_len] = '\0';

    cmd.data = temp;
    return *this;
}

EntityBuilder &EntityBuilder::parent(EntityId parent_eid)
{
    ecmd_.add(world_, make_relationship(CHILD_OF_ID, parent_eid));
    return *this;
}

Entity EntityBuilder::build()
{
    // In deferred mode the command is queued (replayed FIFO per worker at flush) and the returned
    // Entity has no valid id until then. Do not spawn and then patch/add/remove on it separately in
    // the same deferred frame; put everything (components, relationships, parent()) in the builder.
    ecmd_.sort();
    world_->resolve_entity_cmd(ecmd_);
    return Entity(world_, ecmd_.eid);
}

// =========================================================
//
//                    ** EntityCommand **
//
// =========================================================

void EntityCommand::add(World *world, EntityId tid)
{
    // USE LOW ENTITY ID BECAUSE RELATIONSHIP RECORD MIGHT NOT BE AVAILABLE AT
    // THAT POINT YET
    ENGINE_ASSERT(world, "World is null!");
    ENGINE_ASSERT(add_count < 16, "Entity command exceeds add command array!");
    const TypeMetadata *tm = world->get_type_infos()[low_32bit(tid)].meta_;
    AddCommand cmd;
    cmd.tid = tid;
    cmd.data = nullptr;
    cmd.type = TypeAddMode::ADD_TYPE;
    cmd.type_meta = tm;
    add_cmds[add_count++] = cmd;
}

void EntityCommand::assign_move(World *world, EntityId tid, void *data)
{
    // USE LOW ENTITY ID BECAUSE RELATIONSHIP CRECORD MIGHT NOT BE AVAILABLE AT
    // THAT POINT YET
    ENGINE_ASSERT(world, "World is null!");
    ENGINE_ASSERT(add_count < 16, "Entity command exceeds add command array!");
    const TypeMetadata *tm = world->get_type_infos()[low_32bit(tid)].meta_;

    void *temp = world->get_current_scratch().push(tm->alignment, tm->size);
    world->move_to(temp, data, tm);
    AddCommand cmd;
    cmd.tid = tid;
    cmd.data = temp;
    cmd.type = TypeAddMode::ASSIGN_MUT_TYPE;
    cmd.type_meta = tm;
    add_cmds[add_count++] = cmd;
}

void EntityCommand::assign_copy(World *world, EntityId tid, const void *data)
{
    // USE LOW ENTITY ID BECAUSE RELATIONSHIP CRECORD MIGHT NOT BE AVAILABLE AT
    // THAT POINT YET
    ENGINE_ASSERT(world, "World is null!");
    ENGINE_ASSERT(add_count < 16, "Entity command exceeds add command array!");
    const TypeMetadata *tm = world->get_type_infos()[low_32bit(tid)].meta_;

    void *temp = world->get_current_scratch().push(tm->alignment, tm->size);
    world->copy_to(temp, data, tm);
    AddCommand cmd;
    cmd.tid = tid;
    cmd.data = temp;
    cmd.type = TypeAddMode::ASSIGN_CONST_TYPE;
    cmd.type_meta = tm;
    add_cmds[add_count++] = cmd;
}

void EntityCommand::remove(World *world, EntityId tid)
{
    ENGINE_ASSERT(del_count < 16, "Entity command exceeds remove command array!");
    RemoveCommand cmd;
    cmd.tid = tid;
    del_cmds[del_count++] = cmd;
}

void EntityCommand::sort()
{
    std::sort(add_cmds, add_cmds + add_count,
              [](const AddCommand &a, const AddCommand &b) { return a.tid < b.tid; });
    std::sort(del_cmds, del_cmds + del_count,
              [](const RemoveCommand &a, const RemoveCommand &b) { return a.tid < b.tid; });
}

void EntityCommand::id(EntityId eid)
{
    this->eid = eid;
}

void EntityCommand::destroy()
{
    for (uint64_t idx = 0; idx < add_count; ++idx)
    {
        if (add_cmds[idx].data)
        {
            if (add_cmds[idx].type_meta->hook.dtor)
            {
                add_cmds[idx].type_meta->hook.dtor(add_cmds[idx].data);
            }
        }
    }
}
// =========================================================
//
//                   ** EntityPatcher **
//
// =========================================================

void EntityPatcher::build()
{
    ecmd_.sort();
    world_->resolve_entity_cmd(ecmd_);
}

} // namespace VoidEngine::ECS
