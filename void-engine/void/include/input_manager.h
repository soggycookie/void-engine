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
    InputManager() : m_frameInputCount(0), m_mousePos()
    {
        m_currInputState.reset();
        m_prevInputState.reset();
    }

    bool IsBtnPressed(VoidKeyButton btn);
    bool IsBtnReleased(VoidKeyButton btn);
    bool IsBtnHeld(VoidKeyButton btn);

    bool IsBtnPressed(char c);
    bool IsBtnReleased(char c);

    const std::bitset<256>& PrevMask() const {return m_prevInputState;}
    const std::bitset<256>& CurrMask() const {return m_currInputState;}

    MousePos GetMousePos() const {return m_mousePos;}

private:
    friend class Layer;

    void AddEvent(const InputEvent &e);
    void Clear();

private:
    InputEvent m_inputBuffer[MaxInputBuffer];
    std::bitset<MaxKeyCount> m_currInputState;
    std::bitset<MaxKeyCount> m_prevInputState;
    uint32_t m_frameInputCount;
    MousePos m_mousePos;
};

} // namespace VoidEngine
