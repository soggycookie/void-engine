#include "ecs_type.h"
#include "ecs_utils.h"
#include "entity.h"
#include "internal_component.h"
#ifdef __clang__
#pragma once
#include "query.h"
#include "world.h"
#endif //__clang__

namespace ECS
{

///////////////////////////////// Query /////////////////////////////////////

template <typename... CallbackArgs, size_t... I>
void Query::InvokeCallback(Query *query, void (*cb)(CallbackArgs...),
                           QueryIter &iter, const MatchedArchetype &matched,
                           uint32_t eIdx, std::index_sequence<I...>)
{
    cb(query->GetArg<CallbackArgs>(iter, matched, I, eIdx)...);
}

template <typename CallbackArg>
CallbackArg Query::GetArg(const QueryIter &iter,
                          const MatchedArchetype &matched, uint32_t sigIdx,
                          uint32_t eIdx)
{
    if constexpr (std::is_same_v<CallbackArg, const QueryIter &>)
    {
        return iter;
    }
    else
    {
        const QueryTerm &term = terms[callback.sigIdxToTermIdx[sigIdx]];

        void *src = nullptr;
        if (term.travMethod == SELF)
        {
            if (term.op == HAS)
            {
                int32_t colIdx =
                    matched.GetColumnIdx(callback.sigIdxToTermIdx[sigIdx]);
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
                    matched.GetColumnIdx(callback.sigIdxToTermIdx[sigIdx]);
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
void Query::Each(void (*cb)(CallbackArgs...), void *ctx)
{
    // no const or ref or both on entity and query iter
    // static_assert(at_most_one_entity<CallbackArgs...>);
    static_assert(sizeof...(CallbackArgs) >= 1 && sizeof...(CallbackArgs) <= 32,
                  "Callback signatures size must equal or greater than 1");
    static_assert(((sizeof(CallbackArgs) > 1) && ...),
                  "All types must have size > 1");

    static_assert(at_most_one_query_iter<CallbackArgs...> &&
                      is_first_arg_query_iter<CallbackArgs...>,
                  "Query Iter must be const ref and the first args!");

    callback.sigCount = sizeof...(CallbackArgs);
    callback.ctx = ctx;
    callback.fn = RCAST(cb, void (*)());
    callback.sigIdxToTermIdx =
        PTR_CAST(world->m_wAllocator.Alloc(callback.sigCount), int32_t);

    uint32_t termBitmask = 0;

    int64_t callbackSig[] = {
        (std::is_same_v<CallbackArgs, const QueryIter &>
             ? QueryIterIndex
             : ComponentTypeId<std::decay_t<CallbackArgs>>::Id())...};

    // This will create the sig mapping and validate the callback sig
    for (size_t idx = 0; idx < callback.sigCount; ++idx)
    {
        int64_t callbackSigId = callbackSig[idx];

        if (callbackSigId == QueryIterIndex)
        {
            callback.sigIdxToTermIdx[idx] = QueryIterIndex;
            continue;
        }

        for (size_t queryTermIdx = 0; queryTermIdx < termCount; ++queryTermIdx)
        {
            const QueryTerm &term = terms[queryTermIdx];

            if (term.op == NOT)
            {
                continue;
            }

            bool isMasked = (termBitmask & (1 << queryTermIdx)) != 0;
            if (isMasked)
            {
                continue;
            }

            if (LO_ENTITY_ID(term.cId) == callbackSigId)
            {
                callback.sigIdxToTermIdx[idx] = queryTermIdx;
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
    callback.invoker = +[](Query *query, void (*fn)(), void *ctx)
    {
        auto actualCallback = RCAST(fn, void (*)(CallbackArgs...));

        for (size_t idx = 0; idx < query->result.count; ++idx)
        {
            const Archetype *archetype = query->result[idx].archetype;

            QueryIter iter(query->world, 0, ctx, 0);

            // entity level filter
            if (query->isEntityFiltered)
            {
            }
            // archetype level filter
            else
            {
                for (size_t eIdx = 0; eIdx < archetype->count; ++eIdx)
                {
                    iter.eId = archetype->entities[eIdx];
                    // actuallCallback invoke here
                    Query::InvokeCallback<CallbackArgs...>(
                        query, actualCallback, iter, query->result[idx], eIdx,
                        std::index_sequence_for<CallbackArgs...>{});
                }
            }
        }
    };
}

////////////////////////// Query Builder //////////////////////////////

template <typename... T>
QueryBuilder<T...> &QueryBuilder<T...>::Term(EntityId cId)
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

    return *this;
}

template <typename... T>
QueryBuilder<T...> &QueryBuilder<T...>::Term(EntityId first, EntityId second)
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

    m_currTerm.cId = MakeRelationship(first, second);

    return *this;
}

template <typename... T>
template <typename U>
QueryBuilder<T...> &QueryBuilder<T...>::Term()
{
    return Term(ComponentTypeId<U>::Id());
}

template <typename... T>
QueryBuilder<T...> &QueryBuilder<T...>::Through(TraverseMethod method)
{
    m_currTerm.travMethod = method;
    return *this;
}

// template <typename... T>
// QueryBuilder<T...> &QueryBuilder<T...>::TraveseTarget(EntityId targetId)
// {
//     m_currTerm.travTarget = targetId;
//     return *this;
// }

template <typename... T>
QueryBuilder<T...> &QueryBuilder<T...>::Traverse(EntityId relation,
                                                 EntityId target)
{
    m_currTerm.travRelation = relation;
    m_currTerm.travTarget = target;
    return *this;
}

template <typename... T>
QueryBuilder<T...> &QueryBuilder<T...>::TraverseAny(EntityId relation)
{
    return Traverse(relation, EcsAnyId);
}

template <typename... T>
template <typename U>
QueryBuilder<T...> &QueryBuilder<T...>::Traverse(EntityId target)
{
    static_assert(!std::is_reference_v<U> && !std::is_const_v<U>);

    return Traverse(ComponentTypeId<U>::Id(), target);
}

template <typename... T>
template <typename U>
QueryBuilder<T...> &QueryBuilder<T...>::TraverseAny()
{
    static_assert(!std::is_reference_v<U> && !std::is_const_v<U>);

    return TraverseAny(ComponentTypeId<U>::Id());
}

template <typename... T>
QueryBuilder<T...> &QueryBuilder<T...>::Op(TermOp op)
{
    m_currTerm.op = op;
    return *this;
}

template <typename... T>
QueryBuilder<T...> &QueryBuilder<T...>::Cache(EntityId cacheId)
{
    m_desc.cache = true;
    m_desc.eId = cacheId;
    return *this;
}

template <typename... T>
QueryBuilder<T...> &QueryBuilder<T...>::Scope()
{
    m_desc.cache = false;

    return *this;
}

template <typename... T>
template <typename U>
QueryBuilder<T...> &QueryBuilder<T...>::With()
{
    return Term<U>().Op(HAS);
}

template <typename... T>
template <typename U>
QueryBuilder<T...> &QueryBuilder<T...>::Without()
{
    return Term<U>().Op(NOT);
}

//
// template <typename... T>
// template <typename U>
// QueryBuilder<T...> &QueryBuilder<T...>::Optional()
// {
//     return Term<U>().Optional();
// }
//
// template <typename... T>
// QueryBuilder<T...> &QueryBuilder<T...>::With()
// {
//     return Op(HAS);
// }
//
// template <typename... T>
// QueryBuilder<T...> &QueryBuilder<T...>::Without()
// {
//     return Op(NOT);
// }

// template <typename... T>
// QueryBuilder<T...> &QueryBuilder<T...>::Optional()
// {
//     return Op(OPTIONAL);
// }

// template <typename... T>
// QueryBuilder<T...> &QueryBuilder<T...>::SelfUp(EntityId target)
// {
//     return Traverse(SELF_UP).TraveseTarget(target);
// }
//
// template <typename... T>
// QueryBuilder<T...> &QueryBuilder<T...>::SelfUp(EntityId first, EntityId
// second)
// {
//     return Traverse(SELF_UP).TraveseTarget(first, second);
// }

// template <typename... T>
// QueryBuilder<T...> &QueryBuilder<T...>::Up(EntityId target)
// {
//     return Traverse(UP).TraveseTarget(target);
// }

template <typename... T>
template <typename U>
QueryBuilder<T...> &QueryBuilder<T...>::Up(EntityId target)
{
    static_assert(!std::is_reference_v<U> && !std::is_const_v<U>);

    return Up(ComponentTypeId<U>::Id(), target);
}

template <typename... T>
template <typename U>
QueryBuilder<T...> &QueryBuilder<T...>::Cascade()
{
    static_assert(!std::is_reference_v<U> && !std::is_const_v<U>);

    return Cascade(ComponentTypeId<U>::Id());
}

template <typename... T>
QueryBuilder<T...> &QueryBuilder<T...>::Up(EntityId relation, EntityId target)
{
    m_currTerm.validTravTarget = target;
    return Through(UP).Traverse(relation, target);
}

template <typename... T>
QueryBuilder<T...> &QueryBuilder<T...>::Cascade(EntityId relation)
{
    return Through(CASCADE).TraverseAny(relation);
}

// template <typename... Components>
// QueryBuilder<Components...> &
// QueryBuilder<Components...>::Cascade(EntityId first, EntityId second)
// {
//     return Traverse(CASCADE).TraveseTarget(first, second);
// }

template <typename... T>
template <typename U>
QueryBuilder<T...> &QueryBuilder<T...>::Modify()
{
    Term<U>();
    m_currTerm.behavior = STRUCTURE_CHANGE;

    return *this;
}

template <typename... T>
template <typename... CallbackArgs>
QueryHandle QueryBuilder<T...>::Each(void (*callback)(CallbackArgs...),
                                     void *ctx)
{
    m_desc.terms[m_currTermIdx] = m_currTerm;

    // construct query
    void *addr = m_world->m_wAllocator.Alloc(sizeof(Query));
    Query *query = new (addr) Query(m_world, 0);
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
        if (t.travMethod == SELF)
        {
            if (t.op == NOT)
            {
                return EcsInvalidId;
            }

            return t.cId;
        }
        else
        {
            // relationship is used to filter
            // cId component now is just a pass-into-callback reference

            if (t.op == NOT)
            {
                return EcsInvalidId;
            }

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

    if (m_desc.cache)
    {
        Entity e = m_world->CreateEntity(m_desc.eId);
        query->eId = e.GetLowId();
        query->Filter();

        e.AddComponent<EcsQuery>();
        e.Set<EcsQuery>(EcsQuery{query});

        // Observe archetype change
    }

    query->Each<CallbackArgs...>(callback, ctx);

    return QueryHandle(query->eId, query);
}
} // namespace ECS
