#pragma once
#include "ecs_pch.h"
#include "ecs_type.h"

namespace ECS
{
struct PipelineStage
{
    Store<EntityId> systemsIds;
};

struct Pipeline
{
    Store<PipelineStage> stages;
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

struct EntityCommand
{
    EntityId id;
    AddCommand addCmd;
    RemoveCommand removeCmds;
};

} // namespace ECS
