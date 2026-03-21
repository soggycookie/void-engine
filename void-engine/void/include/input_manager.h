#include "input/key_button.h"
#include "input_event.h"
#include "pch.h"

namespace VoidEngine
{

class Layer;

constexpr const uint32_t MaxInputBuffer = 10;

class InputManager
{
public:
    InputManager() : m_frameInputCount(0) { m_currInputs.reset(); }

    bool IsBtnPressed(VoidKeyButton btn);
    bool IsBtnReleased(VoidKeyButton btn);

    bool IsBtnPressed(char c);
    bool IsBtnReleased(char c);

private:
    friend class Layer;

    void AddEvent(const InputEvent &e);
    void Clear();

private:
    InputEvent m_inputBuffer[MaxInputBuffer];
    std::bitset<MaxKeyCount> m_currInputs;
    std::bitset<MaxKeyCount> m_prevInputs;
    uint32_t m_frameInputCount;
};

} // namespace VoidEngine
