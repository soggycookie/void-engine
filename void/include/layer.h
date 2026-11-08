#pragma once
#include "input_event.h"
#include "input_manager.h"
#include "pch.h"

namespace VoidEngine
{
class Application;

class Layer
{
public:
    Layer(Application *app) : app(app), input_manager() {}

    virtual ~Layer() = default;
    virtual void on_init() = 0;
    virtual void on_detach() = 0;
    virtual void on_attach() = 0;

    virtual void on_update(double dt) = 0;
    virtual void on_event(InputEvent &e) { input_manager.add_event(e); };
    virtual void on_post_update() { input_manager.flush_state(); }

    const InputManager &get_input() const { return input_manager; }
protected:
    InputManager input_manager;
    Application *app;
};

} // namespace VoidEngine
