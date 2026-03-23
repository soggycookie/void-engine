#include "input_manager.h"
#include "input/key_button.h"
#include "input_event.h"
#include "pch.h"
#include <cassert>
#include <cstdint>

namespace VoidEngine
{

bool InputManager::IsBtnPressed(VoidKeyButton btn)
{
    assert(btn != VoidKeyButton::NONE);
    return m_prevInputState[static_cast<uint16_t>(btn)] == 0 && 
        m_currInputState[static_cast<uint16_t>(btn)] == 1;
}

bool InputManager::IsBtnReleased(VoidKeyButton btn)
{
    assert(btn != VoidKeyButton::NONE);
    return m_prevInputState[static_cast<uint16_t>(btn)] == 1 &&
           m_currInputState[static_cast<uint16_t>(btn)] == 0;
}

bool InputManager::IsBtnHeld(VoidKeyButton btn)
{
    assert(btn != VoidKeyButton::NONE);
    return m_prevInputState[static_cast<uint16_t>(btn)] == 1 &&
           m_currInputState[static_cast<uint16_t>(btn)] == 1;
}

bool InputManager::IsBtnPressed(char c)
{
    VoidKeyButton btn = VoidKeyButton::NONE;
    switch (c)
    {
    default:
        break;
    }

    return IsBtnPressed(btn);
}
bool InputManager::IsBtnReleased(char c)
{
    VoidKeyButton btn = VoidKeyButton::NONE;
    switch (c)
    {
    default:
        break;
    }

    return IsBtnReleased(btn);
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
        m_currInputState.set(static_cast<uint16_t>(e.KeyBtn()));

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
    m_currInputState.reset();
}
} // namespace VoidEngine
