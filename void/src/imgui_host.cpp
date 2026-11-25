#include "imgui_host.h"
#include "imgui_platform.h"
#include "logger/log.h"
#include "utils.h"

#include "imgui.h"

#include <cstdlib>

namespace VoidEngine
{
namespace
{
constexpr uint64_t IMGUI_ALIGN = 16;

void *imgui_alloc(size_t size, void *user)
{
    return SCAST(TLFS *, user)->tlfs_alloc(size == 0 ? 1 : size, IMGUI_ALIGN);
}

void imgui_free(void *ptr, void *user)
{
    if (!ptr)
    {
        return;
    }
    SCAST(TLFS *, user)->tlfs_free(ptr);
}

// Restored after the TLFS is destroyed so a stray late ImGui allocation cannot touch freed memory.
void *crt_alloc(size_t size, void *)
{
    return std::malloc(size);
}

void crt_free(void *ptr, void *)
{
    std::free(ptr);
}
} // namespace

bool ImGuiHost::init(Window *window)
{
    ENGINE_ASSERT(window, "Window must not be null!");
    ENGINE_ASSERT(!initialized_, "ImGuiHost is already initialized!");

    tlfs_.init(mib(256), mib(1));
#ifdef VOID_DEBUG
    tlfs_.set_name("ImGui");
#endif // VOID_DEBUG
    ImGui::SetAllocatorFunctions(&imgui_alloc, &imgui_free, &tlfs_);

    IMGUI_CHECKVERSION();
    context_ = ImGui::CreateContext();

    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    // Docking off for now, and no multi-viewport: dragging a panel outside the app window keeps it
    // inside the main viewport instead of creating an OS window.
    io.ConfigFlags &= ~(ImGuiConfigFlags_DockingEnable | ImGuiConfigFlags_ViewportsEnable);

    ImGui::StyleColorsDark();

    if (!ImGuiPlatform::init(window))
    {
        ImGui::DestroyContext(context_);
        context_ = nullptr;
        ImGui::SetAllocatorFunctions(&crt_alloc, &crt_free, nullptr);
        tlfs_.destroy();
        return false;
    }

    // The window is per-monitor DPI aware; scale once at startup (WM_DPICHANGED is not handled yet).
    float scale = ImGuiPlatform::get_dpi_scale(window);
    ImGuiStyle &style = ImGui::GetStyle();
    style.ScaleAllSizes(scale);
    style.FontScaleDpi = scale;

    initialized_ = true;
    return true;
}

void ImGuiHost::destroy()
{
    if (!initialized_)
    {
        return;
    }
    ImGuiPlatform::shutdown();
    ImGui::DestroyContext(context_);
    context_ = nullptr;
    ImGui::SetAllocatorFunctions(&crt_alloc, &crt_free, nullptr);
    tlfs_.destroy();
    initialized_ = false;
}

void ImGuiHost::begin_frame()
{
    if (!initialized_)
    {
        return;
    }
    ImGuiPlatform::new_frame();
    ImGui::NewFrame();
}

ImDrawData *ImGuiHost::end_frame()
{
    if (!initialized_)
    {
        return nullptr;
    }
    ImGui::Render();
    return ImGui::GetDrawData();
}

bool ImGuiHost::wants_keyboard() const
{
    return initialized_ && ImGui::GetIO().WantCaptureKeyboard;
}

bool ImGuiHost::wants_mouse() const
{
    return initialized_ && ImGui::GetIO().WantCaptureMouse;
}
} // namespace VoidEngine
