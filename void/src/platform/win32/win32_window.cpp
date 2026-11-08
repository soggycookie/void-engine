#ifdef VOID_WIN32
#include "win32_window.h"
#include "vallocator.h"
// #include "renderer.h"
#include "window.h"

#include "input/win32_vk_mapper.h"
#include "input_event.h"

#include <winuser.h>
namespace VoidEngine
{
#define MIN_WINDOW_X 200
#define MIN_WINDOW_Y 200

static ClientDimension get_client_dimension(HWND hwnd)
{
    RECT client_rect;
    GetClientRect(hwnd, &client_rect);

    ClientDimension dimension;

    dimension.width = client_rect.right - client_rect.left;
    dimension.height = client_rect.bottom - client_rect.top;

    return dimension;
}

// Win32 window definition
Window *Window::create(Vallocator &vallocator, const WindowProperty &property, EventCallback func)
{
    return construct<Win32_Window>(vallocator, property, func);
}

void Window::destroy(Vallocator &vallocator, Window *window)
{
    destruct(vallocator, window);
}
/*
NOTE:

While your window procedure executes, it blocks any other messages for windows
created on the same thread. Therefore, avoid lengthy processing inside your
window procedure. For example, suppose your program opens a TCP connection and
waits indefinitely for the server to respond. If you do that inside the window
procedure, your UI will not respond until the request completes. During that
time, the window cannot process mouse or keyboard input, repaint itself, or even
close.
https://learn.microsoft.com/en-us/windows/win32/learnwin32/writing-the-window-procedure
*/

static LRESULT CALLBACK window_procedure(HWND hwnd, UINT u_msg, WPARAM wparam, LPARAM lparam)
{
    LRESULT result = 0;
    Window *window = reinterpret_cast<Window *>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

    switch (u_msg)
    {
    //////////////////////////////////////////////////////////////
    //                 APPLICATION EVENT
    //////////////////////////////////////////////////////////////
    case WM_NCCREATE:
    {
        CREATESTRUCT *cs = reinterpret_cast<CREATESTRUCT *>(lparam);
        window = static_cast<Window *>(cs->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
        result = DefWindowProc(hwnd, u_msg, wparam, lparam);
        break;
    }
    case WM_CLOSE:
    {
        InputEvent e(InputEventCategory::APPLICATION, InputEventType::APP_CLOSED, 0, 0);
        window->dispatch_input_event(e);

        break;
    }
    case WM_DESTROY:
    {
        // InputEvent e(InputEventCategory::APPLICATION,
        //              InputEventType::APP_CLOSED, 0, 0);
        // window->DispatchInputEvent(e);

        break;
    }
    case WM_SIZE:
    {
        int width = LOWORD(lparam);
        int height = HIWORD(lparam);

        window->set_width(width);
        window->set_height(height);

        InputEvent e(InputEventCategory::APPLICATION, InputEventType::APP_RESIZING, width, height);
        window->dispatch_input_event(e);

        break;
    }
    case WM_ENTERSIZEMOVE:
    {
        InputEvent e(InputEventCategory::APPLICATION, InputEventType::APP_ENTER_RESIZE, 0, 0);
        window->dispatch_input_event(e);
        break;
    }
    case WM_EXITSIZEMOVE:
    {

        ClientDimension dimension = get_client_dimension(hwnd);
        InputEvent e(InputEventCategory::APPLICATION, InputEventType::APP_EXIT_RESIZE, dimension.width,
                     dimension.height);
        window->dispatch_input_event(e);
        break;
    }
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        FillRect(hdc, &ps.rcPaint, (HBRUSH)(COLOR_WINDOW + 1));

        EndPaint(hwnd, &ps);
        break;
    }
    case WM_GETMINMAXINFO:
    {
        if (window)
        {
            ((MINMAXINFO *)lparam)->ptMinTrackSize.x = window->get_window_property().min_width_;
            ((MINMAXINFO *)lparam)->ptMinTrackSize.y = window->get_window_property().min_height_;
        }
        else
        {
            ((MINMAXINFO *)lparam)->ptMinTrackSize.x = MIN_WINDOW_X;
            ((MINMAXINFO *)lparam)->ptMinTrackSize.y = MIN_WINDOW_Y;
        }
        break;
    }
    case WM_MOVING:
    {
        // Renderer::EndFrame();
        break;
    }
    //////////////////////////////////////////////////////////////
    //                 KEYBOARD EVENT
    //////////////////////////////////////////////////////////////
    case WM_SYSKEYDOWN:
    case WM_KEYDOWN:
    {
        KeyCode button = Win32VKToKeyCode(wparam, lparam);
        InputEvent e(InputEventType::KEY_PRESSED, button);
        window->dispatch_input_event(e);
        break;
    }
    case WM_SYSKEYUP:
    case WM_KEYUP:
    {
        KeyCode button = Win32VKToKeyCode(wparam, lparam);
        InputEvent e(InputEventType::KEY_RELEASED, button);
        window->dispatch_input_event(e);
        break;
    }
    //////////////////////////////////////////////////////////////
    //                 MOUSE EVENT
    //////////////////////////////////////////////////////////////
    case WM_MOUSEMOVE:
    {

        // TODO: support modifier
        // relative to upper-left corner
        int32_t x = LOWORD(lparam);
        int32_t y = HIWORD(lparam);

        InputEvent e(InputEventType::MOUSE_MOVE, KeyCode::NONE, x, y);
        window->dispatch_input_event(e);

        break;
    }
    case WM_LBUTTONDOWN:
    {
        InputEvent e(InputEventType::MOUSE_PRESSED, KeyCode::MOUSE_LEFT, 0, 0);
        window->dispatch_input_event(e);

        break;
    }
    case WM_MBUTTONDOWN:
    {
        InputEvent e(InputEventType::MOUSE_PRESSED, KeyCode::MOUSE_MIDDLE, 0, 0);
        window->dispatch_input_event(e);

        break;
    }
    case WM_RBUTTONDOWN:
    {
        InputEvent e(InputEventType::MOUSE_PRESSED, KeyCode::MOUSE_RIGHT, 0, 0);
        window->dispatch_input_event(e);

        break;
    }
    case WM_LBUTTONUP:
    {
        InputEvent e(InputEventType::MOUSE_RELEASED, KeyCode::MOUSE_LEFT, 0, 0);
        window->dispatch_input_event(e);

        break;
    }
    case WM_MBUTTONUP:
    {
        InputEvent e(InputEventType::MOUSE_RELEASED, KeyCode::MOUSE_MIDDLE, 0, 0);
        window->dispatch_input_event(e);

        break;
    }
    case WM_RBUTTONUP:
    {
        InputEvent e(InputEventType::MOUSE_RELEASED, KeyCode::MOUSE_RIGHT, 0, 0);
        window->dispatch_input_event(e);

        break;
    }
    case WM_MOUSEWHEEL:
    {
        // TODO: handle rotate direction, distance

        InputEvent e(InputEventCategory::MOUSE, InputEventType::MOUSE_WHEEL_ROTATED, 0, 0);
        window->dispatch_input_event(e);

        break;
    }
    case WM_XBUTTONDOWN:
    {
        int16_t btn = HIWORD(wparam);
        KeyCode voidBtn;

        if (btn == XBUTTON1)
        {
            voidBtn = KeyCode::MOUSE_X1;
            std::cout << "x button 1" << std::endl;
        }
        else
        {
            voidBtn = KeyCode::MOUSE_X2;
            std::cout << "x button 2" << std::endl;
        }

        InputEvent e(InputEventType::MOUSE_PRESSED, voidBtn, 0, 0);
        window->dispatch_input_event(e);

        break;
    }
    case WM_XBUTTONUP:
    {
        int16_t btn = HIWORD(wparam);
        KeyCode voidBtn;

        if (btn == XBUTTON1)
        {
            voidBtn = KeyCode::MOUSE_X1;
        }
        else
        {
            voidBtn = KeyCode::MOUSE_X2;
        }

        InputEvent e(InputEventType::MOUSE_RELEASED, voidBtn, 0, 0);
        window->dispatch_input_event(e);

        break;
    }
    default:
    {
        result = DefWindowProc(hwnd, u_msg, wparam, lparam);

        break;
    }
    }
    return result;
}

static void register_window_class(HINSTANCE hInst, const char *className)
{
    WNDCLASSEX window_class;
    ZeroMemory(&window_class, sizeof(WNDCLASSEX));

    window_class.cbSize = sizeof(WNDCLASSEX);
    window_class.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
    window_class.hInstance = hInst;
    window_class.lpfnWndProc = window_procedure;
    window_class.lpszClassName = className;

    ATOM atom = RegisterClassEx(&window_class);
    if (atom == 0)
    {
        DWORD error = GetLastError();
        char buf[256];
        snprintf(buf, sizeof(buf), "RegisterClassEx failed! Error: %lu", error);
        MessageBoxA(NULL, buf, "Window Class Registration Failed", MB_OK | MB_ICONERROR);
    }
}

/*
propagate it to application layer -> dispatch to other layers
we should not bind events, do other work inside window
because it is platform-specific
*/
void Win32_Window::update()
{
    LARGE_INTEGER curr_count;
    QueryPerformanceCounter(&curr_count);
    curr_count_ = curr_count.QuadPart;

    if (prev_count_ == 0)
    {
        prev_count_ = curr_count_;
    }

    if (is_dragging_)
    {
        delta_time_ = 0;
    }
    else
    {
        delta_time_ =
            static_cast<double>(curr_count_ - prev_count_) / static_cast<double>(count_per_sec_.QuadPart);
    }

    prev_count_ = curr_count_;

    window_time_ += delta_time_;

    MSG msg;
    ZeroMemory(&msg, sizeof(MSG));

    while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    // std::cout << m_windowTime << std::endl;
}

void Win32_Window::begin_time_elapse()
{
    LARGE_INTEGER count;
    QueryPerformanceCounter(&count);
    stop_watch_prev_count_ = count.QuadPart;
}

void Win32_Window::end_time_elapse(double &out_time)
{
    LARGE_INTEGER count;
    QueryPerformanceCounter(&count);
    stop_watch_curr_count_ = count.QuadPart;

    int64_t delta_count = stop_watch_curr_count_ - stop_watch_prev_count_;
    out_time = SCAST(double, delta_count) / SCAST(double, count_per_sec_.QuadPart);
}

bool Win32_Window::init()
{
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    const char class_name[] = "VoidEngineWindowClass";
    hinstance_ = GetModuleHandle(NULL);
    register_window_class(hinstance_, class_name);

    constexpr UINT base_api = 96;
    UINT dpi = GetDpiForSystem();
    float scale = SCAST(float, dpi) / SCAST(float, base_api);
    property_.width_ *= scale;
    property_.height_ *= scale;

    window_handle_ =
        CreateWindowEx(0, class_name, "Void-engine", WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT,
                       CW_USEDEFAULT, property_.width_, property_.height_, NULL, NULL, hinstance_, this);

    // LOG_INFO("Win32", "Window: %u - %u", m_property.width, m_property.height);
    // LOG_INFO("Win32", "Rect: %u - %u", GetClientDimension(m_windowHandle).width,
    //          GetClientDimension(m_windowHandle).height);

    if (!window_handle_)
    {
        DWORD err = GetLastError();
        char msg[256];
        sprintf_s(msg, "CreateWindowEx failed: %lu\n", err);
        OutputDebugStringA(msg);
        MessageBoxA(0, msg, "Error", 0);
        return false;
    }

    return true;
}

void *Win32_Window::get_display_window()
{
    return SCAST(void *, window_handle_);
}

} // namespace VoidEngine

#endif // VOID_WIN32
