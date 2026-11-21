#pragma once

#ifdef VOID_WIN32
#define NOMINMAX
#include <windows.h>

#include "window.h"

namespace VoidEngine
{

#define HR(hr)                                                                                          \
    if (FAILED(hr))                                                                                     \
    {                                                                                                   \
        return false;                                                                                   \
    }

class Win32_Window : public Window
{
public:
    Win32_Window(const WindowProperty &property, EventCallback func)
        : Window(property, func), curr_count_(0), prev_count_(0), hwnd_(nullptr), hinstance_(nullptr)
    {
        QueryPerformanceFrequency(&count_per_sec_);
    }

    void update() override;

    //TODO: move this to profiler
    void begin_time_elapse() override;
    void end_time_elapse(double &outPassedTime) override;

    void *get_window_handle() override
    {
        return hwnd_;
    }

    ClientDimension get_framebuffer_size() const override
    {
        RECT rect;
        GetClientRect(hwnd_, &rect);
        return ClientDimension{.width = (rect.right - rect.left), .height = (rect.bottom - rect.top)};
    }

    HWND get_hwnd()
    {
        return hwnd_;
    }

    HINSTANCE get_hinstance()
    {
        return hinstance_;
    }

private:
    bool init() override;

private:
    HWND hwnd_;
    HINSTANCE hinstance_;
    LARGE_INTEGER count_per_sec_;

    int64_t curr_count_;
    int64_t prev_count_;

    int64_t stop_watch_curr_count_;
    int64_t stop_watch_prev_count_;
};

} // namespace VoidEngine
#endif // VOID_WIN32
