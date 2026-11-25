#pragma once

namespace VoidEngine
{
class Window;

// Platform half of ImGui (window, OS input, cursor, DPI). Defined per platform:
// Win32 -> src/platform/win32/win32_imgui.cpp (wraps imgui_impl_win32).
// Requires a current ImGui context.
namespace ImGuiPlatform
{
bool init(Window *window);
void new_frame();
void shutdown();
// Content scale of the monitor the window is on (1.0 = 96 DPI).
float get_dpi_scale(Window *window);
} // namespace ImGuiPlatform
} // namespace VoidEngine
