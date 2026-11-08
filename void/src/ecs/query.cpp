#include "ecs/query.h"
#include "ecs/ecs_type.h"
#include "ecs/internal_component.h"
#include "ecs/world.h"

namespace VoidEngine::ECS
{

///////////////////////// QueryArchetype ////////////////////////////

void MatchedArchetype::set_term_idx_to_col_idx(Vallocator &vallocator, int32_t *mapped_cols, uint32_t count)
{
    int32_t count_diff = count - INLINE_ARR_COUNT;
    std::memcpy(lo_term_idx_to_col_idx, mapped_cols,
                ((count <= INLINE_ARR_COUNT) ? count : INLINE_ARR_COUNT) * sizeof(int32_t));
    if (count_diff > 0)
    {
        ENGINE_ASSERT(!hi_term_idx_to_col_idx, "High term idx to col idx is already set!");
        hi_term_idx_to_col_idx = RCAST(int32_t *, vallocator.valloc(count_diff * sizeof(int32_t), alignof(int32_t)));
        ENGINE_ASSERT(hi_term_idx_to_col_idx, "Failed to allocate!");
        std::memcpy(hi_term_idx_to_col_idx, mapped_cols + INLINE_ARR_COUNT,
                    count_diff * sizeof(int32_t));
    }
    mapped_count = count;
}

int32_t MatchedArchetype::get_column_idx(uint32_t term_idx) const
{
    if (term_idx < INLINE_ARR_COUNT)
    {
        return lo_term_idx_to_col_idx[term_idx];
    }
    else
    {
        return hi_term_idx_to_col_idx[term_idx - INLINE_ARR_COUNT];
    }
}

void MatchedArchetype::destroy(Vallocator &vallocator)
{
    if (hi_term_idx_to_col_idx)
    {
        vallocator.vfree((mapped_count - INLINE_ARR_COUNT) * sizeof(int32_t), hi_term_idx_to_col_idx);
    }
    if (hi_entity_mask)
    {
        vallocator.vfree(hi_mask_capacity * sizeof(uint32_t), hi_entity_mask);
    }
}

void MatchedArchetype::allocate_mask(Vallocator &vallocator)
{
    uint32_t cur_mask_count = std::ceil(SCAST(float, archetype->count) / SCAST(float, EntityPerMask));
    if (cur_mask_count > INLINE_ARR_COUNT)
    {
        uint32_t new_capacity = cur_mask_count - INLINE_ARR_COUNT;
        new_capacity = new_capacity > 4 ? new_capacity : 4;

        if (hi_mask_capacity < new_capacity)
        {
            uint32_t *new_masks = RCAST(uint32_t *, vallocator.valloc(new_capacity * sizeof(uint32_t), alignof(uint32_t)));
            ENGINE_ASSERT(new_masks, "Failed to allocate high mask!");

            if (hi_entity_mask)
            {
                std::memcpy(new_masks, hi_entity_mask, hi_mask_capacity * sizeof(uint32_t));
                vallocator.vfree(hi_mask_capacity * sizeof(uint32_t), hi_entity_mask);
            }
            hi_entity_mask = new_masks;
            hi_mask_capacity = new_capacity;
        }
    }
}

uint32_t &MatchedArchetype::mask(uint32_t eidx)
{
    uint32_t mask_count = std::ceil(SCAST(float, archetype->count) / SCAST(float, EntityPerMask));
    uint32_t mask_idx = SCAST(float, eidx) / SCAST(float, EntityPerMask);
    ENGINE_ASSERT(mask_idx < mask_count || eidx < archetype->count, "Entity index or mask index is out of bound!");

    if (mask_idx < INLINE_ARR_COUNT)
    {
        return lo_entity_mask[mask_idx];
    }
    else
    {
        ENGINE_ASSERT(mask_idx - INLINE_ARR_COUNT < hi_mask_capacity,
                   "Mask index is out of bound!");
        return hi_entity_mask[mask_idx - INLINE_ARR_COUNT];
    }
}

void MatchedArchetype::set_mask(Vallocator &vallocator, uint32_t eidx, bool bit)
{
    allocate_mask(vallocator);
    uint32_t &m = mask(eidx);
    uint32_t idx = eidx % EntityPerMask;

    if (bit)
    {
        m |= (1 << idx);
    }
    else
    {
        m &= ~(1 << idx);
    }
}

bool MatchedArchetype::get_mask(uint32_t eIdx)
{
    uint32_t m = mask(eIdx);
    uint32_t idx = eIdx % EntityPerMask;

    return (m & (1 << idx)) > 0;
}

//////////////////////////// QueryResult ////////////////////////////////

void QueryResult::init(Vallocator& vallocator)
{
    archetypes.init(vallocator, 8);
}

MatchedArchetype &QueryResult::add(Vallocator &vallocator, const Archetype *archetype)
{
    archetypes.emplace_back(vallocator, archetype);
    return archetypes[archetypes.get_count() - 1];
}

void QueryResult::clear(Vallocator &vallocator)
{
    for(uint64_t idx = 0; idx < archetypes.get_count(); ++idx)
    {
        archetypes[idx].destroy(vallocator);
        archetypes[idx].~MatchedArchetype();
    }
}

void QueryResult::destroy(Vallocator &vallocator)
{
    clear(vallocator);
    archetypes.destroy(vallocator);
}

/////////////////////////////// Query /////////////////////////////////

void Query_T::init(World *world, EntityId eid)
{
    ENGINE_ASSERT(world, "World must not be null!");
    this->world = world;
    this->eid = eid;
    result.init(world->get_vallocator());
}

void Query_T::execute()
{
    if (!is_cached || !is_result_available)
    {
        clear_result();
        query_archetypes();
        if (is_filtered)
        {
            filter_all();
        }
        is_result_available = true;
    }
    exec_callback.invoker(this, exec_callback.fn, exec_callback.ctx);
}

void Query_T::filter_all()
{
    for (uint64_t idx = 0; idx < result.archetypes.get_count(); ++idx)
    {
        for (uint64_t eidx = 0; eidx < result[idx].archetype->count; ++eidx)
        {
            filter_callback.invoker(this, filter_callback.fn, result[idx], eidx,
                                         filter_callback.ctx);
        }
    }
}

void Query_T::filter_archetype(MatchedArchetype &marched_archetype)
{
    for (uint64_t eidx = 0; eidx < marched_archetype.archetype->count; ++eidx)
    {
        filter_callback.invoker(this, filter_callback.fn, marched_archetype, eidx,
                                     filter_callback.ctx);
    }
}

void Query_T::filter_entity(MatchedArchetype &marched_archetype, uint32_t eidx)
{

    filter_callback.invoker(this, filter_callback.fn, marched_archetype, eidx,
                                 filter_callback.ctx);
}

// void Query_T::Move(Query_T &&other)
// {
//     eId = other.eId;
//     world = other.world;
//     terms = other.terms;
//     result.Move(world->GetWorldAllocator(), std::move(other.result));
//     execCallback = other.execCallback;
//     termCount = other.termCount;
//     isEntityFiltered = other.isEntityFiltered;
//     sortedTermIdx = other.sortedTermIdx;
//     lastSweep_archetype = other.lastSweep_archetype;
//     entityFilterCallback = other.entityFilterCallback;
//     is_cached = other.is_cached;
//     structureChangeCount = other.structureChangeCount;
//
//     other.terms = nullptr;
//     other.sortedTermIdx = nullptr;
// }

void Query_T::query_archetypes()
{
    ENGINE_ASSERT(term_count > 0, "Term Count must be greater than 0!");
    QueryResult result;

    const QueryTerm &anchor_term = terms[sorted_term_indices[0]];
    ENGINE_ASSERT(anchor_term.op != NOT, "Query should not rely on only NOT operations!");

    EntityId anchor_term_tid = ENTITY_INVALID_ID;

    if (anchor_term.trav_method == SELF)
    {
        anchor_term_tid = anchor_term.tid;
    }
    else
    {
        anchor_term_tid = make_relationship(
            anchor_term.trav_relation, anchor_term.trav_target == WILDCARD_ID ? 0 : anchor_term.trav_target);
    }

    const TypeInfo &anchor_ti = world->get_type_infos()[anchor_term_tid];

    // anchor term work as a point to narrow down archetype list
    for (uint64_t idx = 0; idx < anchor_ti.archetypes.get_count(); ++idx)
    {
        Archetype *archetype = anchor_ti.archetypes[idx];
        add_if_match(archetype);
    }
}

void Query_T::add_if_match(Archetype *archetype)
{
    int32_t *term_idx_to_col_idx =
        SCAST(int32_t *, world->get_vallocator().valloc(term_count * sizeof(int32_t), alignof(int32_t)));
    ENGINE_ASSERT(term_idx_to_col_idx, "Failed to allocate!");

    const TypeSet &type_set = archetype->type_set;
    bool is_valid = true;

    for (uint64_t term_idx = 0; term_idx < term_count - structure_change_count; ++term_idx)
    {
        QueryTerm &term = terms[sorted_term_indices[term_idx]];
        ENGINE_ASSERT(term.access != STRUCTURE_CHANGE, "Terms should never contain only Modify");

        switch (term.trav_method)
        {
        case ECS::SELF:
        {
            if (term.op == HAS)
            {
                int32_t tidx = type_set.search(term.tid);
                if (tidx == TypeSet::NOT_FOUND_IDX)
                {
                    is_valid = false;
                }
                else
                {
                    term_idx_to_col_idx[sorted_term_indices[term_idx]] = archetype->column_map[tidx];
                }
            }
            else if (term.op == NOT)
            {
                if (type_set.has(term.tid))
                {
                    is_valid = false;
                }
                else
                {
                    term_idx_to_col_idx[sorted_term_indices[term_idx]] = TypeSet::NOT_FOUND_IDX;
                }
            }
            else
            {
                ENGINE_ASSERT(false, "");
            }
            break;
        }
        case ECS::UP:
        {
            EntityId relationship_id = make_relationship(term.trav_relation, term.trav_target);
            int32_t tidx = type_set.search(relationship_id);
            if (tidx == TypeSet::NOT_FOUND_IDX)
            {
                is_valid = false;
                break;
            }

            EntityId target = term.trav_target;
            target = high_32bit(type_set[tidx]);

            const Archetype *target_archetype = world->get_entity_archetype(target);

            if (term.op == HAS)
            {
                int32_t tidx = target_archetype->type_set.search(term.tid);

                if (tidx == TypeSet::NOT_FOUND_IDX)
                {
                    is_valid = false;
                }
                else
                {
                    term.valid_trav_target = target;
                    term_idx_to_col_idx[sorted_term_indices[term_idx]] =
                        target_archetype->column_map[tidx];
                }
            }
            else if (term.op == NOT)
            {
                if (world->has_type(target, term.tid))
                {
                    is_valid = false;
                }
                else
                {
                    term_idx_to_col_idx[sorted_term_indices[term_idx]] = TypeSet::NOT_FOUND_IDX;
                }
            }
            else
            {
                ENGINE_ASSERT(false, "");
            }
            break;
        }
        case ECS::CASCADE:
        {
            EntityId relationship_id = make_relationship(term.trav_relation, term.trav_target);
            int32_t tidx = type_set.search(relationship_id);
            if (tidx == TypeSet::NOT_FOUND_IDX)
            {
                is_valid = false;
                break;
            }

            EntityId target = term.trav_target;
            target = high_32bit(type_set[tidx]);

            const Archetype *target_archetype = world->get_entity_archetype(target);
            if (term.op == HAS)
            {
                while (true)
                {
                    int32_t cascaded_tidx = target_archetype->type_set.search(term.tid);

                    if (cascaded_tidx == TypeSet::NOT_FOUND_IDX)
                    {
                        int32_t target_relationship_idx =
                            target_archetype->type_set.search(relationship_id);

                        if (target_relationship_idx == TypeSet::NOT_FOUND_IDX)
                        {
                            is_valid = false;
                            break;
                        }
                        else
                        {
                            target = high_32bit(target_archetype->type_set[target_relationship_idx]);
                            target_archetype = world->get_entity_archetype(target);
                        }
                    }
                    else
                    {
                        term.valid_trav_target = target;
                        term_idx_to_col_idx[sorted_term_indices[term_idx]] =
                            target_archetype->column_map[cascaded_tidx];
                        break;
                    }
                }
            }
            else if (term.op == NOT)
            {
                while (true)
                {
                    if (world->has_type(target, term.tid))
                    {
                        int32_t target_relationship_idx =
                            target_archetype->type_set.search(relationship_id);

                        if (target_relationship_idx == TypeSet::NOT_FOUND_IDX)
                        {
                            is_valid = false;
                            break;
                        }
                        else
                        {
                            target = high_32bit(target_archetype->type_set[target_relationship_idx]);
                            target_archetype = world->get_entity_archetype(target);
                        }
                    }
                    else
                    {
                        // valid
                        break;
                    }
                }
            }
            else
            {
                ENGINE_ASSERT(false, "");
            }

            break;
        }
        default:
        {
            ENGINE_ASSERT(false, "Unknown term traverse method!");
        }
        }

        if (!is_valid)
        {
            break;
        }
    }

    if (is_valid)
    {
        if (eid != ENTITY_INVALID_ID)
        {
            archetype->tracked_queries.push_back(
                world->get_vallocator(),
                TrackedQuery{low_32bit(eid), SCAST(uint32_t, result.archetypes.get_count())});
        }

        MatchedArchetype &ma = result.add(world->get_vallocator(), archetype);
        ma.set_term_idx_to_col_idx(world->get_vallocator(), term_idx_to_col_idx, term_count);
    }
    world->get_vallocator().vfree(sizeof(int32_t) * term_count, term_idx_to_col_idx);
}

void Query_T::clear_result()
{
    result.clear(world->get_vallocator());
}

void Query_T::destroy()
{
    ENGINE_ASSERT(world, "World is null!");
    ENGINE_ASSERT(terms, "Terms are empty!");

    result.destroy(world->get_vallocator());
    if (is_cached)
    {
        world->remove_entity(eid);
    }
    world->get_vallocator().vfree(sizeof(QueryTerm) * term_count, terms);
    world->get_vallocator().vfree(sizeof(int32_t) * term_count, sorted_term_indices);
    world->get_vallocator().vfree(sizeof(int32_t) * exec_callback.sig_count,
                                  exec_callback.sig_idx_to_term_idx);
    world->get_vallocator().vfree(sizeof(int32_t) * filter_callback.sig_count,
                                  filter_callback.sig_idx_to_term_idx);
}

///////////////////////// QueryHandle /////////////////////////

void QueryHandle::execute()
{
    ENGINE_ASSERT(query, "Query must not be null!");
    query->execute();
}

void QueryHandle::destroy()
{
    ENGINE_ASSERT(query, "Query must not be null!");
    query->destroy();
    query = nullptr;
}

/////////////////////////// QueryCallbackBuilder /////////////////////////

void QueryCallbackBuilder::create_cache_entity(const char *name)
{
    ENGINE_ASSERT(query->is_cached, "Query must be cached!");
    Entity e = query->world
                   ->entity_builder()
                   // .id(query->eid)
                   .name(name)
                   .assign_component<Query>(Query{query})
                   .build();
    query->eid = e.get_eid();
    query->world->update_type_info_tracked_queries(query);
}

/////////////////////////// QueryCallbackBuilder /////////////////////////

void SystemCallbackBuilder::create_cache_entity(const char *name)
{
    ENGINE_ASSERT(query->is_cached, "System must be cached!");
    auto &builder = query->world
                        ->entity_builder()
                        // .id(query->eid)
                        .name(name)
                        .assign_component<System>(System{query});

    bool has_phase = query->world->has_type(phase_id_, PHASE_ID);
    bool is_depended_on = query->world->has_type(phase_id_, make_relationship(DEPEND_ON_ID, WILDCARD_ID));

    if (has_phase && is_depended_on)
    {
        builder.add_relationship<DependOn>(phase_id_);
    }

    Entity e = builder.build();
    query->eid = e.get_eid();
    query->world->update_type_info_tracked_queries(query);
}

} // namespace VoidEngine::ECS
