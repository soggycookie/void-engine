#pragma once
#include "query.h"

namespace VoidEngine::ECS
{
class Query_T;

struct Name
{
    char name[EcsNameLength];
};

struct EcsInherit
{
    void *data;
};

struct Query
{
    Query_T *query;
};

struct System
{
    Query_T *query;
};

struct Time
{
    double delta_time;
    double elapsed_time;
    double real_elapsed_time;
    double application_time;
    double time_multi;
};

// internal tag

struct Phase
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

struct ChildOf
{
};

struct DependOn
{
};

struct EcsToggle
{
};

struct EcsIsA
{
};
} // namespace VoidEngine::ECS
