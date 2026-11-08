#include "input/key_button.h"
#include "input_event.h"

namespace VoidEngine
{

class Layer;

constexpr uint32_t MAX_INPUT_BUFFER = 64;

class InputManager
{
public:
    InputManager() : frame_input_count(0), mouse_pos()
    {
        cur_input_state.reset();
        prev_input_state.reset();
    }

    bool is_btn_pressed(KeyCode btn) const;
    bool is_btn_released(KeyCode btn) const;
    bool is_btn_held(KeyCode btn) const;

    // bool IsBtnPressed(char c);
    // bool IsBtnReleased(char c);

    const std::bitset<256> &prev_mask() const { return prev_input_state; }
    const std::bitset<256> &cur_mask() const { return cur_input_state; }

    MousePos get_mouse_pos() const { return mouse_pos; }

private:
    friend class Layer;

    void add_event(const InputEvent &e);
    void flush_state();

private:
    InputEvent input_buffer[MAX_INPUT_BUFFER];
    std::bitset<MAX_KEYCODE> cur_input_state;
    std::bitset<MAX_KEYCODE> prev_input_state;
    uint32_t frame_input_count;
    MousePos mouse_pos;
};

} // namespace VoidEngine
