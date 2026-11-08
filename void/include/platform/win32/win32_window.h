#pragma once

#ifdef VOID_WIN32
#define NOMINMAX
#include <windows.h>

#include "window.h"

namespace VoidEngine
{

#define HR(hr)                                                                                     \
    if (FAILED(hr))                                                                                \
    {                                                                                              \
        return false;                                                                              \
    }

class Win32_Window : public Window
{
public:
    Win32_Window(const WindowProperty &property, EventCallback func)
        : Window(property, func), curr_count_(0), prev_count_(0), window_handle_(nullptr),
          hinstance_(nullptr)
    {
        QueryPerformanceFrequency(&count_per_sec_);
    }

    void update() override;

    // move this to profiler
    void begin_time_elapse() override;
    void end_time_elapse(double &outPassedTime) override;

    void *get_display_window() override;

    void *get_window_handle() const override { return window_handle_; }

    ClientDimension get_framebuffer_size() const override
    {
        RECT rect;
        GetClientRect(window_handle_, &rect);

        return ClientDimension{.width = (rect.right - rect.left),
                               .height = (rect.bottom - rect.top)};
    }

    HINSTANCE get_module_instance_handle() const { return hinstance_; }

    HWND get_native_handle() const { return window_handle_; }

private:
    bool init() override;

private:
    HWND window_handle_;
    HINSTANCE hinstance_;
    LARGE_INTEGER count_per_sec_;

    int64_t curr_count_;
    int64_t prev_count_;

    int64_t stop_watch_curr_count_;
    int64_t stop_watch_prev_count_;
};

} // namespace VoidEngine
#endif //VOID_WIN32
