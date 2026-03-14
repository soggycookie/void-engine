#include "query.h"
#include "ds/world_allocator.h"
#include "ecs_type.h"
#include "ecs_utils.h"
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

void Query::Execute()
{
    m_callback.invoker(this, m_callback.fn, m_callback.ctx);
}

void Query::Filter()
{
    QueryResult result;

    const QueryTerm &anchorTerm = m_terms[0];

    if (anchorTerm.op == NOT)
    {
        assert(0 && "Query should not rely on only NOT operations!");
    }

    const ComponentRecord &cr = m_world->m_componentIndex[anchorTerm.cId];

    for (size_t aIdx = 0; aIdx < cr.archetypeStore.count; ++aIdx)
    {
        const Archetype *archetype = cr.archetypeStore[aIdx];
        const ComponentSet &cs = archetype->componentSet;
        bool isValid = true;

        for (size_t termIdx = 1; termIdx < m_termCount; ++termIdx)
        {
            const QueryTerm& term = m_terms[termIdx];

            if (term.op == HAS)
            {
                if (!cs.Has(term.cId))
                {
                    isValid = false;
                    break;
                }
            }
            else if (term.op == NOT)
            {
                if (cs.Has(term.cId))
                {
                    isValid = false;
                    break;
                }
            }
            else
            {
                assert(0);
            }
        }

        if (isValid)
        {
            MatchedArchetype ma(archetype);
            result.Add(m_world->m_wAllocator, std::move(ma));
        }
    }

    m_result = std::move(result);
}

void Query::Destroy()
{
    assert(m_world);
    assert(m_terms);

    m_world->m_wAllocator.Free(m_termCount * sizeof(QueryTerm), m_terms);

    m_result.Delete(m_world->m_wAllocator, m_callback.sigCount);
}

} // namespace ECS
