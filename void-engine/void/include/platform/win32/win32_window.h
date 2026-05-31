#pragma once

#define NOMINMAX
#include <windows.h>

#include "window.h"

namespace VoidEngine
{

#define HR(hr)                                                                 \
    if (FAILED(hr))                                                            \
    {                                                                          \
        return false;                                                          \
    }

class Win32_Window : public Window
{
public:
    Win32_Window(const WindowProperty &property, EventCallback func)
        : Window(property, func), m_currCount(0), m_prevCount(0),
          m_windowHandle(nullptr), m_hInstance(nullptr)
    {
        QueryPerformanceFrequency(&m_countsPerSec);
    }

    void Update() override;

    // move this to profiler
    void BeginTimeElapse() override;
    void EndTimeElapse(double &outPassedTime) override;

    void *GetDisplayWindow() override;

    void *GetWindowHandle() const override { return m_windowHandle; }

    ClientDimension GetFramebufferSize() const override
    {
        RECT rect;
        GetClientRect(m_windowHandle, &rect);

        return ClientDimension{.width = (rect.right - rect.left),
                               .height = (rect.bottom - rect.top)};
    }

    HINSTANCE GetModuleInstanceHandle() const { return m_hInstance; }

    HWND GetNativeWindowHandle() const { return m_windowHandle; }

private:
    bool Init() override;
    // bool SetupRenderer() override;

    void SetUpScene();

private:
    HWND m_windowHandle;
    HINSTANCE m_hInstance;
    LARGE_INTEGER m_countsPerSec;

    int64_t m_currCount;
    int64_t m_prevCount;

    int64_t m_stopWatchCurrCount;
    int64_t m_stopWatchPrevCount;
};

} // namespace VoidEngine
