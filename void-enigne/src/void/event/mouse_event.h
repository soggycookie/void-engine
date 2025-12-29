#pragma once

#include "event.h"
#include "void/input/key_button.h"

namespace VoidEngine
{

    struct MousePos
    {
        int32_t x;
        int32_t y;
    };

    class MousePressedEvent : public Event
    {
    public:
        MousePressedEvent(VoidMouseButton btn)
            : m_btn(btn)
        {
        }

        VoidMouseButton GetButton() const
        {
            return m_btn;
        }

        EVENT_CATEGORY(EventCategory::MOUSE)
        EVENT_TYPE(EventType::MOUSE_PRESSED)

    private:
        VoidMouseButton m_btn;
    };

    class MouseReleasedEvent : public Event
    {
    public:
        MouseReleasedEvent(VoidMouseButton btn)
            : m_btn(btn)
        {
        }

        VoidMouseButton GetButton() const
        {
            return m_btn;
        }

        EVENT_CATEGORY(EventCategory::MOUSE)
        EVENT_TYPE(EventType::MOUSE_RELEASED)

    private:
        VoidMouseButton m_btn;
    };

    class MouseWheelRotatedEvent : public Event
    {
    public:
        MouseWheelRotatedEvent() = default;

        EVENT_CATEGORY(EventCategory::MOUSE)
        EVENT_TYPE(EventType::MOUSE_WHEEL_ROTATED)
    };

    class MouseMovedEvent : public Event
    {
    public:
        MouseMovedEvent(int32_t x, int32_t y)
            : m_pos({x, y})
        {
        }

        MousePos GetMousePos() const
        {
            return m_pos;
        }

        int32_t GetX() const
        {
            return m_pos.x;
        }
        
        int32_t GetY() const
        {
            return m_pos.y;
        }

        EVENT_CATEGORY(EventCategory::MOUSE)
        EVENT_TYPE(EventType::MOUSE_MOVE)

    private:
        MousePos m_pos;
    };
}
