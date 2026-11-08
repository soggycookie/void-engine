#include "ecs/world.h"
#include "ecs/ecs_type.h"
#include "ecs/entity.h"
#include "ecs/internal_component.h"
#include "ecs/internal_component_id.h"
#include "ecs/pipeline.h"
#include "ecs/query.h"
#include "job_system.h"

namespace VoidEngine::ECS
{

thread_local int32_t World::t_scratch_idx = -1;

void World::WorkerScratch::init()
{
    scratch_arena.init(mib(16), kib(8));
}

void World::WorkerScratch::destroy()
{
    scratch_arena.destroy();
}

void* World::WorkerScratch::push(uint64_t size, uint64_t align)
{
    return scratch_arena.push(size, align, nullptr);
}

void World::WorkerScratch::push_entity_cmd(EntityCommand &&cmd)
{
    void *addr = push(sizeof(EntityCommandNode), alignof(EntityCommandNode));
    ENGINE_ASSERT(addr, "Address is null!");
    EntityCommandNode *node = new (addr) EntityCommandNode();
    node->next = head;
    head = node;
    node->command = std::move(cmd);
}

void World::WorkerScratch::reset()
{
    scratch_arena.pop_to(0);
    head = nullptr;
}

void World::run_system(void *batch)
{
    EcsBatch* ecsBatch = RCAST(EcsBatch*, batch);
    ENGINE_ASSERT(ecsBatch, "Batch is null!");
    World* world = ecsBatch->world;
    ENGINE_ASSERT(world, "World is null!");
    World::t_scratch_idx = (world->get_worker_round_robin().fetch_add(1) % world->get_worker_count()) + 1;

    uint32_t idx = 0;
    while((idx = ecsBatch->cursor.fetch_add(1, std::memory_order_relaxed)) < ecsBatch->count)
    {
        ecsBatch->systems[idx]->execute();
    }

    World::t_scratch_idx = -1;
    ecsBatch->remaining.fetch_sub(1, std::memory_order_release);
}

void World::run_system_main(void* batch)
{
    EcsBatch* ecsBatch = RCAST(EcsBatch*, batch);
    ENGINE_ASSERT(ecsBatch, "Batch is null!");
    World* world = ecsBatch->world;
    ENGINE_ASSERT(world, "World is null!");

    uint32_t idx = 0;
    while((idx = ecsBatch->cursor.fetch_add(1, std::memory_order_relaxed)) < ecsBatch->count)
    {
        ecsBatch->systems[idx]->execute();
    }

    ecsBatch->remaining.fetch_sub(1, std::memory_order_release);
}

World *create_world(Vallocator &vallocator)
{
    World *w = construct<World>(vallocator);
    w->startup();
    return w;
}

void destroy_world(Vallocator &vallocator, World *world)
{
    world->shutdown();
    destruct(vallocator, world);
}

void World::startup()
{
    static_assert(std::is_move_constructible_v<TypeInfo>);

    t_scratch_idx = 0; //main thread
    vallocator.init(gib(64), mib(16), gib(64), mib(16));
    // InitAllocators();
    entity_records.init(vallocator, 256);
    archetypes.init(vallocator, 64);
    type_metas.init(vallocator, 64);
    cached_queries.init(vallocator, 32);
    type_infos.init(vallocator, 64);
    archetype_map.init(vallocator, 64);
    type_ids.init(vallocator, 64);
    worker_scratches.init(vallocator, 16);


    deferred = false;
    first_frame = true;

    worker_scratches.emplace_back(vallocator);
    worker_scratches[0].init();
    loop_pipeline.init(vallocator);

    register_internal_components();
    init_pipeline_phase();
    flush_deferred_cmds(); //reset the scratch
}

void World::register_internal_components()
{
    component<Name>().id(NAME_ID).register_type();
    component<EcsInherit>().id(EcsInheritId).register_type();
    component<System>()
        .id(SYSTEM_ID)
        .move_ctor(
            [](void *dest, void *src)
            {
                auto s = new (dest) System(std::move(*SCAST(System *, src)));
                SCAST(System *, src)->query = nullptr;
            })
        .dtor(
            [](void *src)
            {
                System *s = SCAST(System *, src);
                s->~System();
            })
        .register_type();

    component<Query>()
        .id(QUERY_ID)
        .move_ctor(
            [](void *dest, void *src)
            {
                new (dest) Query(std::move(*SCAST(Query *, src)));
                SCAST(System *, src)->query = nullptr;
            })
        .dtor(
            [](void *src)
            {
                Query *s = SCAST(Query *, src);
                s->~Query();
            })
        .register_type();

    component<Time>().id(TIME_ID).singleton().register_type();

    tag<Phase>().id(PHASE_ID).register_type();
    tag<EcsArchetype>().id(EcsArchetypeId).register_type();
    tag<EcsPipeline>().id(EcsPipelineId).register_type();
    tag<EcsDisabled>().id(EcsDisabledId).register_type();

    relation<ChildOf>().id(CHILD_OF_ID).exclusive().register_type();
    relation<DependOn>().id(DEPEND_ON_ID).register_type();
    relation<EcsToggle>().id(EcsToggleId).register_type();
    relation<EcsIsA>().id(EcsIsAId).register_type();
}

void World::set_worker_count(uint32_t desired_worker_count)
{
    uint32_t max_worker_count = JobSystem::get().get_desired_worker_count();
    if (max_worker_count == 1)
    {
        desired_worker_count = 0;
        return;
    }
    multithread = true;
    worker_count = std::min(desired_worker_count, max_worker_count - 1);

    for (uint64_t idx = 1; idx < worker_count + 1; ++idx)
    {
        worker_scratches.emplace_back(vallocator);
        worker_scratches[worker_scratches.get_count() - 1].init();
    }
}

World::WorkerScratch& World::get_current_scratch()
{
    return worker_scratches[t_scratch_idx];
}

void World::init_pipeline_phase()
{
    entity_builder().id(ON_BOOT_ID).name(ON_BOOT).add_tag<Phase>().build();
    entity_builder().id(ON_LOOP_ID).name(ON_LOOP).add_tag<Phase>().build();
}

void World::setup_default_pipeline()
{
    entity_builder()
        .id(ON_START_ID)
        .name(ON_START)
        .add_tag<Phase>()
        .add_relationship<DependOn>(ON_BOOT_ID)
        .build();

    entity_builder()
        .id(ON_VALIDATION_ID)
        .name(ON_VALIDATION)
        .add_tag<Phase>()
        .add_relationship<DependOn>(ON_LOOP_ID)
        .build();

    entity_builder()
        .id(ON_START_FRAME_ID)
        .name(ON_START_FRAME)
        .add_tag<Phase>()
        .add_relationship<DependOn>(ON_VALIDATION_ID)
        .build();

    entity_builder()
        .id(ON_PRE_UPDATE_ID)
        .name(ON_PRE_UPDATE)
        .add_tag<Phase>()
        .add_relationship<DependOn>(ON_START_FRAME_ID)
        .build();

    entity_builder()
        .id(ON_UPDATE_ID)
        .name(ON_UPDATE)
        .add_tag<Phase>()
        .add_relationship<DependOn>(ON_PRE_UPDATE_ID)
        .build();

    entity_builder()
        .id(ON_POST_UPDATE_ID)
        .name(ON_POST_UPDATE)
        .add_tag<Phase>()
        .add_relationship<DependOn>(ON_UPDATE_ID)
        .build();

    entity_builder()
        .id(ON_END_FRAME_ID)
        .name(ON_END_FRAME)
        .add_tag<Phase>()
        .add_relationship<DependOn>(ON_POST_UPDATE_ID)
        .build();
}

void World::register_relationship(EntityId relation_id, EntityId target_id, const TypeMetadata *relation_meta)
{
    ENGINE_ASSERT(relation_meta, "");
    ENGINE_ASSERT(target_id != ENTITY_INVALID_ID && relation_id != ENTITY_INVALID_ID, "");
    ENGINE_ASSERT(is_entity_existing(target_id) && is_entity_existing(relation_id), "");

    EntityId relationship_id = make_relationship(relation_id, target_id);

    TypeInfo type_info;
#ifdef VOID_DEBUG
    const Name &relName = get_mut<Name>(relation_id);
    const Name &targetName = get_mut<Name>(target_id);

    uint64_t rel_name_size = std::strlen(relName.name);
    uint64_t target_name_size = std::strlen(targetName.name);

    // reserve last char for null terminator
    if (rel_name_size >= MaxEntityNameLength - 1)
    {
        rel_name_size = MaxEntityNameLength - 1;
        target_name_size = 0;
    }
    else
    {
        // minus one for the space between 2 names
        uint64_t max_sec_size = (MaxEntityNameLength - 1) - rel_name_size - 1;

        if (target_name_size > max_sec_size)
        {
            target_name_size = max_sec_size;
        }
    }

    std::memcpy(type_info.name, relName.name, rel_name_size);
    if (target_name_size != 0)
    {
        type_info.name[rel_name_size] = ' ';
        std::memcpy(type_info.name + rel_name_size + 1, targetName.name, target_name_size);
        type_info.name[rel_name_size + 1 + target_name_size] = '\0';
    }
    else
    {
        type_info.name[rel_name_size] = '\0';
    }

    Entity e = entity_builder().name(type_info.name).build();
#else
    Entity e = create_entity_builder().Build();
#endif //VOID_DEBUG
    type_info.init(vallocator, relation_meta);
    type_infos.insert(vallocator, relationship_id, std::move(type_info));
}

Entity World::create_base_entity(LowEntityId eid)
{
    ENGINE_ASSERT(!entity_records.is_existing_skip_version(eid), "Id already exists!");
    bool try_reuse = false;
    if (eid == ENTITY_INVALID_ID)
    {
        eid = get_free_id();
        try_reuse = true;
    }
    uint64_t returned_id = entity_records.push(vallocator, eid, EntityRecord(), try_reuse);
    // new id is accepted
    if (try_reuse && SCAST(EntityId, eid) == returned_id)
    {
        consume_free_id(eid);
    }

    return Entity(this, returned_id);
}

EntityBuilder World::create_entity(const char *name, EntityId parent_eid)
{
    return create_entity(ENTITY_INVALID_ID, name, parent_eid);
}

EntityBuilder World::create_entity(EntityId eid, const char *name, EntityId parent_eid)
{
    auto& builder = entity_builder().name(name).id(eid);
    if(parent_eid != ENTITY_INVALID_ID)
    {
        builder.add_relationship<ChildOf>(parent_eid);
    }
    return std::move(builder);
}

EntityBuilder World::entity_builder() { return EntityBuilder(this); }

bool World::is_entity_existing(EntityId eid) const
{
    return entity_records.is_existing(eid);
}

bool World::is_type_registerd(EntityId tid) const
{
    return type_infos.contain_key(tid) && entity_records.is_existing_skip_version(tid);
}

EntityId World::get_entity_full_id(LowEntityId low_eid)
{
    ENGINE_ASSERT(entity_records.is_existing_skip_version(low_eid), "Entity with this low id does not exist!");
    return entity_records.get_full_id(low_eid);
}

LowEntityId World::get_free_id()
{
    LowEntityId next_id = next_free_eid;
    while (entity_records.is_existing(next_id))
    {
        ENGINE_ASSERT(next_id != UINT32_MAX, "Reach maximum id value!");
        ++next_id;
    }
    ENGINE_ASSERT(next_id != ENTITY_INVALID_ID, "New id must not be invalid id value!");
    return next_id;
}

void World::consume_free_id(LowEntityId new_id)
{
    next_free_eid = ++new_id;
}

Entity World::get_entity_from_id(EntityId eid)
{
    ENGINE_ASSERT(is_entity_existing(eid), "Entity does not exist!");
    return Entity(this, eid);
}

EntityRecord &World::get_entity_record(EntityId eid)
{
    ENGINE_ASSERT(is_entity_existing(eid), "Entity does not exist!");
    return entity_records[eid];
}

void World::set_child_of(EntityId child_id, EntityId parent_id)
{
    ENGINE_ASSERT(is_entity_existing(child_id), "Child entity does not exist!");
    ENGINE_ASSERT(is_entity_existing(parent_id), "Parent entity does not exist!");
    EntityRecord &r = entity_records[child_id];
    if (r.archetype)
    {
        ENGINE_ASSERT(!r.archetype->type_set.has(make_relationship(CHILD_OF_ID, WILDCARD_ID)),
            "Entity already has a parent!");
    }
    add_relationship(child_id, CHILD_OF_ID, parent_id);
}

EntityId World::get_parent(EntityId child_id)
{
    ENGINE_ASSERT(is_entity_existing(child_id), "Child entity does not exist!");
    EntityRecord &r = entity_records[child_id];
    int32_t cs_idx = r.archetype->type_set.search(make_relationship(CHILD_OF_ID, WILDCARD_ID));
    if (cs_idx == TypeSet::NOT_FOUND_IDX)
    {
        return ENTITY_INVALID_ID;
    }
    return high_32bit(r.archetype->type_set[cs_idx]);
}

// HotArray<EntityId> World::GetChildren(EntityId eId)
// {
//     EntityId relationshipId = MakeRelationship(EcsChildOfId, eId);
//     if (!component_infos.contain_key(relationshipId))
//     {
//         return HotArray<EntityId>();
//     }
//
//     ComponentInfo &cr = component_infos[relationshipId];
//     HotArray<EntityId> children;
//
//     for (uint64_t idx = 0; idx < cr.archetypes.get_count(); ++idx)
//     {
//         for (uint64_t eIdx = 0; eIdx < cr.archetypes[idx]->count; ++eIdx)
//         {
//             children.PushBack(&wallocator, cr.archetypes[idx]->entities[eIdx]);
//         }
//     }
//
//     return std::move(children);
// }

void World::remove_entity(EntityId eid)
{
    ENGINE_ASSERT(is_entity_existing(eid), "Entity does not exist!");
    EntityRecord &r = entity_records[eid];
    ENGINE_ASSERT(r.archetype, "Entity archetype must not be null!");

    swap_back_entity(r);
    for (uint64_t idx = 0; idx < r.archetype->column_count; ++idx)
    {
        Column &col = r.archetype->columns[idx];
        const TypeMetadata *ti = col.type_meta;
        ENGINE_ASSERT(ti, "Type info must not be null!");

        if (ti->hook.dtor)
        {
            void *src = OFFSET_ADDR(col.data, ti->size * (r.archetype->count - 1));
            ti->hook.dtor(src);
        }
    }
    --r.archetype->count;
    entity_records.remove(eid);
}

void World::resolve_entity_cmd(EntityCommand &command)
{
    if (deferred)
    {
        get_current_scratch().push_entity_cmd(std::move(command));
        return;
    }

    if (command.mode == EntityCommandMode::ENTITY_SPAWN)
    {
        command.eid = create_base_entity(command.eid).get_eid();

        ENGINE_ASSERT(command.add_cmds[0].tid == NAME_ID, "First add is not name!");
        // Default name
        if (!command.add_cmds[0].data)
        {
            Name eName;
            std::snprintf(eName.name, MaxEntityNameLength, DefaultEntityName,
                          low_32bit(command.eid));
            command.add_cmds[0].data = &eName;
        }
    }

    EntityRecord &r = entity_records[command.eid];
    Archetype *src_archetype = r.archetype;
    Archetype *dst_archetype = r.archetype;

    for (uint64_t idx = 0; idx < command.add_count; ++idx)
    {
        AddCommand &cmd = command.add_cmds[idx];
        if (cmd.type_meta->is_relation())
        {
            if (!type_infos.contain_key(cmd.tid))
            {
                register_relationship(low_32bit(cmd.tid), high_32bit(cmd.tid), cmd.type_meta);
            }
        }
        dst_archetype = get_or_create_archetype_add(dst_archetype, cmd.tid);
    }

    for (uint64_t idx = 0; idx < command.del_count; ++idx)
    {
        dst_archetype = get_or_create_archetype_remove(dst_archetype, command.del_cmds[idx].tid);
    }

    if (src_archetype == dst_archetype)
    {
        return;
    }

    if (dst_archetype && dst_archetype->count == dst_archetype->capacity)
    {
        dst_archetype->resize(vallocator);
    }

    if (src_archetype)
    {
        swap_back_entity(r);
    }

    uint64_t src_tset_count = (src_archetype == nullptr) ? 0 : src_archetype->type_set.count;
    uint64_t dst_tset_count = (dst_archetype == nullptr) ? 0 : dst_archetype->type_set.count;
    uint64_t max_ts_count = std::max(src_tset_count, dst_tset_count);

    uint64_t src_idx = 0;
    uint64_t dst_idx = 0;

    uint64_t add_idx = 0;
    uint64_t del_idx = 0;

    uint64_t addCmdCount = command.add_count;
    uint64_t removeCmdCount = command.del_count;
    uint64_t has_add_cmd = command.add_count > 0;
    uint64_t has_del_cmd = command.del_count > 0;


    while (true)
    {
        bool s = false;
        bool d = false;

        EntityId src_tset_id = ENTITY_INVALID_ID;
        if (src_idx < src_tset_count)
        {
            src_tset_id = src_archetype->type_set[src_idx];
            s = true;
        }

        EntityId dst_tset_id = ENTITY_INVALID_ID;
        if (dst_idx < dst_tset_count)
        {
            dst_tset_id = dst_archetype->type_set[dst_idx];
            d = true;
        }

        if (!s && !d)
        {
            break;
        }

        // same type -> move
        if (src_tset_id == dst_tset_id && src_tset_id != ENTITY_INVALID_ID)
        {
            int32_t dst_col_idx = dst_archetype->column_map[dst_idx];

            // no data type -> skip to next type
            if (dst_col_idx == TypeSet::NOT_FOUND_IDX)
            {
                ++src_idx;
                ++dst_idx;
                continue;
            }

            Column &dst_col = dst_archetype->columns[dst_col_idx];
            const TypeMetadata *ti = dst_col.type_meta;
            ENGINE_ASSERT(ti, "");
            void *dest = OFFSET_ADDR(dst_col.data, ti->size * dst_archetype->count);

            int32_t src_col_idx = src_archetype->column_map[src_idx];
            ENGINE_ASSERT(src_col_idx != TypeSet::NOT_FOUND_IDX, "Index does not exist!");
            Column &src_col = src_archetype->columns[src_col_idx];
            void *src = OFFSET_ADDR(src_col.data, ti->size * (src_archetype->count - 1));

            move_to(dest, src, ti);

            if (ti->hook.dtor)
            {
                ti->hook.dtor(src);
            }
            ++src_idx;
            ++dst_idx;
        }
        else
        {
            if (has_add_cmd)
            {
                ENGINE_ASSERT(dst_archetype, "Destination archetype must not be null!");
                ENGINE_ASSERT(add_idx < command.add_count, "Exceed add cmd count!");
                ENGINE_ASSERT(dst_tset_id == command.add_cmds[add_idx].tid, "Wrong add type!");
                AddCommand &cmd = command.add_cmds[add_idx];

                int32_t col_idx = dst_archetype->column_map[dst_idx];

                if (col_idx == TypeSet::NOT_FOUND_IDX)
                {
                    ++dst_idx;
                    ++add_idx;
                    continue;
                }

                Column &col = dst_archetype->columns[col_idx];
                const TypeMetadata *ti = col.type_meta;
                ENGINE_ASSERT(ti, "");
                void *dst = OFFSET_ADDR(col.data, ti->size * dst_archetype->count);

                switch (cmd.type)
                {
                case TypeAddMode::ADD_TYPE:
                {
                    ENGINE_ASSERT(!cmd.data, "");
                    ti->hook.ctor(dst);
                    break;
                }
                case TypeAddMode::ASSIGN_CONST_TYPE:
                {
                    ENGINE_ASSERT(cmd.data, "");
                    copy_to(dst, cmd.data, ti);
                    break;
                }
                case TypeAddMode::ASSIGN_MUT_TYPE:
                {
                    ENGINE_ASSERT(cmd.data, "");
                    move_to(dst, cmd.data, ti);
                    break;
                }
                default:
                {
                    ENGINE_ASSERT(false, "");
                }
                }
                ++dst_idx;
                ++add_idx;
            }
            else if (has_del_cmd)
            {
                ENGINE_ASSERT(src_archetype, "Source archetype must not be null!");
                ENGINE_ASSERT(del_idx < command.del_count, "Exceed del cmd count!");
                ENGINE_ASSERT(src_tset_id == command.del_cmds[del_idx].tid, "Wrong del type!");
                int32_t col_idx = src_archetype->column_map[src_idx];

                if (col_idx == TypeSet::NOT_FOUND_IDX)
                {
                    ++src_idx;
                    ++del_idx;
                    continue;
                }

                Column &col = src_archetype->columns[col_idx];
                const TypeMetadata *ti = col.type_meta;
                ENGINE_ASSERT(ti, "");

                void *src = OFFSET_ADDR(col.data, ti->size * (src_archetype->count - 1));
                if (ti->hook.dtor)
                {
                    ti->hook.dtor(src);
                }
                ++src_idx;
                ++del_idx;
            }
            else
            {
                ENGINE_ASSERT(false, "Unknown case!");
            }
        }
    }

    command.destroy();

    r.archetype = dst_archetype;
    r.row = 0;

    if (src_archetype)
    {
        --src_archetype->count;
        // std::cout << srcArchetype->name << " move 1 entity, remaining: " << srcArchetype->count << std::endl;
        // for (uint64_t i = 0; i < srcArchetype->count; ++i)
        // {
        //     std::cout << srcArchetype->entities[i] << " | ";
        // }
        // std::cout << std::endl;
        revalidate_cached_query_entity_filter(src_archetype, src_archetype->count,
                                           EntityRevalidationMode::ON_REMOVED);
    }

    if (dst_archetype)
    {
        dst_archetype->entities[dst_archetype->count] = command.eid;
        r.row = dst_archetype->count;
        revalidate_cached_query_entity_filter(dst_archetype, dst_archetype->count,
                                           EntityRevalidationMode::ON_ADDED);
        ++dst_archetype->count;
        // std::cout << destArchetype->name << " obtain 1 entity, total: " << destArchetype->count << std::endl;
        // for (uint64_t i = 0; i < destArchetype->count; ++i)
        // {
        //     std::cout << destArchetype->entities[i] << " | ";
        // }
        // std::cout << std::endl;
    }
}

void World::move_to(void *dst, void *src, const TypeMetadata *meta)
{
    ENGINE_ASSERT(dst, "Destination must not be null!");
    ENGINE_ASSERT(src, "Source must not be null!");
    ENGINE_ASSERT(meta, "Type info must not be null!");

    if (meta->is_trivial())
    {
        std::memcpy(dst, src, meta->size);
    }
    else
    {
        ENGINE_ASSERT(meta->hook.copy_ctor || meta->hook.move_ctor,
                      "Type if not trivial must have copy/move ctor!");
        if (meta->hook.move_ctor)
        {
            meta->hook.move_ctor(dst, src);
        }
        else if (meta->hook.copy_ctor)
        {
            meta->hook.copy_ctor(dst, src);
        }
    }
}

void World::copy_to(void *dst, const void *src, const TypeMetadata *meta)
{
    ENGINE_ASSERT(dst, "Destination must not be null!");
    ENGINE_ASSERT(src, "Source must not be null!");
    ENGINE_ASSERT(meta, "Type info must not be null!");

    if (meta->is_trivial())
    {
        std::memcpy(dst, src, meta->size);
    }
    else
    {
        ENGINE_ASSERT(meta->hook.copy_ctor, "Type if not trivial must have copy/move ctor!");
        if (meta->hook.copy_ctor)
        {
            meta->hook.copy_ctor(dst, src);
        }
    }
}

void World::add_component(EntityId eid, EntityId tid)
{
    ENGINE_ASSERT(is_entity_existing(eid), "Entity does not exist!");
    ENGINE_ASSERT(is_type_registerd(tid), "Type is not registered!");
    EntityRecord &r = entity_records[eid];
    const TypeMetadata *tm = type_infos[tid].meta;

    ENGINE_ASSERT(tm->is_component(), "");
    ENGINE_ASSERT(!tm->is_singleton(), "");
    if (r.archetype)
    {
        ENGINE_ASSERT(!r.archetype->type_set.has(tid), "Component already exist in set!");
    }

    if (deferred)
    {
        EntityCommand cmd;
        cmd.add(this, tid);
        cmd.mode = EntityCommandMode::ENTITY_PATCH;
        get_current_scratch().push_entity_cmd(std::move(cmd));
    }
    else
    {
        Archetype *dst_arch = get_or_create_archetype_add(r.archetype, tid);
        ENGINE_ASSERT(dst_arch, "Destination archetype must not be null!");
        move_archetype_add_op(eid, r, dst_arch);
    }
}

void World::assign_component_move(EntityId eid, EntityId tid, void *data)
{
    ENGINE_ASSERT(is_entity_existing(eid), "Entity does not exist!");
    ENGINE_ASSERT(is_type_registerd(tid), "Type is not registered!");
    EntityRecord &r = entity_records[eid];
    const TypeMetadata *tm = type_infos[tid].meta;

    ENGINE_ASSERT(tm->is_component(), "");
    ENGINE_ASSERT(!tm->is_singleton(), "");
    if (r.archetype)
    {
        ENGINE_ASSERT(!r.archetype->type_set.has(tid), "Component already exist in set!");
    }

    if (deferred)
    {
        EntityCommand cmd;
        cmd.assign_move(this, tid, data);
        cmd.mode = EntityCommandMode::ENTITY_PATCH;
        get_current_scratch().push_entity_cmd(std::move(cmd));
    }
    else
    {
        Archetype *dst_arch = get_or_create_archetype_add(r.archetype, tid);
        move_archetype_move_op(eid, r, dst_arch, data);
    }
}

void World::assign_component_copy(EntityId eid, EntityId tid, const void *data)
{
    ENGINE_ASSERT(is_entity_existing(eid), "Entity does not exist!");
    ENGINE_ASSERT(is_type_registerd(tid), "Type is not registered!");
    EntityRecord &r = entity_records[eid];
    const TypeMetadata *tm = type_infos[tid].meta;

    ENGINE_ASSERT(tm->is_component(), "");
    ENGINE_ASSERT(!tm->is_singleton(), "");
    if (r.archetype)
    {
        ENGINE_ASSERT(!r.archetype->type_set.has(tid), "Component already exist in set!");
    }

    if (deferred)
    {
        EntityCommand cmd;
        cmd.assign_copy(this, tid, data);
        cmd.mode = EntityCommandMode::ENTITY_PATCH;
        get_current_scratch().push_entity_cmd(std::move(cmd));
    }
    else
    {
        Archetype *dst_arch = get_or_create_archetype_add(r.archetype, tid);
        move_archetype_copy_op(eid, r, dst_arch, data);
    }
}

void World::add_relationship(EntityId eid, EntityId relation_id, EntityId target_eid)
{
    ENGINE_ASSERT(is_entity_existing(eid), "Entity does not exist!");
    ENGINE_ASSERT(is_entity_existing(target_eid), "Target entity does not exist!");
    ENGINE_ASSERT(is_type_registerd(relation_id), "Type is not registered!");

    EntityId relationship_id = make_relationship(relation_id, target_eid);
    EntityRecord &r = entity_records[eid];
    const TypeMetadata *tm = type_infos[relation_id].meta;
    ENGINE_ASSERT(tm->is_relation(), "");
    ENGINE_ASSERT(!tm->is_singleton(), "");
    if (r.archetype)
    {
        ENGINE_ASSERT(!r.archetype->type_set.has(relationship_id), "Relationship already exist in set!");
        if (tm->is_exclusive())
        {
            ENGINE_ASSERT(!r.archetype->type_set.has(make_relationship(relationship_id, WILDCARD_ID)),
                          "Relation already exists in set!");
        }
    }

    if (deferred)
    {
        EntityCommand cmd;
        cmd.add(this, relationship_id);
        cmd.mode = EntityCommandMode::ENTITY_PATCH;
        get_current_scratch().push_entity_cmd(std::move(cmd));
    }
    else
    {
        if (!type_infos.contain_key(relationship_id))
        {
            register_relationship(relation_id, target_eid, tm);
        }
        Archetype *dst_arch = get_or_create_archetype_add(r.archetype, relationship_id);
        ENGINE_ASSERT(dst_arch, "Destination archetype must not be null!");
        move_archetype_add_op(eid, r, dst_arch);
    }
}

void World::assign_relationship_move(EntityId eid, EntityId relation_id, EntityId target_eid, void *data)
{
    ENGINE_ASSERT(is_entity_existing(eid), "Entity does not exist!");
    ENGINE_ASSERT(is_entity_existing(target_eid), "Target entity does not exist!");
    ENGINE_ASSERT(is_type_registerd(relation_id), "Type is not registered!");

    EntityId relationship_id = make_relationship(relation_id, target_eid);

    EntityRecord &r = entity_records[eid];
    const TypeMetadata *tm = type_infos[relation_id].meta;
    ENGINE_ASSERT(tm->is_relation(), "");
    ENGINE_ASSERT(tm->has_data(), "");
    ENGINE_ASSERT(!tm->is_singleton(), "");
    if (r.archetype)
    {
        ENGINE_ASSERT(!r.archetype->type_set.has(relationship_id), "Relationship already exist in set!");
        if (tm->is_exclusive())
        {
            ENGINE_ASSERT(!r.archetype->type_set.has(make_relationship(relationship_id, WILDCARD_ID)),
                          "Relation already exists in set!");
        }
    }

    if (deferred)
    {
        EntityCommand cmd;
        cmd.assign_move(this, relationship_id, data);
        cmd.mode = EntityCommandMode::ENTITY_PATCH;
        get_current_scratch().push_entity_cmd(std::move(cmd));
    }
    else
    {
        if (!type_infos.contain_key(relationship_id))
        {
            register_relationship(relation_id, target_eid, tm);
        }
        Archetype *destArchetype = get_or_create_archetype_add(r.archetype, relationship_id);
        move_archetype_move_op(eid, r, destArchetype, data);
    }
}

void World::assign_relationship_copy(EntityId eid, EntityId relation_id, EntityId target_eid, const void *data)
{
    ENGINE_ASSERT(is_entity_existing(eid), "Entity does not exist!");
    ENGINE_ASSERT(is_entity_existing(target_eid), "Target entity does not exist!");
    ENGINE_ASSERT(is_type_registerd(relation_id), "Type is not registered!");

    EntityId relationship_id = make_relationship(relation_id, target_eid);

    EntityRecord &r = entity_records[eid];
    const TypeMetadata *tm = type_infos[relation_id].meta;
    ENGINE_ASSERT(tm->is_relation(), "");
    ENGINE_ASSERT(tm->has_data(), "");
    ENGINE_ASSERT(!tm->is_singleton(), "");
    if (r.archetype)
    {
        ENGINE_ASSERT(!r.archetype->type_set.has(relationship_id), "Relationship already exist in set!");
        if (tm->is_exclusive())
        {
            ENGINE_ASSERT(!r.archetype->type_set.has(make_relationship(relationship_id, WILDCARD_ID)),
                          "Relation already exists in set!");
        }
    }

    if (deferred)
    {
        EntityCommand cmd;
        cmd.assign_copy(this, relationship_id, data);
        cmd.mode = EntityCommandMode::ENTITY_PATCH;
        get_current_scratch().push_entity_cmd(std::move(cmd));
    }
    else
    {
        if (!type_infos.contain_key(relationship_id))
        {
            register_relationship(relation_id, target_eid, tm);
        }
        Archetype *destArchetype = get_or_create_archetype_add(r.archetype, relationship_id);
        move_archetype_copy_op(eid, r, destArchetype, data);
    }
}

void World::add_tag(EntityId eid, EntityId tid)
{
    ENGINE_ASSERT(is_entity_existing(eid), "Entity does not exist!");
    ENGINE_ASSERT(is_type_registerd(tid), "Type is not registered!");
    EntityRecord &r = entity_records[eid];
    const TypeMetadata *tm = type_infos[tid].meta;
    ENGINE_ASSERT(tm->is_tag(), "");
    ENGINE_ASSERT(!tm->is_singleton(), "");
    if (r.archetype)
    {
        ENGINE_ASSERT(!r.archetype->type_set.has(tid), "Component already exist in set!");
    }

    if (deferred)
    {
        EntityCommand cmd;
        cmd.add(this, tid);
        cmd.mode = EntityCommandMode::ENTITY_PATCH;
        get_current_scratch().push_entity_cmd(std::move(cmd));
    }
    else
    {
        Archetype *dst_arch = get_or_create_archetype_add(r.archetype, tid);
        ENGINE_ASSERT(dst_arch, "Destination archetype must not be null!");
        move_archetype_add_op(eid, r, dst_arch);
        if (tm->hook.on_add)
        {
            tm->hook.on_add(nullptr, this);
        }
    }
}

void World::remove_type(EntityId eid, EntityId tid)
{
    ENGINE_ASSERT(is_entity_existing(eid), "Entity does not exist!");
    ENGINE_ASSERT(is_type_registerd(tid), "Type is not registered!");
    EntityRecord &r = entity_records[eid];
    const TypeMetadata *tm = type_infos[tid].meta;
    ENGINE_ASSERT(r.archetype, "");
    ENGINE_ASSERT(r.archetype->type_set.has(tid), "");

    if (deferred)
    {
        EntityCommand cmd;
        cmd.remove(this, eid);
        cmd.mode = EntityCommandMode::ENTITY_PATCH;
        get_current_scratch().push_entity_cmd(std::move(cmd));
    }
    else
    {
        Archetype *destArchetype = get_or_create_archetype_remove(r.archetype, tid);
        move_archetype_remove_op(eid, r, destArchetype);
        if (tm->is_tag() && tm->hook.on_remove)
        {
            tm->hook.on_remove(nullptr, this);
        }
    }
}

bool World::has_type(EntityId eid, EntityId tid)
{
    ENGINE_ASSERT(is_entity_existing(eid), "Entity does not exist!");
    if (high_32bit(tid) == WILDCARD_ID)
    {
        ENGINE_ASSERT(is_type_registerd(low_32bit(tid)), "Type is not registered!");
    }
    else
    {
        ENGINE_ASSERT(is_type_registerd(tid), "Type is not registered!");
    }
    EntityRecord &r = entity_records[eid];
    ENGINE_ASSERT(r.archetype, "Entity archetype must not be null!");

    return r.archetype->type_set.has(tid);
}

void World::set_move(EntityId eid, EntityId cid, void *data)
{
    ENGINE_ASSERT(is_entity_existing(eid), "Entity does not exist!");
    ENGINE_ASSERT(is_type_registerd(cid), "Type is not registered!");
    EntityRecord &r = entity_records[eid];
    ENGINE_ASSERT(r.archetype, "Entity archetype must not be null!");
    int32_t idx = r.archetype->type_set.search(cid);
    ENGINE_ASSERT(idx != TypeSet::NOT_FOUND_IDX, "Type does not exist in set!");
    int32_t col_idx = r.archetype->column_map[idx];
    ENGINE_ASSERT(col_idx != Archetype::INVALID_COLUMN, "Type does not contain data!");
    Column &col = r.archetype->columns[col_idx];
    const TypeMetadata *ti = col.type_meta;
    ENGINE_ASSERT(ti, "Type info must not be null!");
    void *dst = OFFSET_ADDR(col.data, ti->size * r.row);
    move_to(dst, data, ti);

    revalidate_cached_query_entity_filter(r.archetype, r.row, EntityRevalidationMode::ON_MODIFIED);

    ti->hook.on_set(dst, this);
}

void World::set_copy(EntityId eid, EntityId cid, const void *data)
{
    ENGINE_ASSERT(is_entity_existing(eid), "Entity does not exist!");
    ENGINE_ASSERT(is_type_registerd(cid), "Type is not registered!");
    EntityRecord &r = entity_records[eid];
    ENGINE_ASSERT(r.archetype, "Entity archetype must not be null!");
    int32_t idx = r.archetype->type_set.search(cid);
    ENGINE_ASSERT(idx != TypeSet::NOT_FOUND_IDX, "Type does not exist in set!");
    int32_t col_idx = r.archetype->column_map[idx];
    ENGINE_ASSERT(col_idx != Archetype::INVALID_COLUMN, "Type does not contain data!");
    Column &col = r.archetype->columns[col_idx];
    const TypeMetadata *ti = col.type_meta;
    ENGINE_ASSERT(ti, "Type info must not be null!");
    void *dst = OFFSET_ADDR(col.data, ti->size * r.row);
    copy_to(dst, data, ti);

    revalidate_cached_query_entity_filter(r.archetype, r.row, EntityRevalidationMode::ON_MODIFIED);

    ti->hook.on_set(dst, this);
}

void *World::get(EntityId eid, EntityId cid)
{
    ENGINE_ASSERT(is_entity_existing(eid), "Entity does not exist!");
    ENGINE_ASSERT(is_type_registerd(cid), "Type is not registered!");
    EntityRecord &r = entity_records[eid];
    ENGINE_ASSERT(r.archetype, "Entity archetype must not be null!");
    int32_t idx = r.archetype->type_set.search(cid);
    ENGINE_ASSERT(idx != TypeSet::NOT_FOUND_IDX, "Type does not exist in set!");
    int32_t col_idx = r.archetype->column_map[idx];
    ENGINE_ASSERT(col_idx != Archetype::INVALID_COLUMN, "Type does not contain data!");
    Column &col = r.archetype->columns[col_idx];
    const TypeMetadata *ti = col.type_meta;
    ENGINE_ASSERT(ti, "Type info must not be null!");
    void *component = OFFSET_ADDR(col.data, ti->size * r.row);

    return component;
}

void World::swap_back_entity(EntityRecord &r)
{
    ENGINE_ASSERT(r.archetype, "Entity archetype must not be null!");
    Archetype &archetype = *r.archetype;
    if (archetype.count == 0 || (archetype.count == r.row + 1))
    {
        return;
    }

    EntityId swap_id = archetype.entities[r.row];
    EntityId back_id = archetype.entities[archetype.count - 1];
    EntityRecord *back_record = entity_records.get(back_id);
    ENGINE_ASSERT(back_record, "");
    archetype.entities[r.row] = back_id;
    archetype.entities[archetype.count - 1] = swap_id;
    for (uint64_t idx = 0; idx < r.archetype->column_count; idx++)
    {
        Column &col = r.archetype->columns[idx];
        const TypeMetadata &meta = *col.type_meta;
        void *swap = OFFSET_ADDR(col.data, meta.size * r.row);
        void *back = OFFSET_ADDR(col.data, meta.size * (archetype.count - 1));
        void *temp = get_current_scratch().push(meta.size, meta.alignment);
        ENGINE_ASSERT(temp, "");

        if (meta.is_trivial())
        {
            std::memcpy(temp, swap, meta.size);
            std::memcpy(swap, back, meta.size);
            std::memcpy(back, temp, meta.size);
        }
        else
        {
            ENGINE_ASSERT(meta.hook.copy_ctor || meta.hook.move_ctor,
                          "Type if not trivial must have copy/move ctor!");
            if (meta.hook.move_ctor)
            {
                meta.hook.move_ctor(temp, swap);
                meta.hook.move_ctor(swap, back);
                meta.hook.move_ctor(back, temp);
            }
            else if (meta.hook.copy_ctor)
            {
                meta.hook.copy_ctor(temp, swap);
                meta.hook.copy_ctor(swap, back);
                meta.hook.copy_ctor(back, temp);
            }
        }

        if (meta.hook.dtor)
        {
            meta.hook.dtor(temp);
        }
    }

    back_record->row = r.row;
    r.row = archetype.count - 1;
}

void World::update_type_info_tracked_queries(Query_T *query)
{
    ENGINE_ASSERT(query, "Query must not be null!");
    for (uint64_t idx = 0; idx < query->term_count; ++idx)
    {
        const QueryTerm &term = query->terms[idx];
        if (term.op == HAS)
        {
            switch (term.trav_method)
            {
            case SELF:
            {
                EntityId high_id = high_32bit(term.tid);
                EntityId tid = high_id == WILDCARD_ID ? low_32bit(term.tid) : term.tid;
                TypeInfo &ti = type_infos[tid];
                ti.tracked_queries.push_back(vallocator, query->eid);
                break;
            }
            case UP:
            case CASCADE:
            {
                EntityId tid = term.trav_target == WILDCARD_ID
                                   ? term.trav_relation
                                   : make_relationship(term.trav_relation, term.trav_target);
                TypeInfo &ti = type_infos[tid];
                ti.tracked_queries.push_back(vallocator, query->eid);
                break;
            }
            default:
            {
                ENGINE_ASSERT(false, "");
            }
            }
        }
    }
}

Query_T *World::create_query(uint32_t query_id)
{
    cached_queries.push(vallocator, query_id, Query_T());
    Query_T * q = cached_queries.get(query_id);
    q->init(this);
    return q;
}

Archetype *World::create_archetype(TypeSet &&type_set)
{
    ArchetypeId id = get_archetype_id();
    Archetype archetype;
    archetype.init(vallocator, id, std::move(type_set), type_infos);
    archetypes.push(vallocator, id, std::move(archetype));
    Archetype &new_archetype = archetypes[id];

    // Add newly created archetype to component record for query purpose
    // if component record is relationship,
    // add this archetype to base relation and specific relationship

    // Notify all the cached queries

    ++sweep_val;

    const TypeSet &tset = new_archetype.type_set;
    for (uint32_t idx = 0; idx < tset.count; idx++)
    {
        TypeInfo &ti = type_infos[tset[idx]];
        revalidate_cached_query_archetype_filter(ti, &new_archetype, true);
        // base relation
        if (ti.meta->is_relation())
        {
            TypeInfo &base_info = type_infos[low_32bit(tset[idx])];
            revalidate_cached_query_archetype_filter(base_info, &new_archetype, true);
            base_info.archetypes.push_back(vallocator, archetypes.get(id));
        }
        ti.archetypes.push_back(vallocator, archetypes.get(id));
    }
    // shallow copy
    archetype_map.insert(vallocator, new_archetype.type_set, archetypes.get(id));

    return &new_archetype;
}

Archetype *World::get_archetype(const TypeSet &componentSet)
{
    if (archetype_map.contain_key(componentSet))
    {
        return archetype_map[componentSet];
    }

    return nullptr;
}

const Archetype *World::get_entity_archetype(EntityId eid) const
{
    ENGINE_ASSERT(is_entity_existing(eid), "Entity does not exist!");
    return entity_records[eid].archetype;
}

Archetype *World::get_or_create_archetype_add(Archetype *src_arch, EntityId tid)
{
    Archetype *dst_arch = nullptr;
    // find or create edge
    if (src_arch)
    {
        if (src_arch->add_edges.contain_key(tid))
        {
            dst_arch = src_arch->add_edges[tid];
        }
        else
        {
            TypeSet type_set;
            type_set.init_add(vallocator, &src_arch->type_set, tid);
            dst_arch = get_archetype(type_set);
            if (!dst_arch)
            {
                dst_arch = create_archetype(std::move(type_set));
            }
            else
            {
                type_set.destroy(vallocator);
            }
            src_arch->add_edges.insert(vallocator, tid, dst_arch);
        }
    }
    else
    {
        TypeSet type_set;
        type_set.init_add(vallocator, nullptr, tid);
        dst_arch = get_archetype(type_set);
        if (!dst_arch)
        {
            dst_arch = create_archetype(std::move(type_set));
        }
        else
        {
            type_set.destroy(vallocator);
        }
    }
    return dst_arch;
}

Archetype *World::get_or_create_archetype_remove(Archetype *src_arch, EntityId tid)
{
    ENGINE_ASSERT(src_arch, "Source archetype must not be null!");
    Archetype *dst_arch = nullptr;
    if (src_arch->del_edges.contain_key(tid))
    {
        dst_arch = src_arch->del_edges[tid];
    }
    else
    {
        TypeSet type_set;
        type_set.init_remove(vallocator, &src_arch->type_set, tid);
        dst_arch = get_archetype(type_set);
        if (!dst_arch)
        {
            dst_arch = create_archetype(std::move(type_set));
        }
        else
        {
            type_set.destroy(vallocator);
        }

        src_arch->del_edges.insert(vallocator, tid, dst_arch);
    }

    return dst_arch;
}

void World::move_archetype_add_op(EntityId eid, EntityRecord &r, Archetype *dst_arch)
{
    Archetype *src_arch = r.archetype;
    ENGINE_ASSERT(dst_arch, "Destination archetype is null!");

    if (dst_arch->count == dst_arch->capacity)
    {
        dst_arch->resize(vallocator);
    }

    if (!src_arch)
    {
        ENGINE_ASSERT(dst_arch->type_set.count == 1,
                   "Destination archetype has more than 1 component!");
        if (dst_arch->column_count == 1)
        {
            const TypeMetadata *ti = dst_arch->columns[0].type_meta;
            void *dest =
                OFFSET_ADDR(dst_arch->columns[0].data, dst_arch->count * ti->size);
            ti->hook.ctor(dest);
            if (ti->hook.on_add)
            {
                ti->hook.on_add(dest, this);
            }
        }
    }
    else
    {
        // SWAP BACK IN SRC ARCHETYPE
        swap_back_entity(r);
        for (uint64_t idx = 0; idx < dst_arch->column_count; ++idx)
        {
            Column &dst_col = dst_arch->columns[idx];
            const TypeMetadata *ti = dst_col.type_meta;
            ENGINE_ASSERT(ti, "");
            void *dst = OFFSET_ADDR(dst_col.data, ti->size * dst_arch->count);
            int32_t set_idx = dst_arch->column_map[dst_arch->type_set.count + idx];
            int32_t src_idx = src_arch->type_set.search(dst_arch->type_set[idx]);

            if (src_idx == TypeSet::NOT_FOUND_IDX)
            {
                ti->hook.ctor(dst);
                if (ti->hook.on_add)
                {
                    ti->hook.on_add(dst, this);
                }
            }
            else
            {
                ENGINE_ASSERT(src_arch->column_map[src_idx] != Archetype::INVALID_COLUMN, "");
                Column &src_col = src_arch->columns[src_arch->column_map[src_idx]];
                void *src = OFFSET_ADDR(src_col.data, ti->size * r.row);

                move_to(dst, src, ti);
                if (ti->hook.dtor)
                {
                    ti->hook.dtor(src);
                }
            }
        }
        --src_arch->count;
    }
    uint32_t remove_row = r.row;
    dst_arch->entities[dst_arch->count] = eid;
    r.archetype = dst_arch;
    r.row = dst_arch->count;
    ++dst_arch->count;

    revalidate_cached_query_entity_filter(src_arch, remove_row, EntityRevalidationMode::ON_REMOVED);
    revalidate_cached_query_entity_filter(dst_arch, dst_arch->count - 1,
                                       EntityRevalidationMode::ON_ADDED);
}

void World::move_archetype_move_op(EntityId eid, EntityRecord &r, Archetype *dst_arch, void *data)
{
    ENGINE_ASSERT(dst_arch, "Destination archeytype must not be null!");
    ENGINE_ASSERT(data, "Data must not be null!");
    Archetype *src_arch = r.archetype;

    if (dst_arch->count == dst_arch->capacity)
    {
        dst_arch->resize(vallocator);
    }

    if (!src_arch)
    {
        ENGINE_ASSERT(dst_arch->type_set.count == 1,
                   "Destination archetype must have only 1 component!");
        if (dst_arch->column_count == 1)
        {
            const TypeMetadata *ti = dst_arch->columns[0].type_meta;
            ENGINE_ASSERT(ti, "");
            void *dest = OFFSET_ADDR(dst_arch->columns[0].data, dst_arch->count * ti->size);

            move_to(dest, data, ti);
            if (ti->hook.on_add)
            {
                ti->hook.on_add(dest, this);
            }
            if (ti->hook.on_set)
            {
                ti->hook.on_set(dest, this);
            }
        }
    }
    else
    {
        // SWAP BACK IN SRC ARCHETYPE
        swap_back_entity(r);
        for(uint64_t idx = 0; idx < dst_arch->column_count; ++idx)
        {
            Column& dst_col = dst_arch->columns[idx];
            const TypeMetadata* ti = dst_col.type_meta;
            ENGINE_ASSERT(ti, "");
            void* dst = OFFSET_ADDR(dst_col.data, ti->size * dst_arch->count);
            int32_t set_idx = dst_arch->column_map[dst_arch->type_set.count + idx];
            int32_t src_idx = src_arch->type_set.search(dst_arch->type_set[set_idx]);

            if(src_idx == TypeSet::NOT_FOUND_IDX)
            {
                move_to(dst, data, ti);
                if (ti->hook.on_add)
                {
                    ti->hook.on_add(dst, this);
                }
                if (ti->hook.on_set)
                {
                    ti->hook.on_set(dst, this);
                }
            }
            else
            {
                ENGINE_ASSERT(src_arch->column_map[src_idx] != Archetype::INVALID_COLUMN, "");
                Column &src_col = src_arch->columns[src_arch->column_map[src_idx]];
                void *src = OFFSET_ADDR(src_col.data, ti->size * r.row);

                move_to(dst, src, ti);
                if(ti->hook.dtor)
                {
                    ti->hook.dtor(src);
                }
            }
        }
        --src_arch->count;
    }

    uint32_t remove_row = r.row;
    dst_arch->entities[dst_arch->count] = eid;
    r.archetype = dst_arch;
    r.row = dst_arch->count;
    ++dst_arch->count;

    revalidate_cached_query_entity_filter(src_arch, remove_row,
                                       EntityRevalidationMode::ON_REMOVED);
    revalidate_cached_query_entity_filter(dst_arch, dst_arch->count - 1,
                                       EntityRevalidationMode::ON_ADDED);
}

void World::move_archetype_copy_op(EntityId eid, EntityRecord &r, Archetype *dst_arch, const void *data)
{
    Archetype *src_arch = r.archetype;
    ENGINE_ASSERT(dst_arch, "Destination archetype must not be null!");

    if (dst_arch->count == dst_arch->capacity)
    {
        dst_arch->resize(vallocator);
    }

    // empty entity
    if (!src_arch)
    {
        ENGINE_ASSERT(dst_arch, "Destination archeytype is null!");
        if (dst_arch->column_count == 1)
        {
            const TypeMetadata *ti = dst_arch->columns[0].type_meta;
            ENGINE_ASSERT(ti, "");
            void *dst = OFFSET_ADDR(dst_arch->columns[0].data, dst_arch->count * ti->size);

            copy_to(dst, data, ti);
            if (ti->hook.on_add)
            {
                ti->hook.on_add(dst, this);
            }
            if (ti->hook.on_set)
            {
                ti->hook.on_set(dst, this);
            }
        }
    }
    else
    {
        // SWAP BACK IN SRC ARCHETYPE
        swap_back_entity(r);
        for (uint64_t idx = 0; idx < dst_arch->column_count; ++idx)
        {
            Column &dst_col = dst_arch->columns[idx];
            const TypeMetadata *ti = dst_col.type_meta;
            ENGINE_ASSERT(ti, "");
            void *dst = OFFSET_ADDR(dst_col.data, ti->size * dst_arch->count);
            int32_t set_idx = dst_arch->column_map[dst_arch->type_set.count + idx];
            int32_t src_idx = src_arch->type_set.search(dst_arch->type_set[set_idx]);

            if (src_idx == TypeSet::NOT_FOUND_IDX)
            {
                copy_to(dst, data, ti);
                if (ti->hook.on_add)
                {
                    ti->hook.on_add(dst, this);
                }
                if (ti->hook.on_set)
                {
                    ti->hook.on_set(dst, this);
                }
            }
            else
            {
                ENGINE_ASSERT(src_arch->column_map[src_idx] != Archetype::INVALID_COLUMN, "");
                Column &src_col = src_arch->columns[src_arch->column_map[src_idx]];
                void *src = OFFSET_ADDR(src_col.data, ti->size * r.row);

                move_to(dst, src, ti);
                if (ti->hook.dtor)
                {
                    ti->hook.dtor(src);
                }
            }
        }
        --src_arch->count;
    }

    uint32_t remove_row = r.row;
    dst_arch->entities[dst_arch->count] = eid;
    r.archetype = dst_arch;
    r.row = dst_arch->count;
    ++dst_arch->count;

    revalidate_cached_query_entity_filter(src_arch, remove_row, EntityRevalidationMode::ON_REMOVED);
    revalidate_cached_query_entity_filter(dst_arch, dst_arch->count - 1,
                                       EntityRevalidationMode::ON_ADDED);
}

void World::move_archetype_remove_op(EntityId eid, EntityRecord &r, Archetype *dst_arch)
{
    Archetype *src_arch = r.archetype;
    swap_back_entity(r);
    uint32_t remove_row = r.row;
    if (!dst_arch)
    {
        ENGINE_ASSERT(src_arch->type_set.count == 1, "Source archetype must have only 1 component!");
        if (src_arch->column_count == 1)
        {
            Column &src_col = src_arch->columns[0];
            const TypeMetadata *ti = src_col.type_meta;
            ENGINE_ASSERT(ti, "");
            void *src = OFFSET_ADDR(src_col.data, ti->size * (src_arch->count - 1));

            if (ti->hook.on_remove)
            {
                ti->hook.on_remove(src, this);
            }
            if (ti->hook.dtor)
            {
                ti->hook.dtor(src);
            }
        }
        r.row = 0;
        r.archetype = nullptr;
        --src_arch->count;
    }
    else
    {
        if (dst_arch->count == dst_arch->capacity)
        {
            dst_arch->resize(vallocator);
        }

        for (uint64_t idx = 0; idx < src_arch->column_count; ++idx)
        {
            Column &src_col = src_arch->columns[idx];
            const TypeMetadata *ti = src_col.type_meta;
            ENGINE_ASSERT(ti, "");
            void *src = OFFSET_ADDR(src_col.data, ti->size * r.row);
            int32_t set_idx = src_arch->column_map[src_arch->type_set.count + idx];
            int32_t dst_idx = dst_arch->type_set.search(src_arch->type_set[set_idx]);
            if (dst_idx != TypeSet::NOT_FOUND_IDX)
            {
                int32_t dst_col_idx = dst_arch->column_map[dst_idx];
                ENGINE_ASSERT(dst_col_idx != TypeSet::NOT_FOUND_IDX, "");
                Column &dst_col = dst_arch->columns[dst_col_idx];
                void *dst = OFFSET_ADDR(dst_col.data, ti->size * dst_arch->count);

                move_to(dst, src, src_col.type_meta);
                if (ti->hook.dtor)
                {
                    ti->hook.dtor(src);
                }
            }
            else
            {
                if (ti->hook.on_remove)
                {
                    ti->hook.on_remove(src, this);
                }
                if (ti->hook.dtor)
                {
                    ti->hook.dtor(src);
                }
            }
        }

        dst_arch->entities[dst_arch->count] = eid;
        r.archetype = dst_arch;
        r.row = dst_arch->count;
        --src_arch->count;
        ++dst_arch->count;
    }

    revalidate_cached_query_entity_filter(src_arch, remove_row, EntityRevalidationMode::ON_REMOVED);
    revalidate_cached_query_entity_filter(dst_arch, dst_arch == nullptr ? 0 : dst_arch->count - 1,
                                       EntityRevalidationMode::ON_ADDED);
}

void World::revalidate_cached_query_entity_filter(Archetype *archetype, uint32_t affected_row,
                                               EntityRevalidationMode mode)
{
    if (!archetype)
    {
        return;
    }

    // TODO: should query/system an entity, yes? it needs to build a dependency graph
    // should the entity own system/query directly or ptr
    for (uint64_t idx = 0; idx < archetype->tracked_queries.get_count(); ++idx)
    {
        TrackedQuery &tracked_query = archetype->tracked_queries[idx];
        EntityId id = entity_records.get_full_id(tracked_query.id);

        Query_T *query = nullptr;
        if (has_type<Query>(id))
        {
            query = get_mut<Query>(id).query;
        }
        else if (has_type<System>(id))
        {
            query = get_mut<System>(id).query;
        }
        else
        {
            ENGINE_ASSERT(false, "");
        }

        if (!query->is_filtered)
        {
            continue;
        }

        MatchedArchetype &matched = query->result[tracked_query.queryArchetypeIdx];

        switch (mode)
        {
        case World::EntityRevalidationMode::ON_ADDED:
        {
            // Run entity filter on last entity
            query->filter_entity(matched, affected_row);
            break;
        }
        case World::EntityRevalidationMode::ON_REMOVED:
        {
            // Swap bitmask at count idx and remove idx (count is probably
            // minus 1 at this point)
            bool bit = matched.get_mask(matched.archetype->count - 1);
            matched.set_mask(vallocator, matched.archetype->count - 1, false);
            matched.set_mask(vallocator, affected_row, bit);
            break;
        }
        case World::EntityRevalidationMode::ON_MODIFIED:
        {
            query->filter_entity(matched, affected_row);
            break;
        }
        }
    }
}

void World::revalidate_cached_query_archetype_filter(TypeInfo &type_info, Archetype *archetype,
                                                  bool is_new_archetype)
{
    if (is_new_archetype)
    {
        for (uint64_t idx = 0; idx < type_info.tracked_queries.get_count(); ++idx)
        {

            Query_T *query = nullptr;
            if (has_type<Query>(type_info.tracked_queries[idx]))
            {
                query = get_mut<Query>(type_info.tracked_queries[idx]).query;
            }
            else if (has_type<System>(type_info.tracked_queries[idx]))
            {
                query = get_mut<System>(type_info.tracked_queries[idx]).query;
            }
            ENGINE_ASSERT(query, "");

            if (query->last_sweep_archetype == sweep_val)
            {
                continue;
            }

            query->last_sweep_archetype = sweep_val;
            query->add_if_match(archetype);
        }
    }
    else
    {
        // TODO: handle cached query when remove archetype
    }
}

PhaseDependencyBuilder World::create_bootstrap_phase_dependency()
{
    return PhaseDependencyBuilder(this, ON_BOOT_ID);
}

PhaseDependencyBuilder World::create_loop_phase_dependency()
{
    return PhaseDependencyBuilder(this, ON_LOOP_ID);
}

void World::flush_deferred_cmds()
{
    for (uint64_t idx = 0; idx < worker_scratches.get_count(); ++idx)
    {
        EntityCommandNode *node = worker_scratches[idx].get_cmds();
        while(node)
        {
            resolve_entity_cmd(node->command);
            node = node->next;
        }
        worker_scratches[idx].reset();
    }
    system_batch.cursor.store(0);
    system_batch.remaining.store(0);
    system_batch.count = 0;
}

void World::tick()
{
    start_defer();
    if (first_frame)
    {
        Pipeline boot_pipeline;
        boot_pipeline.init(vallocator);
        boot_pipeline.build_from_base_phase(this, ON_BOOT_ID);
        boot_pipeline.progress(this);

        loop_pipeline.build_from_base_phase(this, ON_LOOP_ID);
        first_frame = false;
        boot_pipeline.destroy(vallocator);
    }
    else
    {
        loop_pipeline.progress(this);
    }

    end_defer();
}

void World::shutdown()
{
    loop_pipeline.destroy(vallocator);

    archetype_map.destroy(vallocator);

    for (uint64_t idx = 1; idx < cached_queries.get_count(); idx++)
    {
        uint64_t id = cached_queries.get_dense_array()[idx];
        Query_T *query = cached_queries.get(id);
        query->destroy();
    }
    cached_queries.destroy(vallocator);
    for (auto it = type_infos.begin(); it != type_infos.end(); ++it)
    {
        if(it.is_occupied())
        {
            it.get_val().destroy(vallocator);
        }
    }
    type_infos.destroy(vallocator);
    type_metas.destroy(vallocator);
    type_ids.destroy(vallocator);

    for (uint64_t idx = 1; idx < archetypes.get_count(); idx++)
    {
        uint64_t id = archetypes.get_dense_array()[idx];
        Archetype *archetype = archetypes.get(id);
        ENGINE_ASSERT(archetype, "");
        archetype->destroy(vallocator);
    }
    archetypes.destroy(vallocator);
    entity_records.destroy(vallocator);

    for (uint64_t idx = 0; idx < worker_scratches.get_count(); ++idx)
    {
        worker_scratches[idx].destroy();
    }
    worker_scratches.destroy(vallocator);

    vallocator.destroy();
}

} // namespace VoidEngine::ECS
