#include "win32_vk_mapper.h"


namespace VoidEngine
{
    VoidKeyButton MapVkToVoidKey(int32_t vkCode)
    {
        switch(vkCode)
        {
            case 'A':
                return VoidKeyButton::A;
            case 'B':
                return VoidKeyButton::B;
            default:
                return VoidKeyButton::KEY_UNKNOWN;
        }
    }
}