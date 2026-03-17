#pragma once
#include "ecs_type.h"

namespace ECS
{
class Query;

// component entity id
constexpr EntityId EcsInvalidId = 0;
constexpr EntityId EcsNameId = 1;
constexpr EntityId EcsSystemId = 2;
constexpr EntityId EcsPhaseId = 3;
constexpr EntityId EcsArchetypeId = 4;
constexpr EntityId EcsChildOfId = 5;
constexpr EntityId EcsDependOnId = 6;
constexpr EntityId EcsPipelineId = 7;
constexpr EntityId EcsComponentId = 8;
constexpr EntityId EcsQueryId = 9;
constexpr EntityId EcsToggleId = 10;
constexpr EntityId EcsDisabledId = 11;
constexpr EntityId EcsInheritId = 12;
constexpr EntityId EcsIsAId = 13;

// flags
constexpr EntityId EcsAnyId = 100;

// internal components

constexpr const uint32_t EcsNameLength = 32;
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
    EntityId queryId;
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
