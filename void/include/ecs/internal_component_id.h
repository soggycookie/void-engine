#pragma once

namespace VoidEngine::ECS
{
using EntityId = uint64_t;
using LowEntityId = uint32_t;
// component entity id
constexpr EntityId ENTITY_INVALID_ID = 0;
constexpr EntityId NAME_ID = 1;
constexpr EntityId SYSTEM_ID = 2;
constexpr EntityId PHASE_ID = 3;
constexpr EntityId EcsArchetypeId = 4;
constexpr EntityId CHILD_OF_ID = 5;
constexpr EntityId DEPEND_ON_ID = 6;
constexpr EntityId EcsPipelineId = 7;
constexpr EntityId EcsComponentId = 8;
constexpr EntityId QUERY_ID = 9;
constexpr EntityId EcsToggleId = 10;
constexpr EntityId EcsDisabledId = 11;
constexpr EntityId EcsInheritId = 12;
constexpr EntityId EcsIsAId = 13;

constexpr EntityId TIME_ID = 101;

// Entity id support component

// run once
constexpr EntityId ON_BOOT_ID = 150;
constexpr EntityId ON_START_ID = 151;

// run every frame
constexpr EntityId ON_LOOP_ID = 152;
constexpr EntityId ON_VALIDATION_ID = 153;
constexpr EntityId ON_START_FRAME_ID = 154;
constexpr EntityId ON_PRE_UPDATE_ID = 155;
constexpr EntityId ON_UPDATE_ID = 156;
constexpr EntityId ON_POST_UPDATE_ID = 157;
constexpr EntityId ON_END_FRAME_ID = 158;

constexpr const char *ON_BOOT = "On Boot";
constexpr const char *ON_START = "On Start";
constexpr const char *ON_LOOP = "On Loop";
constexpr const char *ON_VALIDATION = "On Validation";
constexpr const char *ON_START_FRAME = "On Start Frame";
constexpr const char *ON_PRE_UPDATE = "On Pre Update";
constexpr const char *ON_UPDATE = "On Update";
constexpr const char *ON_POST_UPDATE = "On Post Update";
constexpr const char *ON_END_FRAME = "On End Frame";

constexpr EntityId WILDCARD_ID = 199;

// internal components

constexpr const uint32_t EcsNameLength = 32;
constexpr const char *DefaultEntityName = "Entity %u";
constexpr const size_t MaxEntityNameLength = 32;
} // namespace VoidEngine::ECS
