#pragma once
#include "vallocator.h"

struct ImGuiContext;
struct ImDrawData;

namespace VoidEngine
{
class Window;

// Owns the single ImGui context (platform-agnostic). Not a layer: Application calls
// begin_frame/end_frame around the layers' on_imgui_render. The renderer owns the GPU half
// (VulkanBackend::init_imgui / draw_frame).
//
// Config: docking OFF, multi-viewport OFF (panels never become OS windows).
// Allocations: a dedicated TLFS (tlfs_free needs no size, unlike Vallocator::vfree, and ImGui's free
// callback gives none). Not shared with other threads. Main thread only.
class ImGuiHost
{
public:
    // TLFS init -> SetAllocatorFunctions -> CreateContext -> style/DPI -> ImGuiPlatform::init.
    // Needs a created window.
    bool init(Window *window);
    // ImGuiPlatform::shutdown -> DestroyContext -> TLFS destroy (asserts on leaks).
    // The renderer backend must be shut down first.
    void destroy();

    void begin_frame();          // platform new frame + ImGui::NewFrame (renderer new frame goes first)
    ImDrawData *end_frame();     // ImGui::Render; nullptr if not initialized

    bool wants_keyboard() const;
    bool wants_mouse() const;
    bool is_initialized() const { return initialized_; }

private:
    TLFS tlfs_;
    ImGuiContext *context_ = nullptr;
    bool initialized_ = false;
};
} // namespace VoidEngine
