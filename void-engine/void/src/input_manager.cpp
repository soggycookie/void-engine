#include "input_manager.h"
#include "input/key_button.h"
#include "input_event.h"
#include "pch.h"
#include <cassert>
#include <cstdint>

namespace VoidEngine
{

bool InputManager::IsBtnPressed(KeyCode btn) const
{
    assert(btn != KeyCode::NONE);
    return m_prevInputState[static_cast<uint16_t>(btn)] == 0 &&
           m_currInputState[static_cast<uint16_t>(btn)] == 1;
}

bool InputManager::IsBtnReleased(KeyCode btn) const
{
    assert(btn != KeyCode::NONE);
    return m_prevInputState[static_cast<uint16_t>(btn)] == 1 &&
           m_currInputState[static_cast<uint16_t>(btn)] == 0;
}

bool InputManager::IsBtnHeld(KeyCode btn) const
{
    assert(btn != KeyCode::NONE);
    return m_prevInputState[static_cast<uint16_t>(btn)] == 1 &&
           m_currInputState[static_cast<uint16_t>(btn)] == 1;
}


void InputManager::AddEvent(const InputEvent &e)
{
    if (m_frameInputCount == MaxInputBuffer)
    {
        return;
    }

    switch (e.Category())
    {
    case InputEventCategory::MOUSE:
    {
        m_currInputState.set(static_cast<uint16_t>(e.KeyBtn()));

        if (InputEventType::MOUSE_MOVE == e.Type())
        {
            m_mousePos = e.GetMousePos();
        }

        break;
    }
    case InputEventCategory::KEYBOARD:
    {
        switch(e.Type())
        {
            case InputEventType::KEY_PRESSED:
            {
                m_currInputState.set(static_cast<uint16_t>(e.KeyBtn()));
                break;   
            }
            case InputEventType::KEY_RELEASED:
            {
                m_currInputState.reset(static_cast<uint16_t>(e.KeyBtn()));
                break;               
            }
        }

        break;
    }
    default:
        break;
    }
}
void InputManager::Clear()
{
    m_frameInputCount = 0;

    m_prevInputState = m_currInputState;
}
} // namespace VoidEngine
