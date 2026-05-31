#pragma once

#include "input_event.h"
#include "pch.h"

namespace VoidEngine
{
using EventCallback = std::function<void(InputEvent &)>;

struct WindowProperty
{
    const char *title = "void-engine";
    uint32_t width = 1280;
    uint32_t height = 720;
    uint32_t minWidth = 800;
    uint32_t minHeight = 600;
};

class Window
{
public:
    // define at platform-dependent layer
    static Window *Create(const WindowProperty &property, EventCallback func);

    Window(const WindowProperty &property, EventCallback func)
        : m_property(property), m_eventCallback(func), m_deltaTime(0),
          m_windowTime(0), m_isTimeStopped(false)
    {
    }

    virtual ~Window() = default;

    virtual void Update() = 0;

    virtual bool Init() = 0;
    virtual void BeginTimeElapse() = 0;
    virtual void EndTimeElapse(double &outPassedTime) = 0;
    virtual void *GetDisplayWindow() = 0;
    virtual void *GetWindowHandle() const = 0;
    virtual ClientDimension GetFramebufferSize() const = 0;

    double GetDeltaTime() const { return m_deltaTime; }

    void DispatchInputEvent(InputEvent &e) { m_eventCallback(e); }

    double GetWindowTime() const { return m_windowTime; }

    const WindowProperty &GetWindowProperty() const { return m_property; }

    void SetWidth(uint32_t width) { m_property.width = width; }
    void SetHeight(uint32_t height) { m_property.height = height; }

protected:
    // TODO: move this property to APP
    WindowProperty m_property;
    EventCallback m_eventCallback;

    // TODO: move this to another class
    double m_deltaTime;
    double m_windowTime;

    // TODO: handle case the app is paused when dragging
    bool m_isTimeStopped;
};

} // namespace VoidEngine
