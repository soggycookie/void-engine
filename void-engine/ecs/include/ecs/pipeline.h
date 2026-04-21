#pragma once
#include "ds/hash_map.h"
#include "ds/world_allocator.h"
#include "ecs_pch.h"
#include "ecs_type.h"
#include "internal_component.h"
#include "query.h"
#include "type_info.h"

namespace ECS
{

////////////////////////// PipelineStage ///////////////////////////

struct StageStep
{
    Query *system;
    bool isSyncPoint;
};

struct Stage
{
    Store<StageStep> steps;
};

////////////////////////// Pipeline ///////////////////////////

struct Pipeline
{
    Pipeline() = default;

    void Init(World *world) { this->world = world; }

    void BuildFromBasePhase(EntityId basePhaseId);

    void BuildStep(HashMap<EntityId, TermBehavior> &behaviors,
                   EcsSystem *system, Store<StageStep> &appendedStore,
                   WorldAllocator &wAllocator);

    void Progress();

    void Destroy();

    Store<Stage> stages;
    World *world;
};

////////////////////// PhaseDependencyBuilder ///////////////////////

class PhaseDependencyBuilder
{
public:
    PhaseDependencyBuilder(World *world, EntityId baseId);

    PhaseDependencyBuilder &DependedPhase(EntityId eId, const char *name);

private:
    EntityId m_baseId;
    EntityId m_activePhaseId;
    World *m_world;
};

} // namespace ECS
