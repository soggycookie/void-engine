#pragma once

namespace ECS
{
using EntityId = uint64_t;
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

constexpr EntityId EcsInputManagerId = 100;
constexpr EntityId EcsTimeId = 101;

// Entity id support component

// run once
constexpr EntityId EcsOnBootId = 150;
constexpr EntityId EcsOnStartId = 151;

// run every frame
constexpr EntityId EcsOnLoopId = 152;
constexpr EntityId EcsOnValidationId = 153;
constexpr EntityId EcsOnStartFrameId = 154;
constexpr EntityId EcsOnPreUpdateId = 155;
constexpr EntityId EcsOnUpdateId = 156;
constexpr EntityId EcsOnPostUpdateId = 157;
constexpr EntityId EcsOnEndFrameId = 158;

constexpr const char *EcsOnBoot = "EcsOnBoot";
constexpr const char *EcsOnStart = "EcsOnStart";
constexpr const char *EcsOnLoop = "EcsOnLoop";
constexpr const char *EcsOnValidation = "EcsOnValidation";
constexpr const char *EcsOnStartFrame = "EcsOnStartFrame";
constexpr const char *EcsOnPreUpdate = "EcsOnPreUpdate";
constexpr const char *EcsOnUpdate = "EcsOnUpdate";
constexpr const char *EcsOnPostUpdate = "EcsOnPostUpdate";
constexpr const char *EcsOnEndFrame = "EcsOnEndFrame";

constexpr EntityId EcsAnyId = 199;

// internal components

constexpr const uint32_t EcsNameLength = 32;
constexpr const char *DefaultEntityName = "Entity %u";
constexpr const size_t MaxEntityNameLength = 32;
} // namespace ECS
