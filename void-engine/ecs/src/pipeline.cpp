#include "pipeline.h"
#include "ecs_type.h"
#include "internal_component.h"
#include "query.h"
#include "type_info.h"
#include "world.h"
#include <cassert>
#include <cstddef>
#include <cstdint>

namespace ECS
{

void Pipeline::BuildFromBasePhase(EntityId basePhaseId)
{
    assert(world);

    if (world->HasComponent(basePhaseId, EcsPhaseId) &&
        !world->HasRelationship(basePhaseId, EcsDependOnId))
    {
        EntityId id = basePhaseId;
        EntityId rel = MakeRelationship(EcsDependOnId, id);
        bool loop = true;
        while (world->m_componentIndex.ContainsKey(rel) && loop)
        {
            loop = false;
            ComponentRecord &cr = world->m_componentIndex[rel];

            PipelineStage stage;
            // SHOULD ONLY HAVE 2 ARCHETYPES HERE
            for (size_t idx = 0; idx < cr.archetypeStore.count; ++idx)
            {
                Archetype *archetype = cr.archetypeStore[idx];
                int32_t cIdx = archetype->componentSet.Search(EcsSystemId);

                if (cIdx != ComponentSet::NotFoundIdx)
                {
                    int32_t colIdx = archetype->componentMap[cIdx];

                    assert(colIdx != ComponentSet::NotFoundIdx);

                    Column &col = archetype->columns[colIdx];
                    TypeInfo *ti = col.typeInfo;

                    assert(ti);

                    for (size_t eIdx = 0; eIdx < archetype->count; ++eIdx)
                    {
                        EcsSystem *system =
                            PTR_CAST(OFFSET_ELEMENT(col.data, ti->size, eIdx),
                                     EcsSystem);

                        stage.systems.Add(world->m_wAllocator, system->query);
                    }
                }
                else
                {
                    // must have only 1 Phase entity depend on
                    if (archetype->componentSet.Has(EcsPhaseId))
                    {
                        loop = true;
                        id = archetype->entities[0];
                        rel = MakeRelationship(EcsDependOnId, id);
                        continue;
                    }
                    else
                    {
                        assert(0);
                    }
                }

                stages.Add(world->m_wAllocator, stage);
            }


        }
    }
    else
    {
        assert(0 && "Entity is not base phase! Base phase has no DependOn "
                    "relationship and have EcsPhase Tag");
    }
}

void Pipeline::Progress()
{
    for (size_t idx = 0; idx < stages.count; ++idx)
    {
        PipelineStage &stage = stages[idx];
        for (size_t sIdx = 0; sIdx < stage.systems.count; ++sIdx)
        {
            stage.systems[sIdx]->Execute();
        }
    }
}

void Pipeline::Destroy()
{
    for (size_t idx = 0; idx < stages.count; ++idx)
    {
        PipelineStage &stage = stages[idx];
        stage.systems.Destroy(world->m_wAllocator);
    }

    stages.Destroy(world->m_wAllocator);
}

///////////////// PhaseDependencyBuilder //////////////////

PhaseDependencyBuilder::PhaseDependencyBuilder(World* world, EntityId baseId)
    : m_baseId(baseId), m_activePhaseId(baseId), m_world(world)
{
    if (!m_world->HasComponent(m_baseId, EcsPhaseId) ||
        m_world->HasRelationship(m_baseId, EcsDependOnId))
    {
        assert(0);
    }
}

PhaseDependencyBuilder &PhaseDependencyBuilder::DependedPhase(EntityId eId,
                                                              const char *name)
{
    Entity e = m_world->CreateEntity(eId, name);
    e.AddTag<EcsPhase>().AddRelationship<EcsDependOn>(m_activePhaseId);
    m_activePhaseId = e.GetFullId();

    return *this;
}

} // namespace ECS
