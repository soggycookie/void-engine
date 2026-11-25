#pragma once
#include "utils.h"
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

    // Feed one UTF-16 unit from WM_CHAR. Returns the code point, or 0 when the unit is a high
    // surrogate waiting for its pair (characters outside the BMP arrive as two WM_CHARs).
    uint32_t decode_utf16(wchar_t unit)
    {
        if (IS_HIGH_SURROGATE(unit))
        {
            high_surrogate_ = unit;
            return 0;
        }
        if (IS_LOW_SURROGATE(unit))
        {
            const wchar_t high = high_surrogate_;
            high_surrogate_ = 0;
            if (high == 0)
            {
                return 0xFFFD; // lone low surrogate
            }
            return 0x10000 + ((SCAST(uint32_t, high) - 0xD800) << 10) +
                   (SCAST(uint32_t, unit) - 0xDC00);
        }
        high_surrogate_ = 0;
        return unit;
    }

    // Drop a half-received surrogate pair (e.g. on focus loss).
    void reset_text_state()
    {
        high_surrogate_ = 0;
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

    wchar_t high_surrogate_ = 0;
};

} // namespace VoidEngine
#endif // VOID_WIN32
