#pragma once
#include "ecs_pch.h"
#include "ecs_type.h"
#include "query.h"
#include "type_info.h"

namespace ECS
{

////////////////////////// PipelineStage ///////////////////////////

struct PipelineStage
{
    Store<Query *> systems;
};

////////////////////////// Pipeline ///////////////////////////

struct Pipeline
{
    void Init(World *world) { this->world = world; }

    void BuildFromBasePhase(EntityId basePhaseId);

    void Progress();

    void Destroy();

    Store<PipelineStage> stages;
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
