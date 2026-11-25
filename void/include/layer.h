#pragma once
#include "input_event.h"
#include "pch.h"

namespace VoidEngine
{
class Application;
class InputManager;

class Layer
{
public:
    Layer(Application *app) : app_(app) {}

    virtual ~Layer() = default;
    virtual void on_init() = 0;
    virtual void on_detach() = 0;
    virtual void on_attach() = 0;

    virtual void on_update(double dt) = 0;
    // Event routing only (e.g. UI marking an event handled). Input state lives in
    // Application::get_input(), which is updated before layers see the event.
    virtual void on_event(InputEvent &e) {}
    virtual void on_post_update() {}
    // Called between ImGuiHost begin_frame/end_frame, bottom layer first. ImGui calls only here.
    virtual void on_imgui_render() {}

protected:
    // The application's shared input state (state and per-frame event buffer).
    const InputManager &input() const;

    Application *app_;
};

} // namespace VoidEngine
