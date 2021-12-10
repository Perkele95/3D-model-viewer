#pragma once

#include "mv_utils/vec.hpp"

using key_t = uint8_t;

struct input_state
{
    uint32_t keyPressEvents;
    uint32_t mousePressEvents;
    key_t keyBoard[0xFFUI8];
    int32_t mouseWheel;

    vec2<int32_t> mouse, mousePrev, mouseDelta;
};

// Volunds keycodes are identical to windows platform keycodes

enum class KeyCode : key_t
{
    LMBUTTON = 0x01,
    RMBUTTON = 0x02,
    SHIFT = 0x10,
    CTRL = 0x11,
    ALT = 0x12,
    NUM0 = 0x30,
    NUM1 = 0x31,
    NUM2 = 0x32,
    NUM3 = 0x33,
    NUM4 = 0x34,
    NUM5 = 0x35,
    NUM6 = 0x36,
    NUM7 = 0x37,
    NUM8 = 0x38,
    NUM9 = 0x39,
    A = 0x41,
    D = 0x44,
    S = 0x53,
    W = 0x57,
};

enum KEY_EVENT_BITS
{
    KEY_EVENT_W = BIT(0),
    KEY_EVENT_S = BIT(1),
    KEY_EVENT_A = BIT(2),
    KEY_EVENT_D = BIT(3),
    KEY_EVENT_Q = BIT(4),
    KEY_EVENT_E = BIT(5),
    KEY_EVENT_UP = BIT(6),
    KEY_EVENT_DOWN = BIT(7),
    KEY_EVENT_LEFT = BIT(8),
    KEY_EVENT_RIGHT = BIT(9),
    KEY_EVENT_SPACE = BIT(10),
    KEY_EVENT_ESCAPE = BIT(11),
    KEY_EVENT_CONTROL = BIT(12),
    KEY_EVENT_SHIFT = BIT(13),
    KEY_EVENT_F = BIT(14),
};

enum MOUSE_EVENT_BITS
{
    MOUSE_EVENT_MOVE = BIT(0),
    MOUSE_EVENT_LMB = BIT(1),
    MOUSE_EVENT_RMB = BIT(2),
    MOUSE_EVENT_MMB = BIT(3),
};