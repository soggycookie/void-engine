#pragma once
#include "common_type.h"
#include "input/key_button.h"
#include "pch.h"
#include <cstdint>

namespace VoidEngine
{
enum class InputEventCategory : uint16_t
{
    KEYBOARD,
    MOUSE,
    APPLICATION
};

enum class InputEventType : uint16_t
{
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

#define EVENT_CATEGORY(category)                                               \
    EventCategory GetEventCategory() const override { return category; }
#define EVENT_TYPE(event)                                                      \
    EventType GetEventType() const override { return event; }

// TODO: centralize into event queue
class InputEvent
{
public:
    InputEvent() = default;

    InputEvent(InputEventCategory category, InputEventType type, int32_t x,
          int32_t y)
        : m_category(category), m_type(type), m_keyBtn(VoidKeyButton::NONE),
          m_x(x), m_y(y), m_isHandled(false)
    {
    }

    // keyboard
    InputEvent(InputEventType type, VoidKeyButton key)
        : m_category(InputEventCategory::KEYBOARD), m_type(type), m_keyBtn(key),
          m_x(0), m_y(0), m_isHandled(false)
    {
    }

    // mouse
    InputEvent(InputEventType type, VoidMouseButton btn, int32_t x, int32_t y)
        : m_category(InputEventCategory::MOUSE), m_type(type), m_mouseBtn(btn),
          m_x(x), m_y(y), m_isHandled(false)
    {
    }


    ~InputEvent() = default;

    InputEventCategory Category() const { return m_category; }

    InputEventType Type() const { return m_type; }

    VoidKeyButton KeyBtn() const {
        if(m_category != InputEventCategory::KEYBOARD)
        {
            assert(0);
        }
        return m_keyBtn;
    }
    
    VoidMouseButton MouseBtn() const {
        if(m_category != InputEventCategory::MOUSE)
        {
            assert(0);
        }
        return m_mouseBtn;
    }

    bool IsHandled() const { return m_isHandled; }

    void IsHandled(bool isHandled) { m_isHandled = isHandled; }

private:
    InputEventCategory m_category;
    InputEventType m_type;
    union
    {
        VoidKeyButton m_keyBtn;
        VoidMouseButton m_mouseBtn;
    };
    int32_t m_x;
    int32_t m_y;
    bool m_isHandled;
};

} // namespace VoidEngine
