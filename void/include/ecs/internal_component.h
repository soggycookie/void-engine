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
    Query_T *query_;

    Query() : query_(nullptr)
    {
    }

    Query(Query_T *q) : query_(q)
    {
    }

    NO_COPY_CTOR(Query);
    NO_COPY_ASSIGNMENT(Query);
};

struct System
{
    Query_T *query_;

    System() : query_(nullptr)
    {
    }

    System(Query_T *q) : query_(q)
    {
    }

    NO_COPY_CTOR(System);
    NO_COPY_ASSIGNMENT(System);
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
