#pragma once
#include "engine_config.h"
#include "input_event.h"
#include "input_manager.h"
#include "layer_stack.h"
#include "logger/log.h"
#include "window.h"

#include "job_system.h"
#include "profiler.h"
#include "resource_system.h"
#include "imgui_host.h"

// Forward declared so application.h does not pull in vulkan.h / windows.h.
namespace VoidEngine::Vulkan
{
class VulkanBackend;
}

namespace VoidEngine
{
class GameLayer;

class Application
{
private:
    static Application *s_Instance_;
    Vallocator vallocator_;

    EngineConfig config_;
    LayerStack layer_stack_;
    InputManager input_;
    Window *window_ = nullptr;

    // Owned layers, kept by real type so destruct() frees the right size.
    GameLayer *game_layer_ = nullptr;

    Vulkan::VulkanBackend *vulkan_ = nullptr;
    ImGuiHost imgui_host_;
    JobSystem job_system;
    ResourceSystem resource_system_;
    bool running_;
    bool resizing_;

public:
    Application()
    {
        ENGINE_ASSERT(!s_Instance_, "Can not create another instance of app!");
        s_Instance_ = this;
    }

    virtual ~Application() = default;

    bool startup();
    void shutdown();

    void update();

    Vallocator &get_allocator()
    {
        return vallocator_;
    }
    double get_delta_time() const
    {
        return window_->get_delta_time();
    }
    double get_app_time() const
    {
        return window_->get_window_time();
    }
    bool is_app_resizing() const
    {
        return resizing_;
    }

    Window *get_window() const
    {
        return window_;
    }

    const InputManager &get_input() const
    {
        return input_;
    }

    // void PushLayer(Layer* layer);
    // void PushOverLay(Layer* layer);

    static Application &get_app()
    {
        ENGINE_ASSERT(s_Instance_, "App instance is null");
        return *s_Instance_;
    }

    // VulkanBackend &get_vulkand_backend() { return vulkan_backend; }
private:
    void on_event(InputEvent &e);
};
} // namespace VoidEngine
