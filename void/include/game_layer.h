#pragma once
#include "layer.h"
#include "ecs/ecs.h"

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

struct Observer
{
};

//////////////// Input Manager ///////////////

class EcsInputManager
{
public:
    EcsInputManager() : m_mousePos()
    {
        prev_input_state.reset();
        curr_input_state.reset();
    }

    bool is_btn_pressed(KeyCode btn)
    {
        return prev_input_state[static_cast<uint16_t>(btn)] == 0 &&
               curr_input_state[static_cast<uint16_t>(btn)] == 1;
    }

    bool is_btn_released(KeyCode btn)
    {
        return prev_input_state[static_cast<uint16_t>(btn)] == 1 &&
               curr_input_state[static_cast<uint16_t>(btn)] == 0;
    }

    bool is_btn_held(KeyCode btn)
    {
        return prev_input_state[static_cast<uint16_t>(btn)] == 1 &&
               curr_input_state[static_cast<uint16_t>(btn)] == 1;
    }

    MousePos get_mouse_pos() const { return m_mousePos; }

    void set(const std::bitset<256> &prev, const std::bitset<256> &curr, int32_t mouse_x,
             int32_t mouse_y)
    {
        prev_input_state = prev;
        curr_input_state = curr;
        m_mousePos.mouseX = mouse_x;
        m_mousePos.mouseY = mouse_y;
    }

private:
    std::bitset<256> prev_input_state;
    std::bitset<256> curr_input_state;
    MousePos m_mousePos;
};

class GameLayer : public Layer
{
public:
    GameLayer(Application *app) : Layer(app) {}

    void on_init() override;
    void on_detach() override;
    void on_attach() override;
    void on_update(double dt) override;
    void on_event(InputEvent &e) override;
    void on_post_update() override;

private:
    bool paused = false;
    ECS::World *world;
};
} // namespace VoidEngine
