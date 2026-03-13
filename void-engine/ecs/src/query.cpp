#include "query.h"
#include "ecs_type.h"
#include "world.h"

namespace ECS
{

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

    Store<Archetype *> filteredArchetypes;
    filteredArchetypes.Init(m_world->m_wAllocator, 4);

    for (size_t aIdx = 0; aIdx < cr.archetypeStore.count; ++aIdx)
    {
        Archetype *archetype = cr.archetypeStore[aIdx];
        const ComponentSet &cs = archetype->componentSet;
        bool isValid = true;

        for (size_t termIdx = 1; termIdx < m_termCount; ++termIdx)
        {
            const QueryTerm &filterdTerm = m_terms[termIdx];

            if (filterdTerm.op == HAS)
            {
                if (!cs.Has(filterdTerm.cId))
                {
                    isValid = false;
                    break;
                }
            }
            else if (filterdTerm.op == NOT)
            {
                if (cs.Has(filterdTerm.cId))
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
            filteredArchetypes.Add(archetype);
        }
    }

    result.filteredArchetypes = std::move(filteredArchetypes);

    // NOTE: filter entity here
    result.entityMask = nullptr;

    m_cache = std::move(result);
}

} // namespace ECS
