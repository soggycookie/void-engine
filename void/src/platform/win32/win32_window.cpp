#ifdef VOID_WIN32
#include "win32_window.h"
#include "vallocator.h"
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
    Window *window = RCAST(Window *, GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    // Raw hook first (ImGui). Engine events below still fire unless the hook consumed the message.
    if (window)
    {
        if (NativeMessageHook hook = window->get_native_message_hook())
        {
            intptr_t hook_result = hook(hwnd, u_msg, wparam, lparam);
            if (hook_result)
            {
                return hook_result;
            }
        }
    }

    switch (u_msg)
    {
    //////////////////////////////////////////////////////////////
    //                 APPLICATION EVENT
    //////////////////////////////////////////////////////////////
    case WM_NCCREATE:
    {
        CREATESTRUCTW *cs = RCAST(CREATESTRUCTW *, lparam);
        window = SCAST(Window *, cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, RCAST(LONG_PTR, window));
        result = DefWindowProcW(hwnd, u_msg, wparam, lparam);
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
    case WM_KILLFOCUS:
    {
        SCAST(Win32_Window *, window)->reset_text_state();
        InputEvent e(InputEventCategory::APPLICATION, InputEventType::APP_FOCUS_LOST, 0, 0);
        window->dispatch_input_event(e);
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
    // Generated by TranslateMessage from WM_KEYDOWN. The window is Unicode, so wparam is one
    // UTF-16 unit; characters outside the BMP come as a surrogate pair in two messages.
    // WM_SYSCHAR (Alt+key) is left to DefWindowProc so menu mnemonics / Alt+Space still work.
    case WM_CHAR:
    {
        Win32_Window *win32_window = SCAST(Win32_Window *, window);
        const uint32_t codepoint = win32_window->decode_utf16(SCAST(wchar_t, wparam));
        // Skip control characters (Backspace, Tab, Enter, Esc, Ctrl+letter, DEL): those are
        // handled as key events, not text.
        if (codepoint >= 0x20 && codepoint != 0x7F)
        {
            InputEvent e = InputEvent::text(codepoint);
            window->dispatch_input_event(e);
        }
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
        result = DefWindowProcW(hwnd, u_msg, wparam, lparam);

        break;
    }
    }
    return result;
}

// The window class is registered with the W API so the window is Unicode: WM_CHAR then carries
// UTF-16 instead of the ANSI code page.
static void register_window_class(HINSTANCE hInst, const wchar_t *className)
{
    WNDCLASSEXW window_class;
    ZeroMemory(&window_class, sizeof(WNDCLASSEXW));

    window_class.cbSize = sizeof(WNDCLASSEXW);
    window_class.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
    window_class.hInstance = hInst;
    window_class.lpfnWndProc = window_procedure;
    window_class.lpszClassName = className;

    ATOM atom = RegisterClassExW(&window_class);
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

    while (PeekMessageW(&msg, 0, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
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

    const wchar_t class_name[] = L"VoidEngineWindowClass";
    hinstance_ = GetModuleHandle(NULL);
    register_window_class(hinstance_, class_name);

    constexpr UINT base_api = 96;
    UINT dpi = GetDpiForSystem();
    float scale = SCAST(float, dpi) / SCAST(float, base_api);
    property_.width_ *= scale;
    property_.height_ *= scale;

    hwnd_ = CreateWindowExW(0, class_name, L"Void-engine", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                                   CW_USEDEFAULT, CW_USEDEFAULT, property_.width_, property_.height_,
                                   NULL, NULL, hinstance_, this);

    // LOG_INFO("Win32", "Window: %u - %u", m_property.width, m_property.height);
    // LOG_INFO("Win32", "Rect: %u - %u", GetClientDimension(m_windowHandle).width,
    //          GetClientDimension(m_windowHandle).height);

    if (!hwnd_)
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

} // namespace VoidEngine

#endif // VOID_WIN32
