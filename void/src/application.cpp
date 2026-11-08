#include "application.h"
#include "file_system.h"
#include "game_layer.h"

#include "profiler.h"

namespace VoidEngine
{
Application *Application::s_Instance = nullptr;

bool Application::startup()
{
    running_ = true;
    resizing_ = false;
    vallocator_.init(gib(64), mib(4), gib(64), mib(4));

    window_ = Window::create(vallocator_, WindowProperty(), [this](InputEvent &e) { on_event(e); });
    Layer *game_layer = construct<GameLayer>(vallocator_, this);
    layer_stack_.push_layer(game_layer);

    // layer should before this because events dispatch to app
    //  then dispatch to layers, which should already exist
    if (!window_->init())
    {
        return false;
    }

    // vulkan_backend.StartUp(window, allocator);
    Profiler::StartUp(window_);
    job_system.startup(vallocator_, 0);

    FS::startup();
    // setup async io polling
    Job IOCompletionPollingJob;
    IOCompletionPollingJob.fn = FS::poll_io_completions;
    IOCompletionPollingJob.context = this;

    JobSystem::get().schedule(IOCompletionPollingJob, JobPriority::CRITICAL);
    FS::get_current_directory(vallocator_, pwd_);
    String resource_path;
    resource_path.init(vallocator_, 64);
    resource_path.assign(vallocator_, pwd_);
    resource_path.append(vallocator_, u8"\\resource");
    resource_system_.startup(vallocator_, resource_path.mut_data());
    resource_path.destroy(vallocator_);
    return true;
}

void Application::shutdown()
{
    running_ = false;
    resource_system_.shutdown(vallocator_);
    pwd_.destroy(vallocator_);
    FS::shutdown();
    job_system.shutdown();
    layer_stack_.destroy();
    for (uint32_t idx = 0; idx < layer_stack_.get_count(); ++idx) {
        destruct(vallocator_, layer_stack_[idx]);
    }
    // vulkan_backend.Shutdown();
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
        window_->update();

        for (uint32_t idx = layer_stack_.get_count(); idx > 0;)
        {
            Layer *layer = layer_stack_[--idx];
            layer->on_update(window_->get_delta_time());
        }

        for (uint32_t idx = layer_stack_.get_count(); idx > 0;)
        {
            Layer *layer = layer_stack_[--idx];
            layer->on_post_update();
        }
    }
}

// TODO: create layer to dispatch event to lower layers
void Application::on_event(InputEvent &e)
{
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
