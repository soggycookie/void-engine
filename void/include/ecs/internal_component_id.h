#pragma once

namespace VoidEngine::ECS
{
using EntityId = uint64_t;
using LowEntityId = uint32_t;
// component entity id
constexpr LowEntityId ENTITY_INVALID_ID = 0;
constexpr LowEntityId NAME_ID = 1;
constexpr LowEntityId SYSTEM_ID = 2;
constexpr LowEntityId PHASE_ID = 3;
constexpr LowEntityId EcsArchetypeId = 4;
constexpr LowEntityId CHILD_OF_ID = 5;
constexpr LowEntityId DEPEND_ON_ID = 6;
constexpr LowEntityId EcsPipelineId = 7;
constexpr LowEntityId EcsComponentId = 8;
constexpr LowEntityId QUERY_ID = 9;
constexpr LowEntityId EcsToggleId = 10;
constexpr LowEntityId EcsDisabledId = 11;
constexpr LowEntityId EcsInheritId = 12;
constexpr LowEntityId EcsIsAId = 13;

constexpr LowEntityId TIME_ID = 101;

// Entity id support component

// run once
constexpr LowEntityId ON_BOOT_ID = 150;
constexpr LowEntityId ON_START_ID = 151;

// run every frame
constexpr LowEntityId ON_LOOP_ID = 152;
constexpr LowEntityId ON_VALIDATION_ID = 153;
constexpr LowEntityId ON_START_FRAME_ID = 154;
constexpr LowEntityId ON_PRE_UPDATE_ID = 155;
constexpr LowEntityId ON_UPDATE_ID = 156;
constexpr LowEntityId ON_POST_UPDATE_ID = 157;
constexpr LowEntityId ON_END_FRAME_ID = 158;

constexpr const char *ON_BOOT = "On Boot";
constexpr const char *ON_START = "On Start";
constexpr const char *ON_LOOP = "On Loop";
constexpr const char *ON_VALIDATION = "On Validation";
constexpr const char *ON_START_FRAME = "On Start Frame";
constexpr const char *ON_PRE_UPDATE = "On Pre Update";
constexpr const char *ON_UPDATE = "On Update";
constexpr const char *ON_POST_UPDATE = "On Post Update";
constexpr const char *ON_END_FRAME = "On End Frame";

constexpr LowEntityId WILDCARD_ID = 199;

// internal components

constexpr const uint32_t EcsNameLength = 32;
constexpr const char *DefaultEntityName = "Entity %u";
constexpr const size_t MaxEntityNameLength = 32;
} // namespace VoidEngine::ECS
