#pragma once
#include "common_type.h"
#include "input/key_button.h"
#include "pch.h"
#include <cstdint>

namespace VoidEngine
{
enum class InputEventCategory : uint16_t
{
    NONE,
    KEYBOARD,
    MOUSE,
    APPLICATION
};

enum class InputEventType : uint16_t
{
    NONE,
    KEY_PRESSED,
    KEY_RELEASED,

    APP_CLOSED,
    APP_RESIZING,
    APP_ENTER_RESIZE,
    APP_EXIT_RESIZE,

    MOUSE_PRESSED,
    MOUSE_RELEASED,
    MOUSE_WHEEL_ROTATED,
    MOUSE_MOVE
};

struct MousePos
{
    MousePos() = default;
    MousePos(int32_t x, int32_t y) : mouseX(x), mouseY(y) {}

    int32_t mouseX;
    int32_t mouseY;
};

class InputEvent
{
public:
    InputEvent()
        : m_category(InputEventCategory::NONE), m_type(InputEventType::NONE),
          m_keyBtn(VoidKeyButton::NONE), m_mousePos(), m_isHandled(false)
    {
    }

    InputEvent(InputEventCategory category, InputEventType type, int32_t x,
               int32_t y)
        : m_category(category), m_type(type), m_keyBtn(VoidKeyButton::NONE),
          m_mousePos(x, y), m_isHandled(false)
    {
    }

    // keyboard
    InputEvent(InputEventType type, VoidKeyButton btn)
        : m_category(InputEventCategory::KEYBOARD), m_type(type), m_keyBtn(btn),
          m_mousePos(), m_isHandled(false)
    {
    }

    // mouse
    InputEvent(InputEventType type, VoidKeyButton btn, int32_t x, int32_t y)
        : m_category(InputEventCategory::MOUSE), m_type(type), m_keyBtn(btn),
          m_mousePos(x, y), m_isHandled(false)
    {
    }

    ~InputEvent() = default;

    InputEventCategory Category() const { return m_category; }

    InputEventType Type() const { return m_type; }

    VoidKeyButton KeyBtn() const { return m_keyBtn; }

    MousePos GetMousePos() const { return m_mousePos; }

    bool IsHandled() const { return m_isHandled; }

    void IsHandled(bool isHandled) { m_isHandled = isHandled; }

private:
    InputEventCategory m_category;
    InputEventType m_type;
    VoidKeyButton m_keyBtn;
    MousePos m_mousePos;
    bool m_isHandled;
};

} // namespace VoidEngine
