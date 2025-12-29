#pragma once

#include "event.h"
#include "void/input/key_button.h"

namespace VoidEngine
{

    class KeyboardPressedEvent : public Event
    {
    public:
        KeyboardPressedEvent(VoidKeyButton button)
            : m_button(button)
        {
        }

        EVENT_CATEGORY(EventCategory::KEYBOARD)
        EVENT_TYPE(EventType::KEY_PRESSED)
        
        VoidKeyButton GetEventButton() const
        {
            return m_button;
        }

    private:
        VoidKeyButton m_button;
    };

    class KeyboardReleasedEvent : public Event
    {
    public:
        KeyboardReleasedEvent(VoidKeyButton button)
            : m_button(button)
        {
        }

        EVENT_CATEGORY(EventCategory::KEYBOARD)
        EVENT_TYPE(EventType::KEY_RELEASED)

        VoidKeyButton GetEventButton() const
        {
            return m_button;
        }

    private:
        VoidKeyButton m_button;
    };
}