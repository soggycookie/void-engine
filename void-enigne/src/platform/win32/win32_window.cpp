#include "win32_window.h"

#include "void/window.h"
#include "void/renderer.h"

#include "void/event/event.h"
#include "void/event/application_event.h"
#include "void/event/keyboard_event.h"
#include "void/event/mouse_event.h"
#include "input/win32_vk_mapper.h"

#include <directxmath.h>
namespace VoidEngine
{
#define MIN_WINDOW_X 200
#define MIN_WINDOW_Y 200

    static ClientDimension GetClientDimension(HWND hwnd)
    {
        RECT clientRect;
        GetClientRect(hwnd, &clientRect);

        ClientDimension dimension;

        dimension.width = clientRect.right - clientRect.left;
        dimension.height = clientRect.bottom - clientRect.top;

        return dimension;
    }
    
    //Win32 window definition
    Window* Window::Create(const WindowProperty& property ,EventCallback func)
    {
        void* windowAddr = GlobalPersistantAllocator::Get().Alloc(sizeof(Win32Window), alignof(Win32Window));
        return new (windowAddr) Win32Window(property, func);
    }

    /*
    NOTE:

    While your window procedure executes, it blocks any other messages for windows created on the same thread.
    Therefore, avoid lengthy processing inside your window procedure.
    For example, suppose your program opens a TCP connection and waits indefinitely for the server to respond.
    If you do that inside the window procedure, your UI will not respond until the request completes.
    During that time, the window cannot process mouse or keyboard input, repaint itself, or even close.
    https://learn.microsoft.com/en-us/windows/win32/learnwin32/writing-the-window-procedure
    */

    static LRESULT CALLBACK WindowProcedure(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
        LRESULT result = 0;
        Window* window = reinterpret_cast<Window*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

        switch(uMsg)
        {
            //////////////////////////////////////////////////////////////
            //                 APPLICATION EVENT
            //////////////////////////////////////////////////////////////
            case WM_NCCREATE:
            {
                CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
                window = static_cast<Window*>(cs->lpCreateParams);
                SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
                result = DefWindowProc(hwnd, uMsg, wParam, lParam);
                break;
            }
            case WM_CLOSE:
            {
                ApplicationClosedEvent e;
                window->DispatchEvent(e);

                break;
            }
            case WM_DESTROY:
            {
                ApplicationClosedEvent e;
                window->DispatchEvent(e);
                
                break;
            }
            case WM_SIZE:
            {
                int width = LOWORD(lParam);
                int height = HIWORD(lParam); 

                ClientDimension dimension = {width, height};
                ApplicationResizingEvent e(dimension);
                window->DispatchEvent(e);

                break;
            }
            case WM_ENTERSIZEMOVE:
            {
                ApplicationEnterResizeEvent e;
                window->DispatchEvent(e);
                break;
            }
            case WM_EXITSIZEMOVE :
            {

                ClientDimension dimension = GetClientDimension(hwnd);
                ApplicationExitResizeEvent e(dimension);
                window->DispatchEvent(e);
                break;
            }
            case WM_PAINT:
            {
                PAINTSTRUCT ps;
                HDC hdc = BeginPaint(hwnd, &ps);

                FillRect(hdc, &ps.rcPaint, (HBRUSH) (COLOR_WINDOW + 1));

                EndPaint(hwnd, &ps);
                break;
            }
            case WM_GETMINMAXINFO:
            {
                if(window)
                {
                    ((MINMAXINFO*)lParam)->ptMinTrackSize.x = window->GetDimension().width;
                    ((MINMAXINFO*)lParam)->ptMinTrackSize.y = window->GetDimension().height;
                }
                else
                {
                    ((MINMAXINFO*)lParam)->ptMinTrackSize.x = MIN_WINDOW_X;
                    ((MINMAXINFO*)lParam)->ptMinTrackSize.y = MIN_WINDOW_Y;
                }
                break;
            }
            case WM_MOVING:
            {
                Renderer::EndFrame();
                break;
            }
            //////////////////////////////////////////////////////////////
            //                 KEYBOARD EVENT
            //////////////////////////////////////////////////////////////
            case WM_KEYDOWN:
            {
                VoidKeyButton button = MapVkToVoidKey(wParam);
                KeyboardPressedEvent e(button);
                window->DispatchEvent(e);
                break;
            }
            case WM_KEYUP:
            {
                VoidKeyButton button = MapVkToVoidKey(wParam);
                KeyboardReleasedEvent e(button);
                window->DispatchEvent(e);
                break;
            }
            //////////////////////////////////////////////////////////////
            //                 MOUSE EVENT
            //////////////////////////////////////////////////////////////
            case WM_MOUSEMOVE:
            {

                //TODO: support modifier
                //relative to upper-left corner
                int32_t x = LOWORD(lParam);
                int32_t y = HIWORD(lParam);

                MouseMovedEvent e(x, y);
                window->DispatchEvent(e);

                break;
            }
            case WM_LBUTTONDOWN:
            {
                MousePressedEvent e(VoidMouseButton::LEFT_BTN);
                window->DispatchEvent(e);

                break;
            }
            case WM_MBUTTONDOWN:
            {
                MousePressedEvent e(VoidMouseButton::MIDDLE_BTN);
                window->DispatchEvent(e);

                break;
            }
            case WM_RBUTTONDOWN:
            {
                MousePressedEvent e(VoidMouseButton::RIGHT_BTN);
                window->DispatchEvent(e);

                break;
            }
            case WM_LBUTTONUP:
            {
                MouseReleasedEvent e(VoidMouseButton::LEFT_BTN);
                window->DispatchEvent(e);

                break;
            }
            case WM_MBUTTONUP:
            {
                MouseReleasedEvent e(VoidMouseButton::MIDDLE_BTN);
                window->DispatchEvent(e);

                break;
            }
            case WM_RBUTTONUP:
            {
                MouseReleasedEvent e(VoidMouseButton::RIGHT_BTN);
                window->DispatchEvent(e);

                break;
            }
            case WM_MOUSEWHEEL:
            {
                //TODO: handle rotate direction, distance
                
                MouseWheelRotatedEvent e;
                window->DispatchEvent(e);

                break;
            }
            case WM_XBUTTONDOWN:
            {
                int16_t btn = HIWORD(wParam);
                VoidMouseButton voidBtn;
                
                if(btn == XBUTTON1)
                {
                    voidBtn = VoidMouseButton::X_BUTTON_1;
                    std::cout << "x button 1" << std::endl;
                }
                else
                {
                    voidBtn = VoidMouseButton::X_BUTTON_2;
                    std::cout << "x button 2" << std::endl;
                }

                MousePressedEvent e(voidBtn);
                window->DispatchEvent(e);

                break;
            }
            case WM_XBUTTONUP:
            {
                int16_t btn = HIWORD(wParam);
                VoidMouseButton voidBtn;

                if(btn == XBUTTON1)
                {
                    voidBtn = VoidMouseButton::X_BUTTON_1;
                }
                else
                {
                    voidBtn = VoidMouseButton::X_BUTTON_2;
                }

                MouseReleasedEvent e(voidBtn);
                window->DispatchEvent(e);

                break;
            }
            default:
            {
                result = DefWindowProc(hwnd, uMsg, wParam, lParam);

                break;
            }

        }
        return result;
    }


