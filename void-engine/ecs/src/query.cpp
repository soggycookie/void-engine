#include "query.h"
#include "ds/world_allocator.h"
#include "ecs_type.h"
#include "ecs_utils.h"
#include "internal_component.h"
#include "world.h"
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace ECS
{

///////////////////////// Matched Archetype ////////////////////////////

void MatchedArchetype::SetMatchedColumns(WorldAllocator &wAllocator,
                                         int32_t *mappedCols, uint32_t count)
{
    int32_t countDiff = count - InlineArrayOptimizationCount;

    std::memcpy(smallMatchedColumns, mappedCols, count * sizeof(int32_t));

    if (countDiff > 0)
    {
        largeMatchedColumns =
            PTR_CAST(wAllocator.Alloc(countDiff * sizeof(int32_t)), int32_t);
        std::memcpy(largeMatchedColumns,
                    mappedCols + InlineArrayOptimizationCount,
                    countDiff * sizeof(int32_t));
    }
}

void MatchedArchetype::Delete(WorldAllocator &wAllocator,
                              uint32_t callbackSigCount)
{
    if (largeMatchedColumns)
    {
        assert(callbackSigCount > InlineArrayOptimizationCount);
        wAllocator.Free(sizeof(int32_t) *
                            (callbackSigCount - InlineArrayOptimizationCount),
                        largeMatchedColumns);
    }

    if (largeEntityMask)
    {
        constexpr const uint32_t numBitOfUint32 = 32;
        int32_t count = std::ceil(CAST(archetype->count, float) /
                                  CAST(numBitOfUint32, float));

        assert(count > InlineArrayOptimizationCount);

        wAllocator.Free(sizeof(uint32_t) *
                            (count - InlineArrayOptimizationCount),
                        largeEntityMask);
    }
}
//////////////////////////// QueryResult ////////////////////////////////

void QueryResult::Add(WorldAllocator &wAllocator, MatchedArchetype &&matched)
{
    constexpr const uint32_t defaultCapacity = 4;

    if (count < InlineArrayOptimizationCount)
    {
        smallMatchedArchetypes[count] = std::move(matched);
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

            if (largeMatchedArchetypes)
            {
                for (size_t idx = 0;
                     idx < (count - InlineArrayOptimizationCount); ++idx)
                {
                    new (&newMatched[idx]) MatchedArchetype(
                        std::move(largeMatchedArchetypes[idx]));
                }
            }

            largeMatchedArchetypes = newMatched;
            capacity = expandedCapa;
        }

        new (&largeMatchedArchetypes[count - InlineArrayOptimizationCount])
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
            smallMatchedArchetypes[idx].Delete(wAllocator, callbackSigCount);
        }
    }
    else
    {
        assert(largeMatchedArchetypes);

        for (size_t idx = 0; idx < count - InlineArrayOptimizationCount; ++idx)
        {
            largeMatchedArchetypes[idx].Delete(wAllocator, callbackSigCount);
            largeMatchedArchetypes[idx].~MatchedArchetype();
        }

        wAllocator.Free(sizeof(MatchedArchetype) * capacity,
                        largeMatchedArchetypes);
    }
}

/////////////////////////////// Query /////////////////////////////////

void Query::Execute() { callback.invoker(this, callback.fn, callback.ctx); }

void Query::Filter()
{
    assert(termCount > 0);
    QueryResult result;

    const QueryTerm &anchorTerm = terms[0];

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
        anchorTermId =
            MakeRelationship(anchorTerm.travRelation, anchorTerm.travTarget);
    }

    const ComponentRecord &cr = world->m_componentIndex[anchorTermId];

    for (size_t aIdx = 0; aIdx < cr.archetypeStore.count; ++aIdx)
    {
        Archetype *archetype = cr.archetypeStore[aIdx];
        const ComponentSet &cs = archetype->componentSet;
        bool isValid = true;

        for (size_t termIdx = 1; termIdx < termCount; ++termIdx)
        {
            const QueryTerm &term = terms[termIdx];

            switch (term.travMethod)
            {
            case ECS::SELF:
            {
                if (term.op == HAS)
                {
                    if (!cs.Has(term.cId))
                    {
                        isValid = false;
                    }
                }
                else if (term.op == NOT)
                {
                    if (cs.Has(term.cId))
                    {
                        isValid = false;
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

                if (term.op == HAS)
                {
                    if (!world->HasComponent(target, term.cId) &&
                        !world->HasRelationship(target, term.cId))
                    {
                        isValid = false;
                    }
                }
                else if (term.op == NOT)
                {
                    if (world->HasComponent(target, term.cId) ||
                        world->HasRelationship(target, term.cId))
                    {
                        isValid = false;
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

                if (term.op == HAS)
                {
                    while (true)
                    {
                        if (!world->HasComponent(target, term.cId) &&
                            !world->HasRelationship(target, term.cId))
                        {
                            if (!world->HasRelationship(
                                    target, term.travRelation, EcsAnyId))
                            {
                                isValid = false;
                                break;
                            }
                            else
                            {
                                EntityRecord *r =
                                    world->GetEntityRecord(target);
                                assert(r);

                                const ComponentSet &tempCs =
                                    r->archetype->componentSet;

                                target = HI_ENTITY_ID(
                                    tempCs[tempCs.SearchRelationship(
                                        relationship)]);
                            }
                        }
                        else
                        {
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
                            if (!world->HasRelationship(
                                    target, term.travRelation, EcsAnyId))
                            {
                                isValid = false;
                                break;
                            }
                            else
                            {
                                EntityRecord *r =
                                    world->GetEntityRecord(target);
                                assert(r);

                                const ComponentSet &tempCs =
                                    r->archetype->componentSet;

                                target = HI_ENTITY_ID(
                                    tempCs[tempCs.SearchRelationship(
                                        relationship)]);
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
        }

        if (isValid)
        {
            if (eId != EcsInvalidId)
            {
                archetype->trackedQuery.Add(world->m_wAllocator, eId);
            }

            MatchedArchetype ma(archetype);
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
