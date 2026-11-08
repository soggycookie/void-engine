#pragma once
#include "engine_config.h"
#include "input_event.h"
#include "layer_stack.h"
#include "logger/log.h"
#include "window.h"

#include "job_system.h"
#include "profiler.h"
#include "resource_system.h"
// #include "vulkan_backend.h"

namespace VoidEngine
{

class Application
{
private:
    static Application *s_Instance;
    Vallocator vallocator_;

    EngineConfig config_;
    LayerStack layer_stack_;
    Window *window_;

    // VulkanBackend vulkan_backend;
    JobSystem job_system;
    ResourceSystem resource_system_;
    String pwd_;
    bool running_;
    bool resizing_;

public:
    Application()
    {
        ENGINE_ASSERT(!s_Instance, "Can not create another instance of app!");
        s_Instance = this;
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

    // void PushLayer(Layer* layer);
    // void PushOverLay(Layer* layer);

    static Application &get_app()
    {
        ENGINE_ASSERT(s_Instance, "App instance is null");
        return *s_Instance;
    }

    // VulkanBackend &get_vulkand_backend() { return vulkan_backend; }
private:
    void on_event(InputEvent &e);
};
} // namespace VoidEngine
