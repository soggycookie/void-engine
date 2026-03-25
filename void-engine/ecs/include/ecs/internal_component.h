#pragma once
#include "ecs_type.h"
#include <bitset>
#include <cstdint>

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

//////////////// Input Manager ///////////////

struct MousePos
{
    MousePos() = default;

    int32_t mouseX;
    int32_t mouseY;
};

class EcsInputManager
{
public:
    EcsInputManager() : m_mousePos()
    {
        m_prevInputState.reset();
        m_currInputState.reset();
    }

    bool IsBtnPressed(char btn) { return true; }
    bool IsBtnReleased(char btn) { return true; }
    bool IsBtnHeld(char btn) { return true; }

    MousePos GetMousePos() const { return m_mousePos; }

    void Set(const std::bitset<256> &prev, const std::bitset<256> &curr,
             int32_t mouseX, int32_t mouseY)
    {
        m_prevInputState = prev;
        m_currInputState = curr;
        m_mousePos.mouseX = mouseX;
        m_mousePos.mouseY = mouseY;
    }

private:
    std::bitset<256> m_prevInputState;
    std::bitset<256> m_currInputState;
    MousePos m_mousePos;
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
