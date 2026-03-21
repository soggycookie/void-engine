#pragma once

#include "ds/dynamic_array.h"
#include "engine_config.h"
#include "game_layer.h"
#include "input_event.h"
#include "layer_stack.h"
#include "pch.h"
#include "window.h"

namespace VoidEngine
{
class Application
{
public:
    Application() = default;

    virtual ~Application() = default;

    bool StartUp();
    void ShutDown();

    void Update();

    bool IsAppRunning() const { return m_isRunning; }

    // void PushLayer(Layer* layer);
    // void PushOverLay(Layer* layer);

private:
    void OnEvent(InputEvent &e);

private:
    LayerStack *m_layerStack;
    Window *m_window;
    GameLayer *m_gameLayer;
    EngineConfig m_config;
    bool m_isRunning = true;
    bool m_isResizing = false;
};
} // namespace VoidEngine
