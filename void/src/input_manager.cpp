#include "input_manager.h"
#include "input/key_button.h"
#include "input_event.h"
#include "logger/log.h"
#include "pch.h"
#include "utils.h"

namespace VoidEngine
{

bool InputManager::is_btn_pressed(KeyCode btn) const
{
    ENGINE_ASSERT(btn != KeyCode::NONE, "Keycode must not be NONE");
    return prev_input_state[static_cast<uint16_t>(btn)] == 0 &&
           cur_input_state[static_cast<uint16_t>(btn)] == 1;
}

bool InputManager::is_btn_released(KeyCode btn) const
{
    ENGINE_ASSERT(btn != KeyCode::NONE, "Keycode must not be NONE");
    return prev_input_state[static_cast<uint16_t>(btn)] == 1 &&
           cur_input_state[static_cast<uint16_t>(btn)] == 0;
}

bool InputManager::is_btn_held(KeyCode btn) const
{
    ENGINE_ASSERT(btn != KeyCode::NONE, "Keycode must not be NONE");
    return prev_input_state[static_cast<uint16_t>(btn)] == 1 &&
           cur_input_state[static_cast<uint16_t>(btn)] == 1;
}

void InputManager::add_event(const InputEvent &e)
{
    switch (e.get_category())
    {
    case InputEventCategory::MOUSE:
    {
        cur_input_state.set(SCAST(uint16_t, (e.get_key_btn())));

        if (InputEventType::MOUSE_MOVE == e.get_type())
        {
            mouse_pos = e.get_mouse_pos();
        }

        break;
    }
    case InputEventCategory::KEYBOARD:
    {
        switch (e.get_type())
        {
        case InputEventType::KEY_PRESSED:
        {
            cur_input_state.set(SCAST(uint16_t, (e.get_key_btn())), true);
            break;
        }
        case InputEventType::KEY_RELEASED:
        {
            cur_input_state.set(SCAST(uint16_t, (e.get_key_btn())), false);
            break;
        }
        default:
        {
            break;
        }
        }

        break;
    }
    default:
    {
        return;
    }
    }

    // if (frame_input_count < MAX_INPUT_BUFFER)
    // {
    //     input_buffer[frame_input_count++];
    // }
}
void InputManager::flush_state()
{
    frame_input_count = 0;
    prev_input_state = cur_input_state;
}
} // namespace VoidEngine
