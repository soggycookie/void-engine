#include "pipeline.h"
#include "ds/hash_map.h"
#include "ds/world_allocator.h"
#include "ecs_type.h"
#include "internal_component.h"
#include "query.h"
#include "type_info.h"
#include "world.h"
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace ECS
{

void Pipeline::BuildFromBasePhase(EntityId basePhaseId)
{
    assert(world);

    HashMap<EntityId, TermBehavior> behaviors;
    behaviors.Init(&world->m_wAllocator, 32);

    if (world->HasComponent(basePhaseId, EcsPhaseId) &&
        !world->HasRelationship(basePhaseId, EcsDependOnId))
    {
        EntityId id = basePhaseId;
        EntityId rel = MakeRelationship(EcsDependOnId, id);
        bool loop = true;
        while (world->m_componentRecordIndex.ContainsKey(rel) && loop)
        {
            loop = false;
            ComponentRecord &cr = world->m_componentRecordIndex[rel];

            Stage stage;
            // SHOULD ONLY HAVE 2 ARCHETYPES HERE
            for (size_t idx = 0; idx < cr.archetypes.count; ++idx)
            {
                Archetype *archetype = cr.archetypes[idx];
                int32_t cIdx = archetype->componentSet.Search(EcsSystemId);

                if (cIdx != ComponentSet::NotFoundIdx)
                {
                    int32_t colIdx = archetype->columnMap[cIdx];

                    assert(colIdx != ComponentSet::NotFoundIdx);

                    Column &col = archetype->columns[colIdx];
                    TypeInfo *ti = col.typeInfo;

                    assert(ti);

                    for (size_t eIdx = 0; eIdx < archetype->count; ++eIdx)
                    {
                        EcsSystem *system =
                            PTR_CAST(OFFSET_ELEMENT(col.data, ti->size, eIdx),
                                     EcsSystem);

                        BuildStep(behaviors, system, stage.steps,
                                  world->m_wAllocator);
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

        behaviors.Destroy();
    }
    else
    {
        assert(0 && "Entity is not base phase! Base phase has no DependOn "
                    "relationship and have EcsPhase Tag");
    }

    for (size_t idx = 0; idx < stages.count; ++idx)
    {
        std::cout << "Stage" << std::endl;
        for (size_t step = 0; step < stages[idx].steps.count; ++step)
        {
            if (stages[idx].steps[step].isSyncPoint)
            {
                std::cout << "Sync point inserted" << std::endl;
            }
            std::cout << "System" << std::endl;
        }
    }
}

void Pipeline::BuildStep(HashMap<EntityId, TermBehavior> &behaviors,
                         EcsSystem *system, Store<StageStep> &appendedStore,
                         WorldAllocator &wAllocator)
{
    assert(system);

    auto needSync = [&behaviors, system]() -> bool
    {
        Query &query = *system->query;
        for (size_t idx = 0; idx < query.termCount; ++idx)
        {
            QueryTerm &term = query.terms[idx];

            if (term.travMethod != TraverseMethod::SELF)
            {
                behaviors.Insert(
                    MakeRelationship(term.travRelation, term.travTarget),
                    TermBehavior::READ);
            }

            if (term.behavior == TermBehavior::READ)
            {
                if (behaviors.ContainsKey(term.cId) &&
                    behaviors[term.cId] == TermBehavior::WRITE)
                {
                    return true;
                }

                if (behaviors.ContainsKey(term.cId) &&
                    behaviors[term.cId] == TermBehavior::STRUCTURE_CHANGE)
                {
                    return true;
                }
            }
            else if (term.behavior == TermBehavior::WRITE)
            {
                if (behaviors.ContainsKey(term.cId))
                {
                    return true;
                }
            }
            else
            {
                if (behaviors.ContainsKey(term.cId))
                {
                    return true;
                }
            }
        }
        return false;
    };

    auto claim = [&behaviors, system]()
    {
        Query &query = *system->query;
        for (size_t idx = 0; idx < query.termCount; ++idx)
        {
            behaviors.Insert(query.terms[idx].cId, query.terms[idx].behavior);
        }
    };

    if (needSync())
    {
        // add sync point
        appendedStore.Add(wAllocator, StageStep{system->query, true});
        behaviors.Clear();
    }
    else
    {
        appendedStore.Add(wAllocator, StageStep{system->query, false});
    }

    // add term behavior to monitored hash map
    claim();
}

void Pipeline::Progress()
{
    for (size_t idx = 0; idx < stages.count; ++idx)
    {
        Stage &stage = stages[idx];
        for (size_t sIdx = 0; sIdx < stage.steps.count; ++sIdx)
        {
            StageStep &step = stage.steps[sIdx];
            if (step.isSyncPoint)
            {
                world->FlushDeferredCmd();
            }
            step.system->Execute();
        }
    }
}

void Pipeline::Destroy()
{
    for (size_t idx = 0; idx < stages.count; ++idx)
    {
        Stage &stage = stages[idx];
        stage.steps.Destroy(world->m_wAllocator);
    }

    stages.Destroy(world->m_wAllocator);
}

///////////////// PhaseDependencyBuilder //////////////////

PhaseDependencyBuilder::PhaseDependencyBuilder(World *world, EntityId baseId)
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
    Entity e = m_world->CreateEntity(eId, name)
                   .AddTag<EcsPhase>()
                   .AddRelationship<EcsDependOn>(m_activePhaseId)
                   .Build();
    m_activePhaseId = e.GetFullId();

    return *this;
}

} // namespace ECS