    static void RegisterWindowClass(HINSTANCE hInst, const char* className)
    {
        WNDCLASSEX windowClass;
        ZeroMemory(&windowClass, sizeof(WNDCLASSEX));

        windowClass.cbSize = sizeof(WNDCLASSEX);
        windowClass.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
        windowClass.hInstance = hInst;
        windowClass.lpfnWndProc = WindowProcedure;
        windowClass.lpszClassName = className;

        ATOM atom = RegisterClassEx(&windowClass);
        if (atom == 0) {
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
    void Win32Window::Update()
    {
        LARGE_INTEGER currCount;
        QueryPerformanceCounter(&currCount);
        m_currCount = currCount.QuadPart ;

        if(m_prevCount == 0)
        {
            m_prevCount = m_currCount;
        }

        if(m_isTimeStopped)
        {
            m_deltaTime = 0;
        }
        else
        {
            m_deltaTime = static_cast<double>(m_currCount - m_prevCount) / static_cast<double>(m_countsPerSec.QuadPart);
        }

        m_prevCount = m_currCount;
        
        m_windowTime += m_deltaTime;

        MSG msg;
        ZeroMemory(&msg, sizeof(MSG));
        
        while(PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        //std::cout << m_windowTime << std::endl;
    }

    void Win32Window::BeginTimeElapse()
    {
        LARGE_INTEGER count;
        QueryPerformanceCounter(&count);
        m_stopWatchPrevCount = count.QuadPart;
    }

    void Win32Window::EndTimeElapse(double& outPassedTime)
    {
        LARGE_INTEGER count;
        QueryPerformanceCounter(&count);
        m_stopWatchCurrCount = count.QuadPart;

        int64_t deltaCount = m_stopWatchCurrCount - m_stopWatchPrevCount;
        outPassedTime = static_cast<double>(deltaCount) / static_cast<double>(m_countsPerSec.QuadPart);
    }

    bool Win32Window::Init()
    {
        const char k_className[] = "VoidEngineWindowClass";
        HINSTANCE hInst = GetModuleHandle(NULL);

        RegisterWindowClass(hInst, k_className);

        m_windowHandle = CreateWindowEx(
            0,
            k_className,
            "Void-engine",
            WS_OVERLAPPEDWINDOW | WS_VISIBLE,
            CW_USEDEFAULT, CW_USEDEFAULT, m_property.width, m_property.height,
            NULL,
            NULL,
            hInst,
            this
        );

        if (!m_windowHandle) {
            DWORD err = GetLastError();
            char msg[256];
            sprintf_s(msg, "CreateWindowEx failed: %lu\n", err);
            OutputDebugStringA(msg);
            MessageBoxA(0, msg, "Error", 0);
            return false;
        }   
        
        return true;
    }

    //TODO: support multiple graphic API in the future
    //bool Win32Window::SetupRenderer()
    //{
    //    if(!Window::SetupRenderer())
    //    {
    //        MessageBoxA(m_windowHandle, "D3D11 failed to initialize", "Error", MB_OK);
    //        PostMessage(m_windowHandle, WM_CLOSE, 0, 0);            
    //        return false;
    //    }

    //    //TODO: move this math lib to else where
    //    //or make my own math lib
    //    if(!DirectX::XMVerifyCPUSupport())
    //    {
    //        MessageBoxA(m_windowHandle, "CPU does not support DirectXMath", "Error", MB_OK);      
    //        PostMessage(m_windowHandle, WM_CLOSE, 0, 0);

    //        return false;
    //    }

    //    return true;
    //}

    void Win32Window::SetUpScene()
    {
        
    }

    void* Win32Window::GetDisplayWindow()
    {
        return static_cast<void*>(m_windowHandle);
    }

}