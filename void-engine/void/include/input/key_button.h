#pragma once

namespace VoidEngine
{

constexpr const uint32_t MaxKeyCount = 256;

// ============================================================
//  KeyCode
//  Values mirror Win32 Virtual Key codes where possible.
//  Extended-bit disambiguation is handled in Win32VKToKeyCode.
// ============================================================
enum class KeyCode : uint16_t
{
    NONE = 0,
 
    // --------------------------------------------------------
    // Mouse buttons  (VK_LBUTTON … VK_XBUTTON2)
    // --------------------------------------------------------
    MOUSE_LEFT   = 1,   // VK_LBUTTON
    MOUSE_RIGHT  = 2,   // VK_RBUTTON
    MOUSE_MIDDLE = 4,   // VK_MBUTTON
    MOUSE_X1     = 5,   // VK_XBUTTON1
    MOUSE_X2     = 6,   // VK_XBUTTON2
 
    // --------------------------------------------------------
    // Control / whitespace
    // --------------------------------------------------------
    BACKSPACE    = 8,   // VK_BACK
    TAB          = 9,   // VK_TAB
    ENTER        = 13,  // VK_RETURN  (main keyboard)
    // Generic modifiers — emitted when L/R variant is unknown
    // (e.g. injected input, remapped keys). Prefer LEFT_*/RIGHT_*
    // variants from Win32VKToKeyCode whenever possible.
    SHIFT        = 16,  // VK_SHIFT
    CTRL         = 17,  // VK_CONTROL
    ALT          = 18,  // VK_MENU
    PAUSE        = 19,  // VK_PAUSE
    CAPS_LOCK    = 20,  // VK_CAPITAL
    ESCAPE       = 27,  // VK_ESCAPE
    SPACE        = 32,  // VK_SPACE
 
    // --------------------------------------------------------
    // Navigation cluster  (extended bit = 1)
    // When extended bit = 0 these share VK codes with numpad
    // keys (NumLock off). Disambiguation in Win32VKToKeyCode.
    // --------------------------------------------------------
    PAGE_UP      = 33,  // VK_PRIOR
    PAGE_DOWN    = 34,  // VK_NEXT
    END          = 35,  // VK_END
    HOME         = 36,  // VK_HOME
    LEFT         = 37,  // VK_LEFT
    UP           = 38,  // VK_UP
    RIGHT        = 39,  // VK_RIGHT
    DOWN         = 40,  // VK_DOWN
    PRINT_SCREEN = 44,  // VK_SNAPSHOT
    INSERT       = 45,  // VK_INSERT   (extended = 1)
    DEL          = 46,  // VK_DELETE   (extended = 1) — named DEL to avoid macro clash
 
    // --------------------------------------------------------
    // Top-row digits
    // --------------------------------------------------------
    NUM_0 = 48,
    NUM_1 = 49,
    NUM_2 = 50,
    NUM_3 = 51,
    NUM_4 = 52,
    NUM_5 = 53,
    NUM_6 = 54,
    NUM_7 = 55,
    NUM_8 = 56,
    NUM_9 = 57,
 
    // --------------------------------------------------------
    // Letters  (match VK_A … VK_Z)
    // --------------------------------------------------------
    A = 65,  B = 66,  C = 67,  D = 68,  E = 69,
    F = 70,  G = 71,  H = 72,  I = 73,  J = 74,
    K = 75,  L = 76,  M = 77,  N = 78,  O = 79,
    P = 80,  Q = 81,  R = 82,  S = 83,  T = 84,
    U = 85,  V = 86,  W = 87,  X = 88,  Y = 89,
    Z = 90,
 
    // --------------------------------------------------------
    // System / OS keys
    // --------------------------------------------------------
    LEFT_SUPER  = 91,  // VK_LWIN
    RIGHT_SUPER = 92,  // VK_RWIN
    MENU        = 93,  // VK_APPS  (context-menu key)
 
