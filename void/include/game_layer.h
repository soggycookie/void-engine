#pragma once
#include "layer.h"
#include "input_manager.h"
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

// ECS singleton view of the application's shared InputManager (no copy of the state).
class EcsInputManager
{
public:
    bool is_btn_pressed(KeyCode btn) const { return input_ && input_->is_pressed(btn); }
    bool is_btn_released(KeyCode btn) const { return input_ && input_->is_released(btn); }
    bool is_btn_held(KeyCode btn) const { return input_ && input_->is_down(btn); }

    MousePos get_mouse_pos() const { return input_ ? input_->get_mouse_pos() : MousePos(0, 0); }

    void set(const InputManager *input) { input_ = input; }

private:
    const InputManager *input_ = nullptr;
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
    void on_imgui_render() override;

private:
    bool paused = false;
    ECS::World *world;
};
} // namespace VoidEngine
