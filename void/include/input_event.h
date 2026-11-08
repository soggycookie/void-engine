#pragma once
#include "input/key_button.h"

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
        : category(InputEventCategory::NONE), type(InputEventType::NONE),
          key_btn(KeyCode::NONE), mouse_pos(), handled(false)
    {
    }

    InputEvent(InputEventCategory category, InputEventType type, int32_t x, int32_t y)
        : category(category), type(type), key_btn(KeyCode::NONE), mouse_pos(x, y),
          handled(false)
    {
    }

    // keyboard
    InputEvent(InputEventType type, KeyCode btn)
        : category(InputEventCategory::KEYBOARD), type(type), key_btn(btn), mouse_pos(),
          handled(false)
    {
    }

    // mouse
    InputEvent(InputEventType type, KeyCode btn, int32_t x, int32_t y)
        : category(InputEventCategory::MOUSE), type(type), key_btn(btn), mouse_pos(x, y),
          handled(false)
    {
    }

    InputEventCategory get_category() const { return category; }
    InputEventType get_type() const { return type; }
    KeyCode get_key_btn() const { return key_btn; }
    MousePos get_mouse_pos() const { return mouse_pos; }
    bool is_handled() const { return handled; }
    void handle() { handled = true; }

private:
    InputEventCategory category;
    InputEventType type;
    KeyCode key_btn;
    MousePos mouse_pos;
    bool handled;
};

} // namespace VoidEngine
