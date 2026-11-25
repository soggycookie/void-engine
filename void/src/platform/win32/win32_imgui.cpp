#ifdef VOID_WIN32
#include "win32_window.h"

#include "imgui_platform.h"
#include "logger/log.h"
#include "utils.h"

#include "imgui.h"
#include "backends/imgui_impl_win32.h"

// Declared in imgui_impl_win32.h only behind an #if 0 guard, so forward declare it here.
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wparam,
                                                             LPARAM lparam);

namespace VoidEngine
{
namespace
{
Window *s_window = nullptr;

intptr_t imgui_win32_message_hook(void *handle, uint32_t msg, uintptr_t wparam, intptr_t lparam)
{
    return ImGui_ImplWin32_WndProcHandler(RCAST(HWND, handle), msg, wparam, lparam);
}
} // namespace

bool ImGuiPlatform::init(Window *window)
{
    ENGINE_ASSERT(window, "Window must not be null!");
    Win32_Window *win32_window = RCAST(Win32_Window *, window);
    if (!ImGui_ImplWin32_Init(win32_window->get_hwnd()))
    {
        LOG_ERROR("IMGUI", "ImGui_ImplWin32_Init failed");
        return false;
    }
    // No multi-viewport (D10). imgui_impl_win32 always advertises PlatformHasViewports, and then
    // imgui_impl_vulkan asserts that Platform_CreateVkSurface is set, which this Win32 backend does not
    // provide. Clearing the flag keeps every panel inside the main viewport.
    ImGui::GetIO().BackendFlags &= ~ImGuiBackendFlags_PlatformHasViewports;

    s_window = window;
    window->set_native_message_hook(&imgui_win32_message_hook);
    return true;
}

void ImGuiPlatform::new_frame()
{
    ImGui_ImplWin32_NewFrame();
}

void ImGuiPlatform::shutdown()
{
    if (s_window)
    {
        s_window->set_native_message_hook(nullptr);
        s_window = nullptr;
    }
    ImGui_ImplWin32_Shutdown();
}

float ImGuiPlatform::get_dpi_scale(Window *window)
{
    return ImGui_ImplWin32_GetDpiScaleForHwnd(RCAST(Win32_Window *, window)->get_hwnd());
}
} // namespace VoidEngine
#endif // VOID_WIN32
