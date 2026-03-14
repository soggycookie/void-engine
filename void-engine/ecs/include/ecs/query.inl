#include "ecs_type.h"
#include "ecs_utils.h"
#include "entity.h"
#include <type_traits>
#ifdef __clang__
#pragma once
#include "query.h"
#include "world.h"
#endif //__clang__

namespace ECS
{

///////////////////////////////// Query /////////////////////////////////////



template <typename CallbackArg>
CallbackArg Query::GetArg(QueryIter iter, Archetype *archetype,
                           uint32_t sigIdx, uint32_t eIdx)
{
    if constexpr (std::is_same_v<std::remove_reference_t<CallbackArg>, QueryIter>)
    {
        return iter;
    }
    else
    {
        const QueryTerm &term = m_terms[m_callback.mappedSig[sigIdx]];
        int32_t cIdx = archetype->componentSet.Search(term.cId);
        assert(cIdx != -1);
        int32_t colIdx = archetype->componentMap[cIdx];
        assert(colIdx != -1);

        Column &col = archetype->columns[colIdx];
        TypeInfo &ti = *col.typeInfo;

        void *src = OFFSET_ELEMENT(col.data, ti.size, eIdx);

        return *static_cast<std::remove_reference_t<CallbackArg> *>(src);
    }
}

template <typename... CallbackArgs>
void Query::Each(void (*cb)(CallbackArgs...), void *ctx)
{
    // no const or ref or both on entity and query iter
    // static_assert(at_most_one_entity<CallbackArgs...>);
    static_assert(at_most_one_query_iter<CallbackArgs...>);
    static_assert(sizeof...(CallbackArgs) >= 1 && sizeof...(CallbackArgs) <= 32,
                  "Callback signatures size must equal or greater than 1");
    static_assert(((sizeof(CallbackArgs) > 1) && ...),
                  "All types must have size > 1");

    m_callback.sigCount = sizeof...(CallbackArgs);
    m_callback.ctx = ctx;
    m_callback.fn = RCAST(cb, void (*)());
    m_callback.mappedSig =
        PTR_CAST(m_world->m_wAllocator.Alloc(m_callback.sigCount), int32_t);

    uint32_t termBitmask = 0;

    // support -1 - QueryIterIndex
    int64_t callbackSig[] = {
        (std::is_same_v<CallbackArgs, QueryIter>
             ? QueryIterIndex
             : ComponentTypeId<std::decay_t<CallbackArgs>>::Id())...
    };

    // This will create the sig mapping and validate the callback sig
    for (size_t idx = 0; idx < m_callback.sigCount; ++idx)
    {
        std::cout << callbackSig[idx] << std::endl;
        int64_t callbackSigId = callbackSig[idx];

        if (callbackSigId == QueryIterIndex)
        {
            m_callback.mappedSig[idx] = QueryIterIndex;
            continue;
        }

        for (size_t queryTermIdx = 0; queryTermIdx < m_termCount;
             ++queryTermIdx)
        {
            const QueryTerm &term = m_terms[queryTermIdx];

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
                m_callback.mappedSig[idx] = queryTermIdx;
                termBitmask |= (1 << queryTermIdx);
                break;
            }

            if (queryTermIdx == m_termCount - 1)
            {
                assert(0 && "Callback signatures are not valid!");
            }
        }
    }

