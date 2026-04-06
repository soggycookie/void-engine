#pragma once
#include "ecs.h"
#include "layer.h"
#include "resource.h"

namespace VoidEngine
{
struct Position
{
    double x, y;
};

struct Velocity
{
    float x, y;
};

struct Rotation
{
    float x, y;
};

struct NPC
{
};

//////////////// Input Manager ///////////////


class EcsInputManager
{
public:
    EcsInputManager() : m_mousePos()
    {
        m_prevInputState.reset();
        m_currInputState.reset();
    }

    bool IsBtnPressed(KeyCode btn)
    {
        assert(btn != KeyCode::NONE);
        return m_prevInputState[static_cast<uint16_t>(btn)] == 0 &&
               m_currInputState[static_cast<uint16_t>(btn)] == 1;
    }

    bool IsBtnReleased(KeyCode btn)
    {
        assert(btn != KeyCode::NONE);
        return m_prevInputState[static_cast<uint16_t>(btn)] == 1 &&
               m_currInputState[static_cast<uint16_t>(btn)] == 0;
    }

    bool IsBtnHeld(KeyCode btn)
    {
        assert(btn != KeyCode::NONE);
        return m_prevInputState[static_cast<uint16_t>(btn)] == 1 &&
               m_currInputState[static_cast<uint16_t>(btn)] == 1;
    }

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

class GameLayer : public Layer
{
public:
    GameLayer(Application* app) : m_gameTime(0), Layer(app) {}

    void OnInit() override;
    void OnDetach() override;
    void OnAttach() override;
    void OnUpdate(double dt) override;
    void OnEvent(InputEvent &e) override;
    void OnEndFrame() override;

private:
    uint64_t frameCount = 0;
    ECS::World *world;
    size_t m_gameTime;
    double elapsedTime;
    MeshResource *mesh;
    MaterialResource *material;
};
} // namespace VoidEngine
