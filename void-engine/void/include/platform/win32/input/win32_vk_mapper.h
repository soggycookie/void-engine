#pragma once
#include "pch.h"
#include "input/key_button.h"
#define NOMINMAX
#include <windows.h>

namespace VoidEngine
{
// ------------------------------------------------------------
//  Win32VKToKeyCode
//
//  Call from WM_KEYDOWN / WM_KEYUP / WM_SYSKEYDOWN / WM_SYSKEYUP:
//
//      case WM_KEYDOWN: case WM_KEYUP:
//      case WM_SYSKEYDOWN: case WM_SYSKEYUP:
//          KeyCode key = Win32VKToKeyCode(wParam, lParam);
// ------------------------------------------------------------
inline KeyCode Win32VKToKeyCode(WPARAM vk, LPARAM lParam) noexcept
{
    const bool    extended = (lParam >> 24) & 1;
    const uint8_t scancode = (lParam >> 16) & 0xFF;
 
    switch (vk)
    {
        // --------------------------------------------------
        // ENTER: main board (extended=0) vs numpad (extended=1)
        // --------------------------------------------------
        case VK_RETURN:
            return extended ? KeyCode::KP_ENTER : KeyCode::ENTER;
 
        // --------------------------------------------------
        // Navigation cluster vs numpad (NumLock OFF)
        //   extended=1  nav cluster key
        //   extended=0  numpad key
        // --------------------------------------------------
        case VK_INSERT: return extended ? KeyCode::INSERT    : KeyCode::KP_0;
        case VK_DELETE: return extended ? KeyCode::DEL       : KeyCode::KP_DECIMAL;
        case VK_HOME:   return extended ? KeyCode::HOME      : KeyCode::KP_7;
        case VK_END:    return extended ? KeyCode::END       : KeyCode::KP_1;
        case VK_PRIOR:  return extended ? KeyCode::PAGE_UP   : KeyCode::KP_9;
        case VK_NEXT:   return extended ? KeyCode::PAGE_DOWN : KeyCode::KP_3;
        case VK_LEFT:   return extended ? KeyCode::LEFT      : KeyCode::KP_4;
        case VK_RIGHT:  return extended ? KeyCode::RIGHT     : KeyCode::KP_6;
        case VK_UP:     return extended ? KeyCode::UP        : KeyCode::KP_8;
        case VK_DOWN:   return extended ? KeyCode::DOWN      : KeyCode::KP_2;
        case VK_CLEAR:  return KeyCode::KP_5; // numpad 5 with NumLock off
 
        // --------------------------------------------------
        // Numpad divide — always has extended=1
        // --------------------------------------------------
        case VK_DIVIDE:
            return KeyCode::KP_DIVIDE;
 
        // --------------------------------------------------
        // Modifiers — resolve generic VK to L/R variant
        // --------------------------------------------------
        case VK_SHIFT:
        {
            // MapVirtualKey resolves the true L/R VK from scancode
            const UINT mapped = MapVirtualKey(scancode, MAPVK_VSC_TO_VK_EX);
            if (mapped == VK_RSHIFT) return KeyCode::RIGHT_SHIFT;
            return KeyCode::LEFT_SHIFT;
        }
        case VK_CONTROL:
            return extended ? KeyCode::RIGHT_CTRL : KeyCode::LEFT_CTRL;
 
        case VK_MENU: // ALT
            return extended ? KeyCode::RIGHT_ALT : KeyCode::LEFT_ALT;
 
        // --------------------------------------------------
        // Everything else: VK value == KeyCode value directly
        // --------------------------------------------------
        default:
            if (vk >= static_cast<WPARAM>(MaxKeyCount))
                return KeyCode::KEY_UNKNOWN;
            return static_cast<KeyCode>(vk);
    }
}
}