    // --------------------------------------------------------
    // Numpad  (NumLock ON)
    // --------------------------------------------------------
    KP_0        = 96,  // VK_NUMPAD0
    KP_1        = 97,
    KP_2        = 98,
    KP_3        = 99,
    KP_4        = 100,
    KP_5        = 101,
    KP_6        = 102,
    KP_7        = 103,
    KP_8        = 104,
    KP_9        = 105,
    KP_MULTIPLY = 106, // VK_MULTIPLY
    KP_ADD      = 107, // VK_ADD
    KP_SUBTRACT = 109, // VK_SUBTRACT
    KP_DECIMAL  = 110, // VK_DECIMAL  (numpad '.')
    KP_DIVIDE   = 111, // VK_DIVIDE   (extended = 1)
    // KP_ENTER shares VK_RETURN (13) with extended bit = 1
    // Remapped to 255 by the translation layer (no VK equivalent)
    KP_ENTER    = 255,
 
    // --------------------------------------------------------
    // Function keys
    // --------------------------------------------------------
    F1  = 112, F2  = 113, F3  = 114, F4  = 115,
    F5  = 116, F6  = 117, F7  = 118, F8  = 119,
    F9  = 120, F10 = 121, F11 = 122, F12 = 123,
    F13 = 124, F14 = 125, F15 = 126, F16 = 127,
    F17 = 128, F18 = 129, F19 = 130, F20 = 131,
    F21 = 132, F22 = 133, F23 = 134, F24 = 135,
 
    // --------------------------------------------------------
    // Lock keys
    // --------------------------------------------------------
    NUM_LOCK    = 144, // VK_NUMLOCK
    SCROLL_LOCK = 145, // VK_SCROLL
 
    // --------------------------------------------------------
    // Modifiers — left/right variants
    // Win32VKToKeyCode always resolves to these.
    // The generic SHIFT/CTRL/ALT above are fallback only.
    // --------------------------------------------------------
    LEFT_SHIFT  = 160, // VK_LSHIFT
    RIGHT_SHIFT = 161, // VK_RSHIFT
    LEFT_CTRL   = 162, // VK_LCONTROL
    RIGHT_CTRL  = 163, // VK_RCONTROL
    LEFT_ALT    = 164, // VK_LMENU
    RIGHT_ALT   = 165, // VK_RMENU   (AltGr on intl. keyboards)
 
    // --------------------------------------------------------
    // Browser / media keys
    // --------------------------------------------------------
    BROWSER_BACK    = 166, // VK_BROWSER_BACK
    BROWSER_FORWARD = 167, // VK_BROWSER_FORWARD
    BROWSER_REFRESH = 168, // VK_BROWSER_REFRESH
    BROWSER_STOP    = 169, // VK_BROWSER_STOP
    BROWSER_SEARCH  = 170, // VK_BROWSER_SEARCH
    BROWSER_FAV     = 171, // VK_BROWSER_FAVORITES
    BROWSER_HOME    = 172, // VK_BROWSER_HOME
    VOLUME_MUTE     = 173, // VK_VOLUME_MUTE
    VOLUME_DOWN     = 174, // VK_VOLUME_DOWN
    VOLUME_UP       = 175, // VK_VOLUME_UP
    MEDIA_NEXT      = 176, // VK_MEDIA_NEXT_TRACK
    MEDIA_PREV      = 177, // VK_MEDIA_PREV_TRACK
    MEDIA_STOP      = 178, // VK_MEDIA_STOP
    MEDIA_PLAY      = 179, // VK_MEDIA_PLAY_PAUSE
 
    // --------------------------------------------------------
    // US-layout symbols  (VK_OEM_*)
    // --------------------------------------------------------
    SEMICOLON     = 186, // VK_OEM_1      ;:
    EQUAL         = 187, // VK_OEM_PLUS   =+
    COMMA         = 188, // VK_OEM_COMMA  ,<
    MINUS         = 189, // VK_OEM_MINUS  -_
    PERIOD        = 190, // VK_OEM_PERIOD .>
    SLASH         = 191, // VK_OEM_2      /?
    BACKQUOTE     = 192, // VK_OEM_3      `~
    LEFT_BRACKET  = 219, // VK_OEM_4      [{
    BACKSLASH     = 220, // VK_OEM_5      \|
    RIGHT_BRACKET = 221, // VK_OEM_6      ]}
    APOSTROPHE    = 222, // VK_OEM_7      '"
 
    KEY_UNKNOWN   = 254,
    // 255 is reserved for KP_ENTER above
};

} // namespace VoidEngine
