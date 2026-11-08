#include "ecs_type.h"
#include "entity.h"
#include "internal_component.h"
#ifdef __clang__
#pragma once
#include "query.h"
#include "world.h"
#endif //__clang__

namespace VoidEngine::ECS
{
///////////////////////////////// Query /////////////////////////////////////

template <typename... CallbackArgs, size_t... I>
void Query_T::invoke_exec_callback(void (*cb)(CallbackArgs...), const QueryIter &iter,
                               MatchedArchetype &matched, uint32_t eidx, std::index_sequence<I...>)
{
    cb(get_exec_callback_arg<CallbackArgs>(iter, matched, I, eidx)...);
}

template <typename... CallbackArgs, size_t... I>
bool Query_T::invoke_filter_callback(bool (*cb)(CallbackArgs...), const QueryIter &iter,
                                 MatchedArchetype &matched, uint32_t eidx, std::index_sequence<I...>)
{
    return cb(get_filter_callback_arg<CallbackArgs>(iter, matched, I, eidx)...);
}

template <typename CallbackArg>
CallbackArg Query_T::get_exec_callback_arg(const QueryIter &iter, MatchedArchetype &matched,
                                      uint32_t sig_idx, uint32_t eidx)
{
    if constexpr (std::is_same_v<CallbackArg, const QueryIter &>)
    {
        return iter;
    }
    else
    {
        const QueryTerm &term = terms[exec_callback.sig_idx_to_term_idx[sig_idx]];

        void *src = nullptr;
        if (term.trav_method == SELF)
        {
            if (term.op == HAS)
            {
                int32_t col_idx = matched.get_column_idx(exec_callback.sig_idx_to_term_idx[sig_idx]);
                ENGINE_ASSERT(col_idx != Archetype::INVALID_COLUMN, "");

                Column &col = matched.archetype->columns[col_idx];
                const TypeMetadata &ti = *col.type_meta;
                src = OFFSET_ADDR(col.data, ti.size * eidx);
            }
            else
            {
                ENGINE_ASSERT(false, "");
            }
        }
        else
        {
            if (term.op == HAS)
            {
                if (term.valid_trav_target == ENTITY_INVALID_ID)
                {
                    ENGINE_ASSERT(false, "");
                }

                // do absolute nothing but just to check if this component has
                // data or not
                int32_t col_idx = matched.get_column_idx(exec_callback.sig_idx_to_term_idx[sig_idx]);
                ENGINE_ASSERT(col_idx != Archetype::INVALID_COLUMN, "");

                src = world->get(term.valid_trav_target, term.tid);
            }
            else
            {
                ENGINE_ASSERT(false, "");
            }
        }

        ENGINE_ASSERT(src, "");
        return *static_cast<std::remove_reference_t<CallbackArg> *>(src);
    }
}

template <typename CallbackArg>
CallbackArg Query_T::get_filter_callback_arg(const QueryIter &iter, MatchedArchetype &matched,
                                        uint32_t sig_idx, uint32_t eidx)
{
    if constexpr (std::is_same_v<CallbackArg, const QueryIter &>)
    {
        return iter;
    }
    else
    {
        const QueryTerm &term = terms[filter_callback.sig_idx_to_term_idx[sig_idx]];

        void *src = nullptr;
        if (term.trav_method == SELF)
        {
            if (term.op == HAS)
            {
                int32_t col_idx = matched.get_column_idx(filter_callback.sig_idx_to_term_idx[sig_idx]);
                ENGINE_ASSERT(col_idx != Archetype::INVALID_COLUMN, "");

                Column &col = matched.archetype->columns[col_idx];
                const TypeMetadata &ti = *col.type_meta;
                src = OFFSET_ADDR(col.data, ti.size * eidx);
            }
            else
            {
                ENGINE_ASSERT(false, "");
            }
        }
        else
        {
            if (term.op == HAS)
            {
                ENGINE_ASSERT(term.valid_trav_target != ENTITY_INVALID_ID, "");

                // do absolute nothing but just to check if this component has
                // data or not
                int32_t col_idx = matched.get_column_idx(filter_callback.sig_idx_to_term_idx[sig_idx]);
                ENGINE_ASSERT(col_idx != Archetype::INVALID_COLUMN, "");

                src = world->get(term.valid_trav_target, term.tid);
            }
            else
            {
                ENGINE_ASSERT(false, "");
            }
        }

        ENGINE_ASSERT(src, "");
        return *static_cast<std::remove_reference_t<CallbackArg> *>(src);
    }
}

template <typename... CallbackArgs>
void Query_T::set_execution_callback(void (*cb)(CallbackArgs...), void *ctx)
{
    // no const or ref or both on entity and query iter
    // static_assert(at_most_one_entity<CallbackArgs...>);
    static_assert(sizeof...(CallbackArgs) >= 1 && sizeof...(CallbackArgs) <= 32,
                  "Callback signatures size must equal or greater than 1");
    static_assert(((sizeof(CallbackArgs) > 1) && ...), "All types must not be a tag!");

    if constexpr (has_one_query_iter_v<CallbackArgs...>)
    {
        static_assert(is_first_arg_query_iter_v<CallbackArgs...>,
                      "Query Iter must be const ref and the first args!");
    }

    exec_callback.sig_count = sizeof...(CallbackArgs);
    exec_callback.ctx = ctx;
    exec_callback.fn = RCAST(void (*)(), cb);
    exec_callback.sig_idx_to_term_idx =
        SCAST(int32_t *,
              world->get_vallocator().valloc(exec_callback.sig_count * sizeof(int32_t), alignof(int32_t)));

    uint32_t termBitmask = 0;

    int64_t callback_sig_ids[64] = {(std::is_same_v<CallbackArgs, const QueryIter &>
                                       ? QUERY_ITER_ID
                                       : SCAST(int64_t, TypeId<std::remove_cvref_t<CallbackArgs>>::id()))...};

    // This will create the sig mapping and validate the callback sig
    for (uint64_t idx = 0; idx < exec_callback.sig_count; ++idx)
    {
        int64_t sig_id = callback_sig_ids[idx];

        if (sig_id == QUERY_ITER_ID)
        {
            exec_callback.sig_idx_to_term_idx[idx] = QUERY_ITER_ID;
            continue;
        }

        for (size_t queryTermIdx = 0; queryTermIdx < term_count; ++queryTermIdx)
        {
            const QueryTerm &term = terms[queryTermIdx];
            bool is_masked = (termBitmask & (1 << queryTermIdx)) != 0;

            if (term.op == NOT || term.access == STRUCTURE_CHANGE)
            {
                continue;
            }

            if (is_masked)
            {
                if (queryTermIdx == term_count - 1)
                {
                    ENGINE_ASSERT(0, "Callback signatures are not valid! Pls check to see if term "
                                  "contains the signature type.");
                }
                continue;
            }

            if (low_32bit(term.tid) == sig_id)
            {
                exec_callback.sig_idx_to_term_idx[idx] = queryTermIdx;
                termBitmask |= (1 << queryTermIdx);
                break;
            }

            if (queryTermIdx == term_count - 1)
            {
                ENGINE_ASSERT(0, "Callback signatures are not valid! Pls check to see if term "
                              "contains the signature type.");
            }
        }
    }

    // map callback sig to query term
    exec_callback.invoker = +[](Query_T *query, void (*fn)(), void *ctx)
    {
        auto callback = RCAST(void (*)(CallbackArgs...), fn);
        for (uint64_t idx = 0; idx < query->result.archetypes.get_count(); ++idx)
        {
            const Archetype *archetype = query->result[idx].archetype;
            QueryIter iter(query->world, 0, ctx, 0);

            // entity level filter
            if (query->is_filtered)
            {
                for (size_t eIdx = 0; eIdx < archetype->count; ++eIdx)
                {
                    bool valid = query->result[idx].get_mask(eIdx);
                    if (valid)
                    {
                        iter.eid = archetype->entities[eIdx];
                        query->invoke_exec_callback<CallbackArgs...>(
                            callback, iter, query->result[idx], eIdx,
                            std::index_sequence_for<CallbackArgs...>{});
                    }
                }
            }
            else
            {
                for (size_t eIdx = 0; eIdx < archetype->count; ++eIdx)
                {
                    iter.eid = archetype->entities[eIdx];
                    // actuallCallback invoke here
                    query->invoke_exec_callback<CallbackArgs...>(
                        callback, iter, query->result[idx], eIdx,
                        std::index_sequence_for<CallbackArgs...>{});
                }
            }
        }
    };
}

template <typename... CallbackArgs>
void Query_T::set_filter_callback(bool (*cb)(CallbackArgs...), void *ctx)
{
    is_filtered = true;

    // no const or ref or both on entity and query iter
    // static_assert(at_most_one_entity<CallbackArgs...>);
    static_assert(sizeof...(CallbackArgs) >= 1 && sizeof...(CallbackArgs) <= 32,
                  "Callback signatures size must equal or greater than 1");
    static_assert(((sizeof(CallbackArgs) > 1) && ...), "All types must have size > 1");
    if constexpr (has_one_query_iter_v<CallbackArgs...>)
    {
        static_assert(is_first_arg_query_iter_v<CallbackArgs...>,
                      "Query Iter must be const ref and the first args!");
    }

    filter_callback.sig_count = sizeof...(CallbackArgs);
    filter_callback.ctx = ctx;
    filter_callback.fn = RCAST(bool (*)(), cb);
    filter_callback.sig_idx_to_term_idx =
        SCAST(int32_t *,
              world->get_vallocator().valloc(filter_callback.sig_count * sizeof(int32_t), alignof(int32_t)));

    uint32_t term_bitmask = 0;
    int64_t callback_sig_ids[64] = {(std::is_same_v<CallbackArgs, const QueryIter &>
                                       ? QUERY_ITER_ID
                                       : SCAST(int64_t, TypeId<std::remove_cvref_t<CallbackArgs>>::id()))...};

    // TODO: This does not take traverse into account yet

    // This will create the sig mapping and validate the callback sig
    for (uint64_t idx = 0; idx < filter_callback.sig_count; ++idx)
    {
        int64_t sig_id = callback_sig_ids[idx];
        if (sig_id == QUERY_ITER_ID)
        {
            exec_callback.sig_idx_to_term_idx[idx] = QUERY_ITER_ID;
            continue;
        }

        for (size_t queryTermIdx = 0; queryTermIdx < term_count; ++queryTermIdx)
        {
            const QueryTerm &term = terms[queryTermIdx];
            bool isMasked = (term_bitmask & (1 << queryTermIdx)) != 0;

            if (term.op == NOT || term.access == STRUCTURE_CHANGE)
            {
                continue;
            }

            if (isMasked)
            {
                if (queryTermIdx == term_count - 1)
                {
                    ENGINE_ASSERT(0, "Callback signatures are not valid! Pls check to see if term "
                                  "contains the signature type.");
                }
                continue;
            }

            if (low_32bit(term.tid) == sig_id)
            {
                filter_callback.sig_idx_to_term_idx[idx] = queryTermIdx;
                term_bitmask |= (1 << queryTermIdx);
                break;
            }

            if (queryTermIdx == term_count - 1)
            {
                ENGINE_ASSERT(0, "Callback signatures are not valid! Pls check to see if term "
                              "contains the signature type.");
            }
        }
    }

    // map callback sig to query term
    filter_callback.invoker =
        +[](Query_T *query, bool (*fn)(), MatchedArchetype &matched_arch, uint32_t eidx, void *ctx)
    {
        auto callback = RCAST(bool (*)(CallbackArgs...), fn);
        QueryIter iter(query->world, 0, ctx, 0);

        iter.eid = matched_arch.archetype->entities[eidx];
        // actuallCallback invoke here
        bool valid = query->invoke_filter_callback(callback, iter, matched_arch, eidx,
                                                 std::index_sequence_for<CallbackArgs...>{});
        matched_arch.set_mask(query->world->get_vallocator(), eidx, valid);
    };
}

////////////////////////// QueryBuilderBase //////////////////////////////

template <typename Handle, typename Derived, typename CallbackBuilder, typename... T>
Derived &QueryBuilderBase<Handle, Derived, CallbackBuilder, T...>::term(EntityId tid)
{
    if (!first_term)
    {
        desc.terms[cur_term_idx++] = cur_term;
        ENGINE_ASSERT(cur_term_idx <= (32 - sizeof...(T)), "Exceed 32 term desc!");
        cur_term = QueryTerm();
    }
    else
    {
        first_term = false;
    }
    cur_term.tid = tid;

    return self();
}

template <typename Handle, typename Derived, typename CallbackBuilder, typename... T>
Derived &QueryBuilderBase<Handle, Derived, CallbackBuilder, T...>::term(EntityId relation_id,
                                                                        EntityId target_eid)
{
    if (!first_term)
    {
        desc.terms[cur_term_idx++] = cur_term;
        ENGINE_ASSERT(cur_term_idx <= (32 - sizeof...(T)), "Exceed 32 term desc!");
        cur_term = QueryTerm();
    }
    else
    {
        first_term = false;
    }
    cur_term.tid = make_relationship(relation_id, target_eid);
    return self();
}

template <typename Handle, typename Derived, typename CallbackBuilder, typename... T>
template <typename U>
Derived &QueryBuilderBase<Handle, Derived, CallbackBuilder, T...>::term()
{
    return term(TypeId<U>::id());
}

template <typename Handle, typename Derived, typename CallbackBuilder, typename... T>
Derived &QueryBuilderBase<Handle, Derived, CallbackBuilder, T...>::access(TermAccess term_access)
{
    ENGINE_ASSERT(term_access != TermAccess::STRUCTURE_CHANGE, "");
    cur_term.access = term_access;
    return self();
}

template <typename Handle, typename Derived, typename CallbackBuilder, typename... T>
Derived &QueryBuilderBase<Handle, Derived, CallbackBuilder, T...>::through(TraverseMethod method)
{
    cur_term.trav_method = method;
    return self();
}

template <typename Handle, typename Derived, typename CallbackBuilder, typename... T>
Derived &QueryBuilderBase<Handle, Derived, CallbackBuilder, T...>::traverse(EntityId relation_id,
                                                                            EntityId target_eid)
{
    cur_term.trav_relation = relation_id;
    cur_term.trav_target = target_eid;
    return self();
}

template <typename Handle, typename Derived, typename CallbackBuilder, typename... T>
Derived &QueryBuilderBase<Handle, Derived, CallbackBuilder, T...>::traverse_any(EntityId relation_id)
{
    return traverse(relation_id, WILDCARD_ID);
}

template <typename Handle, typename Derived, typename CallbackBuilder, typename... T>
template <typename U>
Derived &QueryBuilderBase<Handle, Derived, CallbackBuilder, T...>::traverse(EntityId target_eid)
{
    using C = std::remove_cvref_t<U>;
    static_assert(std::is_same_v<U, C>, "Type cannot be ref or const or both!");
    return traverse(TypeId<U>::id(), target_eid);
}

template <typename Handle, typename Derived, typename CallbackBuilder, typename... T>
template <typename U>
Derived &QueryBuilderBase<Handle, Derived, CallbackBuilder, T...>::traverse_any()
{
    using C = std::remove_cvref_t<U>;
    static_assert(std::is_same_v<U, C>, "Type cannot be ref or const or both!");
    return traverse_any(TypeId<U>::id());
}

template <typename Handle, typename Derived, typename CallbackBuilder, typename... T>
Derived &QueryBuilderBase<Handle, Derived, CallbackBuilder, T...>::op(TermOp op)
{
    cur_term.op = op;
    return self();
}

template <typename Handle, typename Derived, typename CallbackBuilder, typename... T>
template <typename U>
Derived &QueryBuilderBase<Handle, Derived, CallbackBuilder, T...>::with(TermAccess term_access)
{
    return term<U>().op(HAS).access(term_access);
}

template <typename Handle, typename Derived, typename CallbackBuilder, typename... T>
template <typename U>
Derived &QueryBuilderBase<Handle, Derived, CallbackBuilder, T...>::without()
{
    return term<U>().op(NOT);
}

template <typename Handle, typename Derived, typename CallbackBuilder, typename... T>
Derived &QueryBuilderBase<Handle, Derived, CallbackBuilder, T...>::with(EntityId tid, TermAccess term_access)
{
    return term(tid).op(HAS).access(term_access);
}

template <typename Handle, typename Derived, typename CallbackBuilder, typename... T>
Derived &QueryBuilderBase<Handle, Derived, CallbackBuilder, T...>::without(EntityId tid)
{
    return term(tid).op(NOT);
}

template <typename Handle, typename Derived, typename CallbackBuilder, typename... T>
Derived &QueryBuilderBase<Handle, Derived, CallbackBuilder, T...>::with(EntityId relation_id,
                                                                        EntityId target_eid,
                                                                        TermAccess term_access)
{
    return term(relation_id, target_eid).op(HAS).access(term_access);
}

template <typename Handle, typename Derived, typename CallbackBuilder, typename... T>
Derived &QueryBuilderBase<Handle, Derived, CallbackBuilder, T...>::without(EntityId relation_id,
                                                                           EntityId target_eid)
{
    return term(relation_id, target_eid).op(NOT);
}

template <typename Handle, typename Derived, typename CallbackBuilder, typename... T>
template <typename U>
Derived &QueryBuilderBase<Handle, Derived, CallbackBuilder, T...>::up(EntityId target_eid,
                                                                      TermAccess term_access)
{
    using C = std::remove_cvref_t<U>;
    static_assert(std::is_same_v<U, C>, "Type cannot be ref or const or both!");
    return up(TypeId<U>::id(), target_eid).access(term_access);
}

template <typename Handle, typename Derived, typename CallbackBuilder, typename... T>
template <typename U>
Derived &QueryBuilderBase<Handle, Derived, CallbackBuilder, T...>::cascade(TermAccess term_access)
{
    using C = std::remove_cvref_t<U>;
    static_assert(std::is_same_v<U, C>, "Type cannot be ref or const or both!");
    return cascade(TypeId<U>::id()).access(term_access);
}

template <typename Handle, typename Derived, typename CallbackBuilder, typename... T>
Derived &QueryBuilderBase<Handle, Derived, CallbackBuilder, T...>::up(EntityId relation_id,
                                                                      EntityId target_eid,
                                                                      TermAccess term_access)
{
    cur_term.valid_trav_target = target_eid;
    return through(UP).traverse(relation_id, target_eid).access(term_access);
}

template <typename Handle, typename Derived, typename CallbackBuilder, typename... T>
Derived &QueryBuilderBase<Handle, Derived, CallbackBuilder, T...>::cascade(EntityId relation_id,
                                                                           EntityId target_eid,
                                                                           TermAccess term_access)
{
    return through(CASCADE).traverse(relation_id, target_eid).access(term_access);
}

template <typename Handle, typename Derived, typename CallbackBuilder, typename... T>
template <typename U>
Derived &QueryBuilderBase<Handle, Derived, CallbackBuilder, T...>::structure_change()
{
    using C = std::remove_cvref_t<U>;
    static_assert(std::is_same_v<U, C>, "Type cannot be ref or const or both!");
    term<U>();
    cur_term.access = STRUCTURE_CHANGE;
    ++structure_change_count;

    return self();
}

template <typename Handle, typename Derived, typename CallbackBuilder, typename... T>
template <typename... CallbackArgs>
CallbackBuilder
QueryBuilderBase<Handle, Derived, CallbackBuilder, T...>::filter(bool (*cb)(CallbackArgs...),
                                                                 void *ctx)
{
    Query_T *q = create_query();
    return CallbackBuilder(q).filter(cb, ctx);
}

template <typename Handle, typename Derived, typename CallbackBuilder, typename... T>
template <typename... CallbackArgs>
Handle QueryBuilderBase<Handle, Derived, CallbackBuilder, T...>::each(void (*cb)(CallbackArgs...), void *ctx,
                                                                      const char *name)
{
    Query_T *q = create_query();
    return CallbackBuilder(q).each(cb, ctx);
}

template <typename Handle, typename Derived, typename CallbackBuilder, typename... T>
Query_T *QueryBuilderBase<Handle, Derived, CallbackBuilder, T...>::create_query()
{
    desc.terms[cur_term_idx] = cur_term;

    uint32_t query_id = get_query_id();
    Query_T* query = world->create_query(query_id);

    query->structure_change_count = structure_change_count;
    query->term_count = cur_term_idx + 1;
    QueryTerm *terms =
        SCAST(QueryTerm *,
              world->get_vallocator().valloc(query->term_count * sizeof(QueryTerm), alignof(QueryTerm)));
    uint8_t *sorted_term_indices = SCAST(
        uint8_t *, world->get_vallocator().valloc(query->term_count * sizeof(uint8_t), alignof(uint8_t)));

    for (uint32_t idx = 0; idx < query->term_count; ++idx)
    {
        terms[idx] = desc.terms[idx];
        sorted_term_indices[idx] = idx;
    }

    auto priority = [](const QueryTerm &t) -> EntityId
    {
        if (t.access == STRUCTURE_CHANGE)
        {
            return ENTITY_INVALID_ID;
        }
        if (t.op == NOT)
        {
            // This might fighting with EcsName
            return ENTITY_INVALID_ID + 1;
        }
        if (t.trav_method == SELF)
        {
            return t.tid;
        }
        else
        {
            // relationship is used to filter
            // cId component now is just a pass-into-callback reference from
            // traversed-to entity

            return make_relationship(t.trav_relation, t.trav_target);
        }
    };

    // sort descending
    // relationship will at the front because of their narrow set

    std::sort(sorted_term_indices, sorted_term_indices + query->term_count,
              [&](uint8_t a, uint8_t b) { return priority(terms[a]) > priority(terms[b]); });

    query->terms = terms;
    query->is_filtered = false;
    query->sorted_term_indices = sorted_term_indices;

    return query;
}

////////////////////////// QueryBuilder /////////////////////////////

template <typename... T>
QueryCallbackBuilder QueryBuilder<T...>::cache(EntityId eId)
{
    Query_T *q =
        QueryBuilderBase<QueryHandle, QueryBuilder<T...>, QueryCallbackBuilder, T...>::create_query();
    return QueryCallbackBuilder(q).cache();
}

////////////////////////// SystemBuilder /////////////////////////////

template <typename... T>
SystemCallbackBuilder SystemBuilder<T...>::phase(EntityId eId)
{
    Query_T *q =
        QueryBuilderBase<SystemHandle, SystemBuilder<T...>, SystemCallbackBuilder, T...>::create_query();
    return SystemCallbackBuilder(q).phase(eId);
}

//////////////////// QueryCallbackBuilderBase ///////////////////////

template <typename Derived, typename Handle>
template <typename... CallbackArgs>
QueryCallBackBuilderBase<Derived, Handle> &
QueryCallBackBuilderBase<Derived, Handle>::filter(bool (*cb)(CallbackArgs...), void *ctx)
{
    query->set_filter_callback(cb, ctx);
    return *this;
}

template <typename Derived, typename Handle>
template <typename... CallbackArgs>
Handle QueryCallBackBuilderBase<Derived, Handle>::each(void (*cb)(CallbackArgs...), void *ctx,
                                                       const char *name)
{
    query->set_execution_callback(cb, ctx);

    Derived &self = *SCAST(Derived *, this);
    if(query->is_cached)
    {
        self.create_cache_entity(name);
        query->query_archetypes();

        if (query->is_filtered)
        {
            query->filter_all();
        }
        query->is_result_available = true;
    }
    return Handle(query);
}

} // namespace VoidEngine::ECS