    // map callback sig to query term
    m_callback.invoker = +[](Query *query, void (*fn)(), void *ctx)
    {
        auto actualCallback = RCAST(fn, void (*)(CallbackArgs...));

        if (query->m_eId == 0)
        {
            // ad-hoc filter
            query->Filter();
        }

        for (size_t idx = 0; idx < query->m_result.count; ++idx)
        {
            const Archetype *archetype = query->m_result[idx].archetype;

            QueryIter iter(query->m_world, 0, ctx, 0);

            // entity level filter
            if (query->m_isEntityFiltered)
            {
            }
            // archetype level filter
            else
            {
                for (size_t eIdx = 0; eIdx < archetype->count; ++eIdx)
                {
                    iter.eId = archetype->entities[eIdx];
                    //actuallCallback invoke here
                    Query::InvokeCallback<CallbackArgs...>(
                        query,
                        actualCallback,
                        iter,
                        const_cast<Archetype*>(archetype),
                        eIdx,
                        std::index_sequence_for<CallbackArgs...>{}
                    );
                }
            }
        }

        if (query->m_eId == 0)
        {
            // NOTE: clear query result;
            query->m_result.Delete(query->m_world->m_wAllocator,
                                   query->m_callback.sigCount);
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
QueryBuilder<T...> &QueryBuilder<T...>::Traverse(TraverseMethod method)
{
    m_currTerm.trav = method;
    return *this;
}

template <typename... T>
QueryBuilder<T...> &QueryBuilder<T...>::TraveseTarget(EntityId targetId)
{
    m_currTerm.travTarget = targetId;
    return *this;
}

template <typename... T>
QueryBuilder<T...> &QueryBuilder<T...>::TraveseTarget(EntityId first,
                                                      EntityId second)
{
    m_currTerm.travTarget = MakeRelationship(first, second);
    return *this;
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
    return Term<U>().With();
}

template <typename... T>
template <typename U>
QueryBuilder<T...> &QueryBuilder<T...>::Without()
{
    return Term<U>().Without();
}

//
// template <typename... T>
// template <typename U>
// QueryBuilder<T...> &QueryBuilder<T...>::Optional()
// {
//     return Term<U>().Optional();
// }
//
template <typename... T>
QueryBuilder<T...> &QueryBuilder<T...>::With()
{
    return Op(HAS);
}

template <typename... T>
QueryBuilder<T...> &QueryBuilder<T...>::Without()
{
    return Op(NOT);
}

// template <typename... T>
// QueryBuilder<T...> &QueryBuilder<T...>::Optional()
// {
//     return Op(OPTIONAL);
// }

template <typename... T>
QueryBuilder<T...> &QueryBuilder<T...>::SelfUp(EntityId target)
{
    return Traverse(SELF_UP).TraveseTarget(target);
}

template <typename... T>
QueryBuilder<T...> &QueryBuilder<T...>::SelfUp(EntityId first, EntityId second)
{
    return Traverse(SELF_UP).TraveseTarget(first, second);
}

template <typename... T>
QueryBuilder<T...> &QueryBuilder<T...>::Up(EntityId target)
{
    return Traverse(UP).TraveseTarget(target);
}

template <typename... T>
QueryBuilder<T...> &QueryBuilder<T...>::Up(EntityId first, EntityId second)
{
    return Traverse(SELF_UP).TraveseTarget(first, second);
}

template <typename... T>
QueryBuilder<T...> &QueryBuilder<T...>::Cascade(EntityId target)
{
    return Traverse(CASCADE).TraveseTarget(target);
}

template <typename... Components>
QueryBuilder<Components...> &
QueryBuilder<Components...>::Cascade(EntityId first, EntityId second)
{
    return Traverse(CASCADE).TraveseTarget(first, second);
}

template <typename... T>
template <typename U>
QueryBuilder<T...> &QueryBuilder<T...>::Modify()
{
    Term<U>();
    m_currTerm.behavior = STRUCTURE_CHANGE;

    return *this;
}

template <typename... T>
Query *QueryBuilder<T...>::Build()
{
    m_desc.terms[m_currTermIdx] = m_currTerm;

    // construct query
    void *addr = m_world->m_wAllocator.Alloc(sizeof(Query));
    Query *query = new (addr) Query(m_world, 0);
    query->m_termCount = m_currTermIdx + 1;
    QueryTerm *terms =
        m_world->m_wAllocator.Alloc<QueryTerm>(query->m_termCount);

    for (uint32_t idx = 0; idx < query->m_termCount; ++idx)
    {
        terms[idx] = m_desc.terms[idx];
    }

    auto priority = [](const QueryTerm &t) -> EntityId
    {
        if (t.op == NOT)
        {
            return 0;
        }
        return t.cId;
    };

    // sort descending
    // relationship will at the front because of their narrow set
    std::sort(terms, terms + query->m_termCount,
              [&](const QueryTerm &a, const QueryTerm &b)
              { return priority(a) > priority(b); });

    query->m_terms = terms;
    query->m_isEntityFiltered = false;

    if (m_desc.cache)
    {
        Entity e = m_world->CreateEntity();
        query->m_eId = e.GetLowId();
        query->Filter();

        e.AddComponent<EcsQuery>();
        e.Set<EcsQuery>(EcsQuery{query});

        // Observe archetype change
    }

    return query;
}
} // namespace ECS
