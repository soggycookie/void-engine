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
    EcsSystem() = default;
    EcsSystem(EcsSystem&& other) = default;

    Query *query;
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
