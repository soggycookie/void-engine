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
    // A typed character (Unicode code point), after keyboard layout, dead keys and IME.
    // Control characters (< 0x20, 0x7F) are not sent; read Backspace/Enter/Tab as key events.
    TEXT_INPUT,

    APP_CLOSED,
    APP_RESIZING,
    APP_ENTER_RESIZE,
    APP_EXIT_RESIZE,
    // The window lost keyboard focus (Alt+Tab, click on another window, ...). Key-up messages
    // for keys held at that moment go to the other window, so held input state is cleared.
    APP_FOCUS_LOST,

    MOUSE_PRESSED,
    MOUSE_RELEASED,
    MOUSE_WHEEL_ROTATED,
    MOUSE_MOVE
};

struct MousePos
{
    MousePos() = default;
    MousePos(int32_t x, int32_t y) : mouse_x_(x), mouse_y_(y) {}

    int32_t mouse_x_;
    int32_t mouse_y_;
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

    // text
    static InputEvent text(uint32_t codepoint)
    {
        InputEvent e;
        e.category = InputEventCategory::KEYBOARD;
        e.type = InputEventType::TEXT_INPUT;
        e.codepoint = codepoint;
        return e;
    }

    InputEventCategory get_category() const { return category; }
    InputEventType get_type() const { return type; }
    KeyCode get_key_btn() const { return key_btn; }
    MousePos get_mouse_pos() const { return mouse_pos; }
    // Only meaningful for TEXT_INPUT.
    uint32_t get_codepoint() const { return codepoint; }
    bool is_handled() const { return handled; }
    void handle() { handled = true; }

private:
    InputEventCategory category;
    InputEventType type;
    KeyCode key_btn;
    MousePos mouse_pos;
    uint32_t codepoint = 0;
    bool handled;
};

} // namespace VoidEngine
