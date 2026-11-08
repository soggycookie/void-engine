#pragma once
#include "input_event.h"

namespace VoidEngine
{
struct ClientDimension
{
public:
    int width;
    int height;
};

using EventCallback = std::function<void(InputEvent &)>;

struct WindowProperty
{
    const char *title_ = "void-engine";
    uint32_t width_ = 1280;
    uint32_t height_ = 720;
    uint32_t min_width_ = 800;
    uint32_t min_height_ = 600;
};

class Vallocator;

class Window
{
public:
    // define at platform-dependent layer
    static Window *create(Vallocator &vallocator, const WindowProperty &property, EventCallback func);
    static void destroy(Vallocator &vallocator, Window *window);

    Window(const WindowProperty &property, EventCallback func)
        : property_(property), dispatch_event_callback_(func), delta_time_(0), window_time_(0),
          is_dragging_(false)
    {
    }

    virtual ~Window() = default;

    virtual void update() = 0;

    virtual bool init() = 0;
    virtual void begin_time_elapse() = 0;
    virtual void end_time_elapse(double &out_time) = 0;
    virtual void *get_display_window() = 0;
    virtual void *get_window_handle() const = 0;
    virtual ClientDimension get_framebuffer_size() const = 0;

    double get_delta_time() const
    {
        return delta_time_;
    }

    void dispatch_input_event(InputEvent &e)
    {
        dispatch_event_callback_(e);
    }

    double get_window_time() const
    {
        return window_time_;
    }

    const WindowProperty &get_window_property() const
    {
        return property_;
    }

    void set_width(uint32_t width)
    {
        property_.width_ = width;
    }
    void set_height(uint32_t height)
    {
        property_.height_ = height;
    }

protected:
    // TODO: move this property to APP
    WindowProperty property_;
    EventCallback dispatch_event_callback_;

    // TODO: move this to another class
    double delta_time_;
    double window_time_;

    // TODO: handle case the app is paused when dragging
    bool is_dragging_;
};

} // namespace VoidEngine
