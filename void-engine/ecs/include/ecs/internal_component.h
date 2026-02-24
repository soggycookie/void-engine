#pragma once
#include "ecs_type.h"
#include "system_meta.h"

namespace ECS
{
    //component entity id
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

    //flags
    constexpr EntityId EcsAnyId = 100;

    //internal components
    struct EcsName
    {
        EcsName() = default;

        char name[16];
    };
    ECS_COMPONENT(EcsName);


    struct EcsInherit
    {
        void* data;
    };
    ECS_COMPONENT(EcsInherit)

    //internal tag
    struct EcsSystem
    {         
    };

    ECS_COMPONENT(EcsSystem);

    struct EcsPhase
    {
    };
    ECS_COMPONENT(EcsPhase);

    struct EcsArchetype
    {
    };
    ECS_COMPONENT(EcsArchetype);

    struct EcsPipeline
    {
    };
    ECS_COMPONENT(EcsPipeline);

    struct EcsComponent
    {
    };
    ECS_COMPONENT(EcsComponent);
    
    struct EcsQuery
    {
    };
    ECS_COMPONENT(EcsQuery);

    struct EcsDisabled
    {
    };
    ECS_COMPONENT(EcsDisabled)

    //internal pair
    struct EcsChildOf
    {
    };
    ECS_COMPONENT(EcsChildOf);

    struct EcsDependOn
    {
    };
    ECS_COMPONENT(EcsDependOn);

    struct EcsToggle
    {
    };
    ECS_COMPONENT(EcsToggle);

    struct EcsIsA
    {
    };
    ECS_COMPONENT(EcsIsA)
}