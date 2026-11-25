#pragma once
#include "utils.h"
#include "input/key_button.h"
#include "input_event.h"

namespace VoidEngine
{

constexpr uint32_t MAX_INPUT_BUFFER = 64;
constexpr uint32_t MAX_TEXT_INPUT = 64;

// Single source of input state, owned by Application. Application feeds it every event before the
// layers see it, so state never depends on whether a layer marked the event handled.
//
// Two views of the same frame:
//  - state: down / pressed / released bits. A press and release inside one frame are both visible,
//    but repeated presses of one key collapse into one flag.
//  - event buffer: every key and mouse-button event in arrival order (press, release, press, ...),
//    for code that needs counts or order (double taps, combos, text-like input). MOUSE_MOVE is not
//    buffered: it would crowd out key events, and position/delta already cover it.
// Both are cleared by begin_frame().
class InputManager
{
public:
    // Call once per frame, before the window pumps messages.
    void begin_frame();
    void on_event(const InputEvent &e);

    // Level: the key is down right now.
    bool is_down(KeyCode k) const;
    // Edges: happened since begin_frame().
    bool is_pressed(KeyCode k) const;
    bool is_released(KeyCode k) const;

    // Ordered key/mouse-button events since begin_frame().
    const InputEvent *get_events() const { return events_; }
    uint32_t get_event_count() const { return event_count_; }
    // Events that did not fit in MAX_INPUT_BUFFER this frame (state is still correct).
    uint32_t get_dropped_count() const { return dropped_count_; }
    // Number of press events for k this frame (counts every press, including repeats within a frame).
    uint32_t press_count(KeyCode k) const;

    MousePos get_mouse_pos() const { return mouse_pos_; }
    // Mouse movement accumulated since begin_frame().
    MousePos get_mouse_delta() const { return mouse_delta_; }

    // Text typed since begin_frame(), as Unicode code points in order (no control characters).
    // Use this for text fields; use key events for Backspace/Enter/arrows.
    const uint32_t *get_text() const { return text_; }
    uint32_t get_text_count() const { return text_count_; }

private:
    void push_event(const InputEvent &e);
    void set_down(KeyCode k);
    void set_up(KeyCode k);
    void release_all();

private:
    std::bitset<MAX_KEYCODE> down_;
    std::bitset<MAX_KEYCODE> pressed_;
    std::bitset<MAX_KEYCODE> released_;
    InputEvent events_[MAX_INPUT_BUFFER];
    uint32_t event_count_ = 0;
    uint32_t dropped_count_ = 0;
    uint32_t text_[MAX_TEXT_INPUT];
    uint32_t text_count_ = 0;
    MousePos mouse_pos_{0, 0};
    MousePos mouse_delta_{0, 0};
    bool has_mouse_pos_ = false;
};

} // namespace VoidEngine
