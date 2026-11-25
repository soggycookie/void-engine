#include "input_manager.h"
#include "utils.h"

namespace VoidEngine
{

namespace
{
// Win32 can deliver VK codes the mapper does not name, so reject NONE and anything out of range
// instead of asserting.
inline bool valid_key(KeyCode k)
{
    return k != KeyCode::NONE && SCAST(uint16_t, k) < MAX_KEYCODE;
}
} // namespace

bool InputManager::is_down(KeyCode k) const
{
    return valid_key(k) && down_[SCAST(uint16_t, k)];
}

bool InputManager::is_pressed(KeyCode k) const
{
    return valid_key(k) && pressed_[SCAST(uint16_t, k)];
}

bool InputManager::is_released(KeyCode k) const
{
    return valid_key(k) && released_[SCAST(uint16_t, k)];
}

void InputManager::set_down(KeyCode k)
{
    if (!valid_key(k))
    {
        return;
    }
    const uint16_t idx = SCAST(uint16_t, k);
    // Ignore key auto-repeat: only a transition up -> down is a press.
    if (!down_[idx])
    {
        pressed_.set(idx);
    }
    down_.set(idx);
}

void InputManager::set_up(KeyCode k)
{
    if (!valid_key(k))
    {
        return;
    }
    const uint16_t idx = SCAST(uint16_t, k);
    if (down_[idx])
    {
        released_.set(idx);
    }
    down_.reset(idx);
}

void InputManager::release_all()
{
    // Every held key/button gets a release edge this frame, so code that acts on release
    // (stop sprinting, end a drag) still runs.
    released_ |= down_;
    down_.reset();
    // The next MOUSE_MOVE may come from anywhere on screen; don't turn that jump into a delta.
    has_mouse_pos_ = false;
}

void InputManager::push_event(const InputEvent &e)
{
    if (event_count_ < MAX_INPUT_BUFFER)
    {
        events_[event_count_++] = e;
    }
    else
    {
        ++dropped_count_;
    }
}

uint32_t InputManager::press_count(KeyCode k) const
{
    if (!valid_key(k))
    {
        return 0;
    }
    uint32_t count = 0;
    for (uint32_t i = 0; i < event_count_; ++i)
    {
        const InputEvent &ev = events_[i];
        if (ev.get_key_btn() == k && (ev.get_type() == InputEventType::KEY_PRESSED ||
                                      ev.get_type() == InputEventType::MOUSE_PRESSED))
        {
            ++count;
        }
    }
    return count;
}

void InputManager::on_event(const InputEvent &e)
{
    switch (e.get_type())
    {
    case InputEventType::KEY_PRESSED:
    case InputEventType::MOUSE_PRESSED:
    {
        push_event(e);
        set_down(e.get_key_btn());
        break;
    }
    case InputEventType::KEY_RELEASED:
    case InputEventType::MOUSE_RELEASED:
    {
        push_event(e);
        set_up(e.get_key_btn());
        break;
    }
    case InputEventType::TEXT_INPUT:
    {
        // Overflow just drops the extra characters; 64 per frame is far above typing speed.
        if (text_count_ < MAX_TEXT_INPUT)
        {
            text_[text_count_++] = e.get_codepoint();
        }
        break;
    }
    case InputEventType::APP_FOCUS_LOST:
    {
        release_all();
        break;
    }
    case InputEventType::MOUSE_MOVE:
    {
        // Only MOUSE_MOVE carries a real position; button events currently report 0,0.
        const MousePos p = e.get_mouse_pos();
        if (has_mouse_pos_)
        {
            mouse_delta_.mouse_x_ += p.mouse_x_ - mouse_pos_.mouse_x_;
            mouse_delta_.mouse_y_ += p.mouse_y_ - mouse_pos_.mouse_y_;
        }
        mouse_pos_ = p;
        has_mouse_pos_ = true;
        break;
    }
    default:
    {
        break;
    }
    }
}

void InputManager::begin_frame()
{
    pressed_.reset();
    released_.reset();
    event_count_ = 0;
    dropped_count_ = 0;
    text_count_ = 0;
    mouse_delta_ = MousePos(0, 0);
}

} // namespace VoidEngine
