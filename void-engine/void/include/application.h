#pragma once

#include "ds/dynamic_array.h"
#include "engine_config.h"
#include "game_layer.h"
#include "input_event.h"
#include "layer_stack.h"
#include "log.h"
#include "pch.h"
#include "window.h"

namespace VoidEngine
{
class Application
{
public:
    Application()
    {
        LOG_ASSERT(s_instance == nullptr, "Can not create another instance of app!");
        s_instance = this;
    }

    virtual ~Application() = default;

    bool StartUp();
    void ShutDown();

    void Update();

    bool IsAppRunning() const { return m_isRunning; }

    double GetDeltaTime() const { return m_window->GetDeltaTime(); }

    double GetApplicationTime() const { return m_window->GetWindowTime(); }

    Window *GetWindow() const { return m_window; }

    // void PushLayer(Layer* layer);
    // void PushOverLay(Layer* layer);

    static Application &GetApp()
    {
        LOG_ASSERT(s_instance, "App instance is null");
        return *s_instance;
    }

    bool IsResizing() const { return m_isResizing; }

private:
    void OnEvent(InputEvent &e);

private:
    static Application *s_instance;

    LayerStack *m_layerStack;
    Window *m_window;
    GameLayer *m_gameLayer;
    EngineConfig m_config;
    bool m_isRunning = true;
    bool m_isResizing = false;
};
} // namespace VoidEngine
