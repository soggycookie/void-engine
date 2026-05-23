#pragma once
#include "ecs_type.h"
#include "query.h"

namespace ECS
{
class Query;

struct EcsName
{
    char name[EcsNameLength];
};

struct EcsInherit
{
    void *data;
};

struct EcsQuery
{
    Query *query;
};

struct EcsSystem
{
    Query *query;
};

struct EcsTime
{
    double deltaTime;
    double elapsedTime;
    double realElapsedTime;
    double applicationTime;
};

// internal tag

struct EcsPhase
{
};

struct EcsArchetype
{
};

struct EcsPipeline
{
};

struct EcsComponent
{
};

struct EcsDisabled
{
};

// internal pair
struct EcsChildOf
{
};

struct EcsDependOn
{
};

struct EcsToggle
{
};

struct EcsIsA
{
};
} // namespace ECS
