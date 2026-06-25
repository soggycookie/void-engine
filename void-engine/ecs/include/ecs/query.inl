#include "ecs_type.h"
#include "ecs_utils.h"
#include "entity.h"
#include "internal_component.h"
#include <cstdint>
#include <utility>
#include <vcruntime_typeinfo.h>
#ifdef __clang__
#pragma once
#include "query.h"
#include "world.h"
#endif //__clang__

namespace ECS
{
///////////////////////////////// Query /////////////////////////////////////

template <typename... CallbackArgs, size_t... I>
void Query::InvokeExecCallback(void (*cb)(CallbackArgs...),
                               const QueryIter &iter, QueryArchetype &matched,
                               uint32_t eIdx, std::index_sequence<I...>)
{
    cb(GetExecCallbackArg<CallbackArgs>(iter, matched, I, eIdx)...);
}

template <typename... CallbackArgs, size_t... I>
bool Query::InvokeFilterCallback(bool (*cb)(CallbackArgs...),
                                 const QueryIter &iter, QueryArchetype &matched,
                                 uint32_t eIdx, std::index_sequence<I...>)
{
    return cb(GetFilterCallbackArg<CallbackArgs>(iter, matched, I, eIdx)...);
}

template <typename CallbackArg>
CallbackArg Query::GetExecCallbackArg(const QueryIter &iter,
                                      QueryArchetype &matched, uint32_t sigIdx,
                                      uint32_t eIdx)
{
    if constexpr (std::is_same_v<CallbackArg, const QueryIter &>)
    {
        return iter;
    }
    else
    {
        const QueryTerm &term = terms[execCallback.sigIdxToTermIdx[sigIdx]];

        void *src = nullptr;
        if (term.travMethod == SELF)
        {
            if (term.op == HAS)
            {
                int32_t colIdx =
                    matched.GetColumnIdx(execCallback.sigIdxToTermIdx[sigIdx]);
                assert(colIdx != -1);

                Column &col = matched.archetype->columns[colIdx];
                TypeInfo &ti = *col.typeInfo;
                src = OFFSET_ELEMENT(col.data, ti.size, eIdx);
            }
            else
            {
                assert(0);
            }
        }
        else
        {
            if (term.op == HAS)
            {
                if (term.validTravTarget == EcsInvalidId)
                {
                    assert(0);
                }

                // do absolute nothing but just to check if this component has
                // data or not
                int32_t colIdx =
                    matched.GetColumnIdx(execCallback.sigIdxToTermIdx[sigIdx]);
                assert(colIdx != -1);

                src = world->Get(term.validTravTarget, term.cId);
            }
            else
            {
                assert(0);
            }
        }

        assert(src);
        return *static_cast<std::remove_reference_t<CallbackArg> *>(src);
    }
}

template <typename CallbackArg>
CallbackArg Query::GetFilterCallbackArg(const QueryIter &iter,
                                        QueryArchetype &matched,
                                        uint32_t sigIdx, uint32_t eIdx)
{
    if constexpr (std::is_same_v<CallbackArg, const QueryIter &>)
    {
        return iter;
    }
    else
    {
        const QueryTerm &term =
            terms[entityFilterCallback.sigIdxToTermIdx[sigIdx]];

        void *src = nullptr;
        if (term.travMethod == SELF)
        {
            if (term.op == HAS)
            {
                int32_t colIdx = matched.GetColumnIdx(
                    entityFilterCallback.sigIdxToTermIdx[sigIdx]);
                assert(colIdx != -1);

                Column &col = matched.archetype->columns[colIdx];
                TypeInfo &ti = *col.typeInfo;
                src = OFFSET_ELEMENT(col.data, ti.size, eIdx);
            }
            else
            {
                assert(0);
            }
        }
        else
        {
            if (term.op == HAS)
            {
                if (term.validTravTarget == EcsInvalidId)
                {
                    assert(0);
                }

                // do absolute nothing but just to check if this component has
                // data or not
                int32_t colIdx = matched.GetColumnIdx(
                    entityFilterCallback.sigIdxToTermIdx[sigIdx]);
                assert(colIdx != -1);

                src = world->Get(term.validTravTarget, term.cId);
            }
            else
            {
                assert(0);
            }
        }

        assert(src);
        return *static_cast<std::remove_reference_t<CallbackArg> *>(src);
    }
}

template <typename... CallbackArgs>
void Query::SetExecutionCallback(void (*cb)(CallbackArgs...), void *ctx)
{
    // no const or ref or both on entity and query iter
    // static_assert(at_most_one_entity<CallbackArgs...>);
    static_assert(sizeof...(CallbackArgs) >= 1 && sizeof...(CallbackArgs) <= 32,
                  "Callback signatures size must equal or greater than 1");
    static_assert(((sizeof(CallbackArgs) > 1) && ...),
                  "All types must not be a tag!");

    if constexpr (has_one_query_iter_v<CallbackArgs...>)
    {
        static_assert(is_first_arg_query_iter_v<CallbackArgs...>,
                      "Query Iter must be const ref and the first args!");
    }

    execCallback.sigCount = sizeof...(CallbackArgs);
    execCallback.ctx = ctx;
    execCallback.fn = RCAST(cb, void (*)());
    execCallback.sigIdxToTermIdx =
        PTR_CAST(world->m_wAllocator.Alloc(execCallback.sigCount), int32_t);

    uint32_t termBitmask = 0;

    int64_t callbackSig[] = {
        (std::is_same_v<CallbackArgs, const QueryIter &>
             ? QueryIterIndex
             : ComponentTypeId<std::decay_t<CallbackArgs>>::Id())...};

    // This will create the sig mapping and validate the callback sig
    for (size_t idx = 0; idx < execCallback.sigCount; ++idx)
    {
        int64_t callbackSigId = callbackSig[idx];

        if (callbackSigId == QueryIterIndex)
        {
            execCallback.sigIdxToTermIdx[idx] = QueryIterIndex;
            continue;
        }

        for (size_t queryTermIdx = 0; queryTermIdx < termCount; ++queryTermIdx)
        {
            const QueryTerm &term = terms[queryTermIdx];
            bool isMasked = (termBitmask & (1 << queryTermIdx)) != 0;

            if (term.op == NOT || term.behavior == STRUCTURE_CHANGE || isMasked)
            {
                continue;
            }

            if (LO_ENTITY_ID(term.cId) == callbackSigId)
            {
                execCallback.sigIdxToTermIdx[idx] = queryTermIdx;
                termBitmask |= (1 << queryTermIdx);
                break;
            }

            if (queryTermIdx == termCount - 1)
            {
                assert(0 && "Callback signatures are not valid!");
            }
        }
    }

    // map callback sig to query term
    execCallback.invoker = +[](Query *query, void (*fn)(), void *ctx)
    {
        auto actualCallback = RCAST(fn, void (*)(CallbackArgs...));

        for (size_t idx = 0; idx < query->result.count; ++idx)
        {
            const Archetype *archetype = query->result[idx].archetype;

            QueryIter iter(query->world, 0, ctx, 0);

            // entity level filter
            if (query->isEntityFiltered)
            {
                for (size_t eIdx = 0; eIdx < archetype->count; ++eIdx)
                {
                    bool valid = query->result[idx].GetMask(eIdx);
                    if (valid)
                    {
                        iter.eId = archetype->entities[eIdx];
                        query->InvokeExecCallback<CallbackArgs...>(
                            actualCallback, iter, query->result[idx], eIdx,
                            std::index_sequence_for<CallbackArgs...>{});
                    }
                }
            }
            else
            {
                for (size_t eIdx = 0; eIdx < archetype->count; ++eIdx)
                {
                    iter.eId = archetype->entities[eIdx];
                    // actuallCallback invoke here
                    query->InvokeExecCallback<CallbackArgs...>(
                        actualCallback, iter, query->result[idx], eIdx,
                        std::index_sequence_for<CallbackArgs...>{});
                }
            }
        }
    };
}

template <typename... CallbackArgs>
void Query::SetFilterCallback(bool (*cb)(CallbackArgs...), void *ctx)
{
    isEntityFiltered = true;

    // no const or ref or both on entity and query iter
    // static_assert(at_most_one_entity<CallbackArgs...>);
    static_assert(sizeof...(CallbackArgs) >= 1 && sizeof...(CallbackArgs) <= 32,
                  "Callback signatures size must equal or greater than 1");
    static_assert(((sizeof(CallbackArgs) > 1) && ...),
                  "All types must have size > 1");
    if constexpr (has_one_query_iter_v<CallbackArgs...>)
    {
        static_assert(is_first_arg_query_iter_v<CallbackArgs...>,
                      "Query Iter must be const ref and the first args!");
    }

    entityFilterCallback.sigCount = sizeof...(CallbackArgs);
    entityFilterCallback.ctx = ctx;
    entityFilterCallback.fn = RCAST(cb, bool (*)());
    entityFilterCallback.sigIdxToTermIdx =
        PTR_CAST(world->m_wAllocator.Alloc(execCallback.sigCount), int32_t);

    uint32_t termBitmask = 0;

    int64_t callbackSig[] = {
        (std::is_same_v<CallbackArgs, const QueryIter &>
             ? QueryIterIndex
             : ComponentTypeId<std::decay_t<CallbackArgs>>::Id())...};

    // TODO: This does not take traverse into account yet

    // This will create the sig mapping and validate the callback sig
    for (size_t idx = 0; idx < entityFilterCallback.sigCount; ++idx)
    {
        int64_t callbackSigId = callbackSig[idx];

        if (callbackSigId == QueryIterIndex)
        {
            execCallback.sigIdxToTermIdx[idx] = QueryIterIndex;
            continue;
        }

        for (size_t queryTermIdx = 0; queryTermIdx < termCount; ++queryTermIdx)
        {
            const QueryTerm &term = terms[queryTermIdx];
            bool isMasked = (termBitmask & (1 << queryTermIdx)) != 0;

            if (term.op == NOT || term.behavior == STRUCTURE_CHANGE || isMasked)
            {
                continue;
            }

            if (LO_ENTITY_ID(term.cId) == callbackSigId)
            {
                entityFilterCallback.sigIdxToTermIdx[idx] = queryTermIdx;
                termBitmask |= (1 << queryTermIdx);
                break;
            }

            if (queryTermIdx == termCount - 1)
            {
                assert(0 && "Callback signatures are not valid!");
            }
        }
    }

    // map callback sig to query term
    entityFilterCallback.invoker =
        +[](Query *query, bool (*fn)(), QueryArchetype &qAr, uint32_t eIdx,
            void *ctx)
    {
        auto actualCallback = RCAST(fn, bool (*)(CallbackArgs...));
        QueryIter iter(query->world, 0, ctx, 0);

        iter.eId = qAr.archetype->entities[eIdx];
        // actuallCallback invoke here
        bool valid = query->InvokeFilterCallback(
            actualCallback, iter, qAr, eIdx,
            std::index_sequence_for<CallbackArgs...>{});

        qAr.SetMask(eIdx, valid);
    };
}

////////////////////////// QueryBuilderBase //////////////////////////////

template <typename Handle, typename Derived, typename CallbackBuilder,
          typename... T>
Derived &
QueryBuilderBase<Handle, Derived, CallbackBuilder, T...>::Term(EntityId cId)
{
    if (!m_firstTerm)
    {
        m_desc.terms[m_currTermIdx++] = m_currTerm;
        assert(m_currTermIdx <= (32 - sizeof...(T)));
        m_currTerm = QueryTerm();
        m_currTerm.fieldId = m_currTermIdx;
    }
    else
    {
        m_firstTerm = false;
    }

    m_currTerm.cId = cId;

    return Self();
}

template <typename Handle, typename Derived, typename CallbackBuilder,
          typename... T>
Derived &QueryBuilderBase<Handle, Derived, CallbackBuilder, T...>::Term(
    EntityId relation, EntityId target)
{
    if (!m_firstTerm)
    {
        m_desc.terms[m_currTermIdx++] = m_currTerm;
        assert(m_currTermIdx <= (32 - sizeof...(T)));
        m_currTerm = QueryTerm();
        m_currTerm.fieldId = m_currTermIdx;
    }
    else
    {
        m_firstTerm = false;
    }

    m_currTerm.cId = MakeRelationship(relation, target);

    return Self();
}

template <typename Handle, typename Derived, typename CallbackBuilder,
          typename... T>
template <typename U>
Derived &QueryBuilderBase<Handle, Derived, CallbackBuilder, T...>::Term()
{
    return Term(ComponentTypeId<U>::Id());
}

template <typename Handle, typename Derived, typename CallbackBuilder,
          typename... T>
Derived &QueryBuilderBase<Handle, Derived, CallbackBuilder, T...>::Behavior(
    TermBehavior behavior)
{
    assert(behavior != TermBehavior::STRUCTURE_CHANGE);
    m_currTerm.behavior = behavior;

    return Self();
}

template <typename Handle, typename Derived, typename CallbackBuilder,
          typename... T>
Derived &QueryBuilderBase<Handle, Derived, CallbackBuilder, T...>::Through(
    TraverseMethod method)
{
    m_currTerm.travMethod = method;
    return Self();
}

template <typename Handle, typename Derived, typename CallbackBuilder,
          typename... T>
Derived &QueryBuilderBase<Handle, Derived, CallbackBuilder, T...>::Traverse(
    EntityId relation, EntityId target)
{
    m_currTerm.travRelation = relation;
    m_currTerm.travTarget = target;
    return Self();
}

template <typename Handle, typename Derived, typename CallbackBuilder,
          typename... T>
Derived &QueryBuilderBase<Handle, Derived, CallbackBuilder, T...>::TraverseAny(
    EntityId relation)
{
    return Traverse(relation, EcsAnyId);
}

template <typename Handle, typename Derived, typename CallbackBuilder,
          typename... T>
template <typename U>
Derived &QueryBuilderBase<Handle, Derived, CallbackBuilder, T...>::Traverse(
    EntityId target)
{
    static_assert(!std::is_reference_v<U> && !std::is_const_v<U>);

    return Traverse(ComponentTypeId<U>::Id(), target);
}

template <typename Handle, typename Derived, typename CallbackBuilder,
          typename... T>
template <typename U>
Derived &QueryBuilderBase<Handle, Derived, CallbackBuilder, T...>::TraverseAny()
{
    static_assert(!std::is_reference_v<U> && !std::is_const_v<U>);

    return TraverseAny(ComponentTypeId<U>::Id());
}

template <typename Handle, typename Derived, typename CallbackBuilder,
          typename... T>
Derived &QueryBuilderBase<Handle, Derived, CallbackBuilder, T...>::Op(TermOp op)
{
    m_currTerm.op = op;
    return Self();
}

template <typename Handle, typename Derived, typename CallbackBuilder,
          typename... T>
template <typename U>
Derived &QueryBuilderBase<Handle, Derived, CallbackBuilder, T...>::With(
    TermBehavior behavior)
{
    return Term<U>().Op(HAS).Behavior(behavior);
}

template <typename Handle, typename Derived, typename CallbackBuilder,
          typename... T>
template <typename U>
Derived &QueryBuilderBase<Handle, Derived, CallbackBuilder, T...>::Without()
{
    return Term<U>().Op(NOT);
}

template <typename Handle, typename Derived, typename CallbackBuilder,
          typename... T>
Derived &QueryBuilderBase<Handle, Derived, CallbackBuilder, T...>::With(
    EntityId cId, TermBehavior behavior)
{
    return Term(cId).Op(HAS).Behavior(behavior);
}

template <typename Handle, typename Derived, typename CallbackBuilder,
          typename... T>
Derived &
QueryBuilderBase<Handle, Derived, CallbackBuilder, T...>::Without(EntityId cId)
{
    return Term(cId).Op(NOT);
}

template <typename Handle, typename Derived, typename CallbackBuilder,
          typename... T>
Derived &QueryBuilderBase<Handle, Derived, CallbackBuilder, T...>::With(
    EntityId relation, EntityId target, TermBehavior behavior)
{
    return Term(relation, target).Op(HAS).Behavior(behavior);
}

template <typename Handle, typename Derived, typename CallbackBuilder,
          typename... T>
Derived &QueryBuilderBase<Handle, Derived, CallbackBuilder, T...>::Without(
    EntityId relation, EntityId target)
{
    return Term(relation, target).Op(NOT);
}

template <typename Handle, typename Derived, typename CallbackBuilder,
          typename... T>
template <typename U>
Derived &QueryBuilderBase<Handle, Derived, CallbackBuilder, T...>::Up(
    EntityId target, TermBehavior behavior)
{
    static_assert(!std::is_reference_v<U> && !std::is_const_v<U>);

    return Up(ComponentTypeId<U>::Id(), target).Behavior(behavior);
}

template <typename Handle, typename Derived, typename CallbackBuilder,
          typename... T>
template <typename U>
Derived &QueryBuilderBase<Handle, Derived, CallbackBuilder, T...>::Cascade(
    TermBehavior behavior)
{
    static_assert(!std::is_reference_v<U> && !std::is_const_v<U>);

    return Cascade(ComponentTypeId<U>::Id()).Behavior(behavior);
}

template <typename Handle, typename Derived, typename CallbackBuilder,
          typename... T>
Derived &QueryBuilderBase<Handle, Derived, CallbackBuilder, T...>::Up(
    EntityId relation, EntityId target, TermBehavior behavior)
{
    m_currTerm.validTravTarget = target;
    return Through(UP).Traverse(relation, target).Behavior(behavior);
}

template <typename Handle, typename Derived, typename CallbackBuilder,
          typename... T>
Derived &QueryBuilderBase<Handle, Derived, CallbackBuilder, T...>::Cascade(
    EntityId relation, EntityId target, TermBehavior behavior)
{
    return Through(CASCADE).Traverse(relation, target).Behavior(behavior);
}

template <typename Handle, typename Derived, typename CallbackBuilder,
          typename... T>
template <typename U>
Derived &QueryBuilderBase<Handle, Derived, CallbackBuilder, T...>::Modify()
{
    Term<U>();
    m_currTerm.behavior = STRUCTURE_CHANGE;
    ++m_structureChangeCount;

    return Self();
}

template <typename Handle, typename Derived, typename CallbackBuilder,
          typename... T>
template <typename... CallbackArgs>
CallbackBuilder
QueryBuilderBase<Handle, Derived, CallbackBuilder, T...>::Filter(
    bool (*cb)(CallbackArgs...), void *ctx)
{
    Query *q = BuildQuery();

    return CallbackBuilder(q).Filter(cb, ctx);
}

template <typename Handle, typename Derived, typename CallbackBuilder,
          typename... T>
template <typename... CallbackArgs>
Handle QueryBuilderBase<Handle, Derived, CallbackBuilder, T...>::Each(
    void (*cb)(CallbackArgs...), const char *name, void *ctx)
{
    Query *q = BuildQuery();

    return CallbackBuilder(q).Each(cb, ctx);
}

template <typename Handle, typename Derived, typename CallbackBuilder,
          typename... T>
Query *QueryBuilderBase<Handle, Derived, CallbackBuilder, T...>::BuildQuery()
{
    m_desc.terms[m_currTermIdx] = m_currTerm;

    // construct query
    void *addr = m_world->m_allocators.queries.Alloc();
    Query *query = new (addr) Query(m_world, 0);
    query->structureChangeCount = m_structureChangeCount;
    query->termCount = m_currTermIdx + 1;
    QueryTerm *terms = m_world->m_wAllocator.Alloc<QueryTerm>(query->termCount);
    uint8_t *sortedTermIdx =
        m_world->m_wAllocator.Alloc<uint8_t>(query->termCount);

    for (uint32_t idx = 0; idx < query->termCount; ++idx)
    {
        terms[idx] = m_desc.terms[idx];
        sortedTermIdx[idx] = idx;
    }

    auto priority = [](const QueryTerm &t) -> EntityId
    {
        if (t.behavior == STRUCTURE_CHANGE)
        {
            return EcsInvalidId;
        }

        if (t.travMethod == SELF)
        {

            if (t.op == NOT)
            {
                // This might fighting with EcsName
                return EcsInvalidId + 1;
            }

            return t.cId;
        }
        else
        {
            // relationship is used to filter
            // cId component now is just a pass-into-callback reference from
            // traversed-to entity

            return MakeRelationship(t.travRelation, t.travTarget);
        }
    };

    // sort descending
    // relationship will at the front because of their narrow set

    std::sort(sortedTermIdx, sortedTermIdx + query->termCount,
              [&](uint8_t a, uint8_t b)
              { return priority(terms[a]) > priority(terms[b]); });

    query->terms = terms;
    query->isEntityFiltered = false;
    query->sortedTermIdx = sortedTermIdx;

    return query;
}

////////////////////////// QueryBuilder /////////////////////////////

template <typename... T>
QueryCallbackBuilder QueryBuilder<T...>::Cache(EntityId eId)
{
    Query *q = QueryBuilderBase<QueryHandle, QueryBuilder<T...>, QueryCallbackBuilder, T...>::BuildQuery();

    return QueryCallbackBuilder(q).Cache(eId);
}

////////////////////////// SystemBuilder /////////////////////////////

template <typename... T>
SystemCallbackBuilder SystemBuilder<T...>::Phase(EntityId eId)
{
    Query *q = QueryBuilderBase<SystemHandle, SystemBuilder<T...>, SystemCallbackBuilder, T...>::BuildQuery();

    return SystemCallbackBuilder(q).Phase(eId);
}

//////////////////// QueryCallbackBuilderBase ///////////////////////

template <typename Derived, typename Handle>
template <typename... CallbackArgs>
QueryCallBackBuilderBase<Derived, Handle> &
QueryCallBackBuilderBase<Derived, Handle>::Filter(bool (*cb)(CallbackArgs...),
                                                  void *ctx)
{
    m_query->SetFilterCallback(cb, ctx);

    return *this;
}

template <typename Derived, typename Handle>
template <typename... CallbackArgs>
Handle
QueryCallBackBuilderBase<Derived, Handle>::Each(void (*cb)(CallbackArgs...),
                                                const char *name, void *ctx)
{
    m_query->SetExecutionCallback(cb, ctx);

    Derived &self = *PTR_CAST(this, Derived);
    self.CreateCachedEntity(name);

    return Handle(m_query);
}

} // namespace ECS
