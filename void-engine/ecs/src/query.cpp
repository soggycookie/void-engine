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

void MatchedArchetype::SetMatchedColumnIdx(WorldAllocator &wAllocator,
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

int32_t MatchedArchetype::GetColumnIdx(uint32_t termIdx) const
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

void MatchedArchetype::Delete(WorldAllocator &wAllocator,
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
//////////////////////////// QueryResult ////////////////////////////////

void QueryResult::Add(WorldAllocator &wAllocator, MatchedArchetype &&matched)
{
    constexpr const uint32_t defaultCapacity = 4;

    if (count < InlineArrayOptimizationCount)
    {
        lo_matchedArchetypes[count] = std::move(matched);
    }
    else
    {
        if (capacity == (count - InlineArrayOptimizationCount))
        {
            uint32_t capa =
                (capacity == 0) ? DefaultArchetypeCapacity : (capacity * 1.5f);
            uint32_t expandedCapa = 0;

            MatchedArchetype *newMatched = PTR_CAST(
                wAllocator.AllocN(sizeof(MatchedArchetype), capa, expandedCapa),
                MatchedArchetype);

            if (hi_matchedArchetypes)
            {
                for (size_t idx = 0;
                     idx < (count - InlineArrayOptimizationCount); ++idx)
                {
                    new (&newMatched[idx]) MatchedArchetype(
                        std::move(hi_matchedArchetypes[idx]));
                }
            }

            hi_matchedArchetypes = newMatched;
            capacity = expandedCapa;
        }

        new (&hi_matchedArchetypes[count - InlineArrayOptimizationCount])
            MatchedArchetype(std::move(matched));
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
            lo_matchedArchetypes[idx].Delete(wAllocator, callbackSigCount);
        }
    }
    else
    {
        assert(hi_matchedArchetypes);

        for (size_t idx = 0; idx < count - InlineArrayOptimizationCount; ++idx)
        {
            hi_matchedArchetypes[idx].Delete(wAllocator, callbackSigCount);
            hi_matchedArchetypes[idx].~MatchedArchetype();
        }

        wAllocator.Free(sizeof(MatchedArchetype) * capacity,
                        hi_matchedArchetypes);
    }
}

/////////////////////////////// Query /////////////////////////////////

void Query::Execute() { callback.invoker(this, callback.fn, callback.ctx); }

void Query::Filter()
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

    const ComponentRecord &cr = world->m_componentIndex[anchorTermId];

    int32_t *matchedColumns = world->m_wAllocator.Alloc<int32_t>(termCount);
    assert(matchedColumns);

    // anchor term work as a point to narrow down archetype list
    for (size_t aIdx = 0; aIdx < cr.archetypeStore.count; ++aIdx)
    {
        Archetype *archetype = cr.archetypeStore[aIdx];
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
            QueryTerm &term = terms[sortedTermIdx[termIdx]];

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
                    target =
                        HI_ENTITY_ID(cs[cs.SearchRelationship(relationship)]);
                }

                Archetype *targetArchetype = world->GetEntityArchetype(target);

                if (term.op == HAS)
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
                    target =
                        HI_ENTITY_ID(cs[cs.SearchRelationship(relationship)]);
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
                        cIdx = (cIdx == ComponentSet::NotFoundIdx)
                                   ? targetArchetype->componentSet
                                         .SearchRelationship(term.cId)
                                   : cIdx;

                        if (cIdx == ComponentSet::NotFoundIdx)
                        {
                            int32_t targetRelationshipIdx =
                                targetArchetype->componentSet
                                    .SearchRelationship(relationship);

                            if (targetRelationshipIdx ==
                                ComponentSet::NotFoundIdx)
                            {
                                isValid = false;
                                break;
                            }
                            else
                            {
                                target = HI_ENTITY_ID(
                                    targetArchetype
                                        ->componentSet[targetRelationshipIdx]);
                                targetArchetype =
                                    world->GetEntityArchetype(target);
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
                                targetArchetype->componentSet
                                    .SearchRelationship(relationship);

                            if (targetRelationshipIdx ==
                                ComponentSet::NotFoundIdx)
                            {
                                isValid = false;
                                break;
                            }
                            else
                            {
                                target = HI_ENTITY_ID(
                                    targetArchetype
                                        ->componentSet[targetRelationshipIdx]);
                                targetArchetype =
                                    world->GetEntityArchetype(target);
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

        if (isValid)
        {
            if (eId != EcsInvalidId)
            {
                archetype->trackedQuery.Add(world->m_wAllocator, eId);
            }

            MatchedArchetype ma(archetype);
            ma.SetMatchedColumnIdx(world->m_wAllocator, matchedColumns,
                                 termCount);
            matchedColumns = nullptr;
            result.Add(world->m_wAllocator, std::move(ma));
        }
    }

    this->result = std::move(result);
}

void Query::Destroy()
{
    assert(world);
    assert(terms);

    world->m_wAllocator.Free(termCount * sizeof(QueryTerm), terms);
    world->m_wAllocator.Free(termCount * sizeof(uint8_t), sortedTermIdx);

    result.Delete(world->m_wAllocator, callback.sigCount);
}

///////////////////////////////// Query Handle ////////////////////////////////

void QueryHandle::Execute()
{
    if (m_query)
    {
        if (m_eId == EcsInvalidId)
        {
            // ad-hoc filter
            m_query->Filter();
        }

        m_query->Execute();
    }
}

void QueryHandle::Destroy()
{
    if (m_query)
    {
        m_query->Destroy();

        if (m_eId != EcsInvalidId)
        {
            m_query->world->RemoveEntity(m_eId);
        }

        m_query->world->m_wAllocator.Free(sizeof(Query), m_query);
        m_query = nullptr;
    }
    else
    {
        assert(0);
    }
}

} // namespace ECS
