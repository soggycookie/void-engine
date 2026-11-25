#include "application.h"
#include "file_system.h"
#include "game_layer.h"
#include "platform/vulkan/vulkan_backend.h"

#include "profiler.h"

namespace VoidEngine
{
Application *Application::s_Instance_ = nullptr;

bool Application::startup()
{
    running_ = true;
    resizing_ = false;
    vallocator_.init(gib(64), mib(4), gib(64), mib(4));

    window_ = Window::create(vallocator_, WindowProperty(), [this](InputEvent &e) { on_event(e); });

    game_layer_ = construct<GameLayer>(vallocator_, this);
    layer_stack_.push_layer(game_layer_);

    // layer should before this because events dispatch to app
    //  then dispatch to layers, which should already exist
    if (!window_->init())
    {
        return false;
    }
    // after init: the HWND does not exist before it (fatal message boxes use it as owner)
    Logger::set_window_handle(window_->get_window_handle());

    Profiler::StartUp(window_);
    // Also initializes the main thread's t_scratches, which Vulkan init uses: keep it before Vulkan.
    job_system.startup(vallocator_, 0);

    // Order: window -> Vulkan -> ImGui context + platform -> ImGui renderer (docs/design/imgui.md).
    vulkan_ = construct<Vulkan::VulkanBackend>(vallocator_);
    if (!vulkan_->startup(window_))
    {
        return false;
    }
    if (!imgui_host_.init(window_))
    {
        return false;
    }
    if (!vulkan_->init_imgui())
    {
        return false;
    }

    FS::startup();
    // setup async io polling
    Job IOCompletionPollingJob;
    IOCompletionPollingJob.fn_ = FS::poll_io_completions;
    IOCompletionPollingJob.context_ = this;

    JobSystem::get().schedule(IOCompletionPollingJob, JobPriority::CRITICAL);
    String pwd = FS::get_current_directory(vallocator_);
    String resource_path;
    resource_path.init(vallocator_, 64);
    resource_path.assign(vallocator_, pwd);
    resource_path.append(vallocator_, u8"\\resource");
    resource_system_.startup(resource_path.data());

    resource_path.destroy(vallocator_);
    pwd.destroy(vallocator_);
    return true;
}

void Application::shutdown()
{
    running_ = false;
    resource_system_.shutdown();
    FS::shutdown();
    job_system.shutdown();

    // Order: ImGui renderer (waits idle) -> layers -> ImGui context -> Vulkan -> window.
    if (vulkan_)
    {
        vulkan_->shutdown_imgui();
    }

    layer_stack_.destroy();
    if (game_layer_)
    {
        // By real type: Vallocator::vfree picks the pool from sizeof(T).
        destruct(vallocator_, game_layer_);
        game_layer_ = nullptr;
    }

    imgui_host_.destroy();

    if (vulkan_)
    {
        vulkan_->shutdown();
        destruct(vallocator_, vulkan_);
        vulkan_ = nullptr;
    }
    Window::destroy(vallocator_, window_);
    vallocator_.destroy();
}

void Application::update()
{
    for (uint32_t idx = layer_stack_.get_count(); idx > 0;)
    {
        layer_stack_[--idx]->on_init();
    }

    while (running_)
    {
        // clear per-frame edges before the pump so events pumped now are visible to on_update
        input_.begin_frame();
        window_->update();
        if (!running_)
        {
            break; // closed during the message pump: don't render into a closing window
        }

        // renderer new frame first, then platform + ImGui::NewFrame
        vulkan_->imgui_new_frame();
        imgui_host_.begin_frame();

        for (uint32_t idx = layer_stack_.get_count(); idx > 0;)
        {
            Layer *layer = layer_stack_[--idx];
            layer->on_update(window_->get_delta_time());
        }

        // bottom -> top, so overlays' windows are submitted last
        for (uint32_t idx = 0; idx < layer_stack_.get_count(); ++idx)
        {
            layer_stack_[idx]->on_imgui_render();
        }

        ImDrawData *ui = imgui_host_.end_frame();
        vulkan_->draw_frame(ui);

        for (uint32_t idx = layer_stack_.get_count(); idx > 0;)
        {
            Layer *layer = layer_stack_[--idx];
            layer->on_post_update();
        }
    }
}

void Application::on_event(InputEvent &e)
{
    // state is updated for every event, handled or not, so a layer swallowing a release can't
    // leave a key stuck
    input_.on_event(e);

    switch (e.get_type())
    {
    case InputEventType::APP_CLOSED:
    {
        running_ = false;
        break;
    }
    case InputEventType::APP_RESIZING:
    {
        break;
    }
    case InputEventType::APP_ENTER_RESIZE:
    {
        resizing_ = true;
        break;
    }
    case InputEventType::APP_EXIT_RESIZE:
    {
        resizing_ = false;
        break;
    }
    default:
        break;
    }

    for (uint32_t idx = layer_stack_.get_count(); idx > 0;)
    {
        layer_stack_[--idx]->on_event(e);
        if (e.is_handled())
        {
            break;
        }
    }
}
} // namespace VoidEngine
