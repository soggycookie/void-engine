#include "query.h"
#include "ds/world_allocator.h"
#include "ecs_type.h"
#include "ecs_utils.h"
#include "internal_component.h"
#include "world.h"
#include <cassert>

namespace ECS
{

///////////////////////// Matched Archetype ////////////////////////////

void QueryArchetype::SetMatchedColumnIdx(WorldAllocator &wAllocator,
                                         int32_t *mappedCols, uint32_t count)
{
    int32_t countDiff = count - InlineArrayOptimizationCount;

    std::memcpy(lo_matchedColumnIdx, mappedCols, count * sizeof(int32_t));

    if (countDiff > 0)
    {
        hi_matchedColumnIdx =
            PTR_CAST(wAllocator.Alloc(countDiff * sizeof(int32_t)), int32_t);
        std::memcpy(hi_matchedColumnIdx,
                    mappedCols + InlineArrayOptimizationCount,
                    countDiff * sizeof(int32_t));
    }
}

int32_t QueryArchetype::GetColumnIdx(uint32_t termIdx) const
{
    if (termIdx < InlineArrayOptimizationCount)
    {
        return lo_matchedColumnIdx[termIdx];
    }
    else
    {
        return hi_matchedColumnIdx[termIdx - InlineArrayOptimizationCount];
    }
}

void QueryArchetype::Delete(WorldAllocator &wAllocator,
                            uint32_t callbackSigCount)
{
    if (hi_matchedColumnIdx)
    {
        assert(callbackSigCount > InlineArrayOptimizationCount);
        wAllocator.Free(sizeof(int32_t) *
                            (callbackSigCount - InlineArrayOptimizationCount),
                        hi_matchedColumnIdx);
    }

    if (hi_entityMask)
    {
        constexpr const uint32_t numBitOfUint32 = 32;
        int32_t count = std::ceil(CAST(archetype->count, float) /
                                  CAST(numBitOfUint32, float));

        assert(count > InlineArrayOptimizationCount);

        wAllocator.Free(sizeof(uint32_t) *
                            (count - InlineArrayOptimizationCount),
                        hi_entityMask);
    }
}

void QueryArchetype::AllocateMask(WorldAllocator &wAllocator)
{
    uint32_t totalMaskCount =
        std::ceil(CAST(archetype->count, float) / CAST(EntityPerMask, float));

    uint32_t add = totalMaskCount - maskCapacity;

    if (totalMaskCount > InlineArrayOptimizationCount && add > 0)
    {
        uint32_t *hi = wAllocator.Calloc<uint32_t>(
            totalMaskCount - InlineArrayOptimizationCount);

        if (hi_entityMask)
        {
            uint32_t size =
                (maskCount - InlineArrayOptimizationCount) * sizeof(uint32_t);
            std::memcpy(hi, hi_entityMask, size);
            wAllocator.Free((maskCapacity - InlineArrayOptimizationCount) *
                                sizeof(uint32_t),
                            hi_entityMask);
        }

        hi_entityMask = hi;
        maskCapacity = totalMaskCount;
    }

    maskCount = totalMaskCount;
}

uint32_t &QueryArchetype::Mask(uint32_t eIdx)
{
    uint32_t maskIdx =
        std::ceil(CAST(eIdx, float) / CAST(EntityPerMask, float));

    if (maskIdx >= maskCount || eIdx >= archetype->count)
    {
        assert(0);
    }

    if (maskIdx < InlineArrayOptimizationCount)
    {
        return lo_entityMask[maskIdx];
    }
    else
    {
        return hi_entityMask[maskIdx - InlineArrayOptimizationCount];
    }
}

void QueryArchetype::SetMask(uint32_t eIdx, bool bit)
{
    uint32_t &mask = Mask(eIdx);
    uint32_t idx = eIdx % EntityPerMask;

    if (bit)
    {
        mask |= (1 << idx);
    }
    else
    {
        mask &= ~(1 << idx);
    }
}

bool QueryArchetype::GetMask(uint32_t eIdx)
{
    uint32_t &mask = Mask(eIdx);
    uint32_t idx = eIdx % EntityPerMask;

    return (mask &= (1 << idx)) > 0;
}

//////////////////////////// QueryResult ////////////////////////////////

void QueryResult::Add(WorldAllocator &wAllocator, QueryArchetype &&matched)
{
    constexpr const uint32_t defaultCapacity = 4;

    if (count < InlineArrayOptimizationCount)
    {
        lo_queryArchetypes[count] = std::move(matched);
    }
    else
    {
        if (capacity == (count - InlineArrayOptimizationCount))
        {
            uint32_t capa =
                (capacity == 0) ? DefaultArchetypeCapacity : (capacity * 1.5f);
            uint32_t expandedCapa = 0;

            QueryArchetype *qAr = PTR_CAST(
                wAllocator.AllocN(sizeof(QueryArchetype), capa, expandedCapa),
                QueryArchetype);

            if (hi_queryArchetypes)
            {
                for (size_t idx = 0;
                     idx < (count - InlineArrayOptimizationCount); ++idx)
                {
                    new (&qAr[idx])
                        QueryArchetype(std::move(hi_queryArchetypes[idx]));
                }
            }

            hi_queryArchetypes = qAr;
            capacity = expandedCapa;
        }

        new (&hi_queryArchetypes[count - InlineArrayOptimizationCount])
            QueryArchetype(std::move(matched));
    }

    ++count;
}

void QueryResult::Delete(WorldAllocator &wAllocator, uint32_t callbackSigCount)
{

    int32_t countDiff = count - InlineArrayOptimizationCount;

    if (countDiff <= 0)
    {
        for (size_t idx = 0; idx < count; ++idx)
        {
            lo_queryArchetypes[idx].Delete(wAllocator, callbackSigCount);
        }
    }
    else
    {
        assert(hi_queryArchetypes);

        for (size_t idx = 0; idx < count - InlineArrayOptimizationCount; ++idx)
        {
            hi_queryArchetypes[idx].Delete(wAllocator, callbackSigCount);
            hi_queryArchetypes[idx].~QueryArchetype();
        }

        wAllocator.Free(sizeof(QueryArchetype) * capacity, hi_queryArchetypes);
    }
}

/////////////////////////////// Query /////////////////////////////////

void Query::Execute()
{
    execCallback.invoker(this, execCallback.fn, execCallback.ctx);
}

void Query::FilterResultEntity()
{
    for (size_t idx = 0; idx < result.count; ++idx)
    {
        result[idx].AllocateMask(world->m_wAllocator);
        for (size_t eIdx = 0; eIdx < result[idx].archetype->count; ++eIdx)
        {
            entityFilterCallback.invoker(this, entityFilterCallback.fn,
                                         result[idx], eIdx,
                                         entityFilterCallback.ctx);
        }
    }
}

void Query::FilterEntity(QueryArchetype &qAr)
{
    for (size_t eIdx = 0; eIdx < qAr.archetype->count; ++eIdx)
    {
        entityFilterCallback.invoker(this, entityFilterCallback.fn, qAr, eIdx,
                                     entityFilterCallback.ctx);
    }
}

void Query::FilterEntity(QueryArchetype &qAr, uint32_t eIdx)
{

    entityFilterCallback.invoker(this, entityFilterCallback.fn, qAr, eIdx,
                                 entityFilterCallback.ctx);
}

void Query::FilterArchetype()
{
    assert(termCount > 0);
    QueryResult result;

    const QueryTerm &anchorTerm = terms[sortedTermIdx[0]];

    if (anchorTerm.op == NOT)
    {
        assert(0 && "Query should not rely on only NOT operations!");
    }

    EntityId anchorTermId = EcsInvalidId;

    if (anchorTerm.travMethod == SELF)
    {
        anchorTermId = anchorTerm.cId;
    }
    else
    {
        anchorTermId = MakeRelationship(
            anchorTerm.travRelation,
            anchorTerm.travTarget == EcsAnyId ? 0 : anchorTerm.travTarget);
    }

    const ComponentRecord &anchorCr = world->m_componentIndex[anchorTermId];

    if (eId != EcsInvalidId)
    {
        // ComponentRecord which has its cId is used as term cid will keep track
        // of the cached query
        for (size_t idx = 0; idx < termCount; ++idx)
        {
            const QueryTerm &term = terms[idx];
            if (term.op == HAS)
            {
                switch (term.travMethod)
                {
                case SELF:
                {
                    EntityId hiId = HI_ENTITY_ID(term.cId);
                    EntityId cId =
                        hiId == EcsAnyId ? LO_ENTITY_ID(term.cId) : term.cId;
                    ComponentRecord &cr = world->m_componentIndex[cId];
                    cr.trackedQueries.Add(world->m_wAllocator, eId);
                    break;
                }
                case UP:
                case CASCADE:
                {
                    EntityId cId = term.travTarget == EcsAnyId
                                       ? term.travRelation
                                       : MakeRelationship(term.travRelation,
                                                          term.travTarget);
                    ComponentRecord &cr = world->m_componentIndex[cId];
                    cr.trackedQueries.Add(world->m_wAllocator, eId);
                    break;
                }
                }
            }
        }
    }
    // anchor term work as a point to narrow down archetype list
    for (size_t aIdx = 0; aIdx < anchorCr.archetypes.count; ++aIdx)
    {
        Archetype *archetype = anchorCr.archetypes[aIdx];

        MatchedArchetype matched = IsMatch(archetype);

        if (matched.matched)
        {
            assert(matched.matchedColumns);
            if (eId != EcsInvalidId)
            {
                archetype->trackedQueries.Add(world->m_wAllocator,
                                              TrackedQuery{eId, result.count});
            }

            QueryArchetype qAr(archetype);
            qAr.SetMatchedColumnIdx(world->m_wAllocator, matched.matchedColumns,
                                    termCount);
            result.Add(world->m_wAllocator, std::move(qAr));
        }
    }

    this->result = std::move(result);
}

MatchedArchetype Query::IsMatch(Archetype *archetype)
{
    int32_t *matchedColumns = world->m_wAllocator.Alloc<int32_t>(termCount);
    assert(matchedColumns);

    const ComponentSet &cs = archetype->componentSet;
    bool isValid = true;

    if (!matchedColumns)
    {
        matchedColumns = world->m_wAllocator.Alloc<int32_t>(termCount);
    }

    // termIdx start at 0 not 1 because the first term maybe a traversal
    // term and it need to be validated carefully 1 only work with
    // SELF-matched term
    for (size_t termIdx = 0; termIdx < termCount; ++termIdx)
    {
        QueryTerm &term = terms[termIdx];

        switch (term.travMethod)
        {
        case ECS::SELF:
        {
            if (term.op == HAS)
            {
                int32_t cIdx = cs.Search(term.cId);
                if (cIdx == ComponentSet::NotFoundIdx)
                {
                    isValid = false;
                }
                else
                {
                    matchedColumns[termIdx] = archetype->componentMap[cIdx];
                }
            }
            else if (term.op == NOT)
            {
                if (cs.Has(term.cId))
                {
                    isValid = false;
                }
                else
                {
                    matchedColumns[termIdx] = ComponentSet::NotFoundIdx;
                }
            }
            else
            {
                assert(0);
            }
            break;
        }
        case ECS::UP:
        {
            EntityId relationship =
                MakeRelationship(term.travRelation, term.travTarget);

            if (!cs.HasRelationship(relationship))
            {
                isValid = false;
                break;
            }

            EntityId target = term.travTarget;

            if (target == EcsAnyId)
            {
                target = HI_ENTITY_ID(cs[cs.SearchRelationship(relationship)]);
            }

            Archetype *targetArchetype = world->GetEntityArchetype(target);

            if (term.op == HAS)
            {
                int32_t cIdx = targetArchetype->componentSet.Search(term.cId);
                cIdx = (cIdx == ComponentSet::NotFoundIdx)
                           ? targetArchetype->componentSet.SearchRelationship(
                                 term.cId)
                           : cIdx;

                if (cIdx == ComponentSet::NotFoundIdx)
                {
                    isValid = false;
                }
                else
                {
                    term.validTravTarget = target;
                    matchedColumns[termIdx] =
                        targetArchetype->componentMap[cIdx];
                }
            }
            else if (term.op == NOT)
            {
                if (world->HasComponent(target, term.cId) ||
                    world->HasRelationship(target, term.cId))
                {
                    isValid = false;
                }
                else
                {
                    matchedColumns[termIdx] = ComponentSet::NotFoundIdx;
                }
            }
            else
            {
                assert(0);
            }

            break;
        }
        case ECS::CASCADE:
        {
            EntityId relationship =
                MakeRelationship(term.travRelation, term.travTarget);

            if (!cs.HasRelationship(relationship))
            {
                isValid = false;
                break;
            }

            EntityId target = term.travTarget;

            if (target == EcsAnyId)
            {
                target = HI_ENTITY_ID(cs[cs.SearchRelationship(relationship)]);
            }
            else
            {
                assert(0);
            }

            Archetype *targetArchetype = world->GetEntityArchetype(target);
            assert(targetArchetype);
            if (term.op == HAS)
            {
                while (true)
                {
                    int32_t cIdx =
                        targetArchetype->componentSet.Search(term.cId);
                    cIdx =
                        (cIdx == ComponentSet::NotFoundIdx)
                            ? targetArchetype->componentSet.SearchRelationship(
                                  term.cId)
                            : cIdx;

                    if (cIdx == ComponentSet::NotFoundIdx)
                    {
                        int32_t targetRelationshipIdx =
                            targetArchetype->componentSet.SearchRelationship(
                                relationship);

                        if (targetRelationshipIdx == ComponentSet::NotFoundIdx)
                        {
                            isValid = false;
                            break;
                        }
                        else
                        {
                            target = HI_ENTITY_ID(
                                targetArchetype
                                    ->componentSet[targetRelationshipIdx]);
                            targetArchetype = world->GetEntityArchetype(target);
                            assert(targetArchetype);
                        }
                    }
                    else
                    {
                        term.validTravTarget = target;
                        matchedColumns[termIdx] =
                            targetArchetype->componentMap[cIdx];
                        // valid
                        break;
                    }
                }
            }
            else if (term.op == NOT)
            {
                while (true)
                {
                    if (world->HasComponent(target, term.cId) ||
                        world->HasRelationship(target, term.cId))
                    {
                        int32_t targetRelationshipIdx =
                            targetArchetype->componentSet.SearchRelationship(
                                relationship);

                        if (targetRelationshipIdx == ComponentSet::NotFoundIdx)
                        {
                            isValid = false;
                            break;
                        }
                        else
                        {
                            target = HI_ENTITY_ID(
                                targetArchetype
                                    ->componentSet[targetRelationshipIdx]);
                            targetArchetype = world->GetEntityArchetype(target);
                            assert(targetArchetype);
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
                assert(0);
            }

            break;
        }
        default:
        {
            assert(0);
        }
        }

        if (!isValid)
        {
            break;
        }
    }

    if (!isValid)
    {
        world->m_wAllocator.Free(sizeof(int32_t) * termCount, matchedColumns);
        matchedColumns = nullptr;
    }

    return MatchedArchetype{matchedColumns, isValid};
}

void Query::Destroy()
{
    assert(world);
    assert(terms);

    world->m_wAllocator.Free(termCount * sizeof(QueryTerm), terms);
    world->m_wAllocator.Free(termCount * sizeof(uint8_t), sortedTermIdx);

    result.Delete(world->m_wAllocator, execCallback.sigCount);
}

///////////////

void QueryHandle::Execute()
{
    if (m_query)
    {
        if (m_query->eId == EcsInvalidId)
        {
            // ad-hoc filter
            m_query->FilterArchetype();

            if (m_query->isEntityFiltered)
            {
                m_query->FilterResultEntity();
            }
        }

        m_query->Execute();
    }
}

void QueryHandle::Destroy()
{
    if (m_query)
    {
        m_query->Destroy();

        if (m_query->eId != EcsInvalidId)
        {
            m_query->world->RemoveEntity(m_query->eId);
        }

        m_query->world->m_allocators.queries.Free(m_query);
        m_query = nullptr;
    }
    else
    {
        assert(0);
    }
}

/////////////////////////// QueryCallbackBuilder /////////////////////////

void QueryCallbackBuilder::CreateCachedEntity()
{
    if (m_query->isCached)
    {
        Entity e = m_query->world->CreateEntity(m_query->eId);
        m_query->eId = e.GetFullId();
        m_query->FilterArchetype();

        if (m_query->isEntityFiltered)
        {
            m_query->FilterResultEntity();
        }

        e.AddComponent<EcsQuery>();
        e.Set<EcsQuery>(EcsQuery{m_query});
    }
}

/////////////////////////// QueryCallbackBuilder /////////////////////////

void SystemCallbackBuilder::CreateCachedEntity()
{
    if (m_query->isCached)
    {
        Entity e = m_query->world->CreateEntity(m_query->eId);
        m_query->eId = e.GetFullId();
        m_query->FilterArchetype();

        if (m_query->isEntityFiltered)
        {
            m_query->FilterResultEntity();
        }

        e.AddComponent<EcsSystem>();
        e.Set<EcsSystem>(EcsSystem{m_query});

        if (m_dependOnId == EcsInvalidId)
        {
            assert(0);
        }
        else
        {
            bool a = m_query->world->HasComponent(m_dependOnId, EcsPhaseId);
            bool b =
                m_query->world->HasRelationship(m_dependOnId, EcsDependOnId);

            if (m_query->world->HasComponent(m_dependOnId, EcsPhaseId) &&
                m_query->world->HasRelationship(m_dependOnId, EcsDependOnId))
            {
                e.AddRelationship<EcsDependOn>(m_dependOnId);
            }
            else
            {
                assert(0);
            }
        }
    }
    else
    {
        assert(0 && "System must be cached!");
    }
}

} // namespace ECS
