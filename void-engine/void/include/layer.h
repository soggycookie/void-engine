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
    Layer(Application* app) : m_app(app), m_inputManager() {}

    virtual ~Layer() = default;
    virtual void OnInit() = 0;
    virtual void OnDetach() = 0;
    virtual void OnAttach() = 0;

    virtual void OnUpdate(double dt) = 0;
    virtual void OnEvent(InputEvent &e) { m_inputManager.AddEvent(e); };
    virtual void OnEndFrame() { m_inputManager.Clear(); }

    const InputManager &Input() const { return m_inputManager; }

private:
    InputManager m_inputManager;
protected:
    const Application* m_app;
};

} // namespace VoidEngine
