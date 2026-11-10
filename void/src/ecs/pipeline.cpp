#include "ecs/pipeline.h"
#include "ecs/ecs_type.h"
#include "ecs/internal_component.h"
#include "ecs/query.h"
#include "ecs/type_info.h"
#include "ecs/world.h"
#include "job_system.h"

namespace VoidEngine::ECS
{

void Pipeline::build_from_base_phase(World *world, EntityId phase_id)
{
    ENGINE_ASSERT(world, "World must not be null!");
    ENGINE_ASSERT(world->has_type(phase_id, PHASE_ID) &&
                      !world->has_type(phase_id, make_relationship(DEPEND_ON_ID, WILDCARD_ID)),
                  "Entity is not base phase! Base phase has no DependOn "
                  "relationship and have EcsPhase Tag");
    FlatHashMap<EntityId, TermAccess> accesses;
    accesses.init(world->get_vallocator(), 64);

    EntityId target_eid = phase_id;
    EntityId relationship_id = make_relationship(DEPEND_ON_ID, target_eid);
    bool loop = true;
    while (world->is_type_registerd(relationship_id) && loop)
    {
        loop = false;
        const TypeInfo &type_info = world->get_type_infos()[relationship_id];

        Stage stage;
        stage.init(world->get_vallocator(), 16);
        for (uint64_t idx = 0; idx < type_info.archetypes.get_count(); ++idx)
        {
            const Archetype *archetype = type_info.archetypes[idx];
            int32_t tidx = archetype->type_set.search(SYSTEM_ID);

            if (tidx != TypeSet::NOT_FOUND_IDX)
            {
                // EcsDependOn EcsSystem (no EcsPhase) -> normal system entity
                int32_t col_idx = archetype->column_map[tidx];
                ENGINE_ASSERT(col_idx != Archetype::INVALID_COLUMN, "");
                Column &col = archetype->columns[col_idx];
                const TypeMetadata *meta = col.type_meta;

                for (uint64_t eidx = 0; eidx < archetype->count; ++eidx)
                {
                    System *system = SCAST(System *, OFFSET_ADDR(col.data, meta->size * eidx));
                    build_step(world, accesses, system, stage);
                }
            }
            else
            {
                // EcsPhase EcsDependOn [id] -> find next phase
                // base phase where it does not depend on anyone
                ENGINE_ASSERT(archetype->type_set.has(PHASE_ID),
                              "Invalid archetype for pipeline builder!");
                loop = true;
                target_eid = archetype->entities[0];
                relationship_id = make_relationship(DEPEND_ON_ID, target_eid);
                continue;
            }
        }
        stages_.push_back(world->get_vallocator(), std::move(stage));
    }
    accesses.destroy(world->get_vallocator());
}

void Pipeline::build_step(World *world, FlatHashMap<EntityId, TermAccess> &accesses, System *system,
                          Stage &appending_stage)
{
    ENGINE_ASSERT(system, "System is null!");
    auto need_sync = [&accesses, system, world]() -> bool
    {
        Query_T &query = *system->query;
        for (uint64_t idx = 0; idx < query.term_count; ++idx)
        {
            QueryTerm &term = query.terms[idx];

            // if (term.trav_method != TraverseMethod::SELF)
            // {
            //     accesses.insert(world->get_vallocator(),
            //                     make_relationship(term.trav_relation, term.trav_target),
            //                     TermAccess::READ);
            // }

            if (term.access == TermAccess::READ)
            {
                if (accesses.contain_key(term.tid) &&
                    (accesses[term.tid] == TermAccess::WRITE ||
                     accesses[term.tid] == TermAccess::STRUCTURE_CHANGE))
                {
                    return true;
                }
            }
            else if (term.access == TermAccess::WRITE)
            {
                if (accesses.contain_key(term.tid))
                {
                    return true;
                }
            }
            else
            {
                if (accesses.contain_key(term.tid))
                {
                    return true;
                }
            }
        }
        return false;
    };

    auto monitor = [&accesses, system, world]()
    {
        Query_T &query = *system->query;
        for (uint64_t idx = 0; idx < query.term_count; ++idx)
        {
            accesses.insert(world->get_vallocator(), query.terms[idx].tid, query.terms[idx].access);
        }
    };

    if (need_sync())
    {
        // add sync point
        appending_stage.push_back(world->get_vallocator(), Step{system->query, true});
        accesses.clear();
    }
    else
    {
        appending_stage.push_back(world->get_vallocator(), Step{system->query, false});
        monitor();
    }
}

void Pipeline::progress(World *world)
{
    if (!world->is_multithread())
    {
        for (uint64_t idx = 0; idx < stages_.get_count(); ++idx)
        {
            Stage &stage = stages_[idx];
            for (uint64_t sidx = 0; sidx < stage.get_count(); ++sidx)
            {
                Step &step = stage[sidx];
                if (step.sync_point)
                {
                    world->flush_deferred_cmds();
                }
                step.system->execute();
            }
        }
    }
    else
    {
        for (uint64_t idx = 0; idx < stages_.get_count(); ++idx)
        {
            Stage &stage = stages_[idx];
            EcsBatch *batch = world->get_system_batch();
            uint64_t current_step = 0;

            while (current_step < stage.get_count())
            {
                uint32_t count = 0;
                uint32_t sync_point = 0;
                for (; current_step < stage.get_count(); ++current_step)
                {
                    Step &step = stage[current_step];
                    if (step.sync_point)
                    {
                        if (current_step == sync_point)
                        {
                            world->flush_deferred_cmds();
                        }
                        else
                        {
                            sync_point = current_step;
                            break;
                        }
                    }
                    batch->systems[count++] = step.system;
                }
                batch->count = count;
                batch->cursor.store(0, std::memory_order_relaxed);
                batch->remaining.store(count, std::memory_order_release);
                uint32_t worker_dispatch = std::min(count - 1, world->get_worker_count());

                for (uint64_t worker = 0; worker < worker_dispatch; ++worker)
                {
                    Job job;
                    job.fn_ = &World::run_system;
                    job.context_ = world;
                    job.ex_data_ = batch;
                    JobSystem::get().schedule(job, JobPriority::HIGH);
                }

                World::run_system_main(world, batch);

                while (batch->remaining.load(std::memory_order_acquire) != 0)
                {
                    //std::cout << "Waiting for worker to finish" << std::endl;
                }
            }
        }
    }
}

void Pipeline::init(Vallocator &vallocator)
{
    stages_.init(vallocator, 16);
}

void Pipeline::destroy(Vallocator &vallocator)
{
    for (uint64_t idx = 0; idx < stages_.get_count(); ++idx)
    {
        Stage &stage = stages_[idx];
        stage.destroy(vallocator);
    }
    stages_.destroy(vallocator);
}

///////////////// PhaseDependencyBuilder //////////////////

PhaseDependencyBuilder::PhaseDependencyBuilder(World *world, EntityId phase_id)
    : phase_id(phase_id), active_phase_id(phase_id), world(world)
{
    ENGINE_ASSERT(world->has_type(phase_id, PHASE_ID) &&
                      !world->has_type(phase_id, make_relationship(DEPEND_ON_ID, WILDCARD_ID)),
                  "Entity is not base phase! Base phase has no DependOn "
                  "relationship and have EcsPhase Tag");
}

PhaseDependencyBuilder &PhaseDependencyBuilder::depend_on(EntityId eid, const char *name)
{
    ENGINE_ASSERT(eid != ENTITY_INVALID_ID, "Entity id must not be invalid!");
    ENGINE_ASSERT(!world->is_entity_existing(eid), "Entity id already exists!");
    Entity e = world->create_entity(eid, name)
                   .add_tag<Phase>()
                   .add_relationship<DependOn>(active_phase_id)
                   .build();
    active_phase_id = e.get_eid();
    return *this;
}

} // namespace VoidEngine::ECS
