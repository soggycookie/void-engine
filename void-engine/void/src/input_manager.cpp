#include "input_manager.h"
#include "input/key_button.h"
#include "pch.h"
#include <cassert>
#include <cstdint>

namespace VoidEngine
{

bool InputManager::IsBtnPressed(VoidKeyButton btn)
{
    assert(btn != VoidKeyButton::NONE);
    return m_currInputs[static_cast<uint16_t>(btn)] == 1;
}

bool InputManager::IsBtnReleased(VoidKeyButton btn)
{
    assert(btn != VoidKeyButton::NONE);
    return m_prevInputs[static_cast<uint16_t>(btn)] == 1 &&
           m_currInputs[static_cast<uint16_t>(btn)] == 0;
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
    case InputEventCategory::KEYBOARD:
    {
        m_currInputs.set(static_cast<uint16_t>(e.KeyBtn()));

        break;
    }
    default:
        break;
    }
}
void InputManager::Clear()
{
    m_frameInputCount = 0;

    m_prevInputs = m_currInputs;
    m_currInputs.reset();
}
} // namespace VoidEngine
