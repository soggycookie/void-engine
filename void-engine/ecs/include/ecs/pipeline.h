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

struct AddCommand
{
    EntityId cId;
    void *data;
};

struct RemoveCommand
{
    EntityId cId;
};

using CmdMode = uint32_t;
constexpr const uint32_t AddCmdMode = 1;
constexpr const uint32_t RemoveCmdMode = 0;

struct EntityDeferredCommand
{
    EntityId id;
    AddCommand addCmd;
    RemoveCommand removeCmds;
    CmdMode mode;
    TypeInfo *typeInfo;
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
