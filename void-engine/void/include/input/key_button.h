#pragma once

namespace VoidEngine
{

    constexpr const uint32_t MaxKeyCount = 256;

enum class VoidKeyButton
{
    NONE = 0,
    LEFT_BTN= 1,
    RIGHT_BTN = 2,
    MIDDLE_BTN = 3,
    X_BUTTON_1 = 4,
    X_BUTTON_2 = 5,


    A = 100,
    B = 101,
    KEY_UNKNOWN = 254,
};

} // namespace VoidEngine
