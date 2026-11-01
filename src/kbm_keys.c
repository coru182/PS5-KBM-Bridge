/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "kbm_keys.h"
#include <ctype.h>
#include <stddef.h>

typedef struct { uint8_t code; const char *name; } KeyName;

static const KeyName NAMES[] = {
    {0x04,"A"},{0x05,"B"},{0x06,"C"},{0x07,"D"},{0x08,"E"},{0x09,"F"},{0x0a,"G"},
    {0x0b,"H"},{0x0c,"I"},{0x0d,"J"},{0x0e,"K"},{0x0f,"L"},{0x10,"M"},{0x11,"N"},
    {0x12,"O"},{0x13,"P"},{0x14,"Q"},{0x15,"R"},{0x16,"S"},{0x17,"T"},{0x18,"U"},
    {0x19,"V"},{0x1a,"W"},{0x1b,"X"},{0x1c,"Y"},{0x1d,"Z"},
    {0x1e,"1"},{0x1f,"2"},{0x20,"3"},{0x21,"4"},{0x22,"5"},{0x23,"6"},{0x24,"7"},
    {0x25,"8"},{0x26,"9"},{0x27,"0"},
    {0x28,"ENTER"},{0x29,"ESC"},{0x2a,"BACKSPACE"},{0x2b,"TAB"},{0x2c,"SPACE"},
    {0x2d,"MINUS"},{0x2e,"EQUAL"},{0x2f,"LBRACKET"},{0x30,"RBRACKET"},{0x31,"BACKSLASH"},
    {0x33,"SEMICOLON"},{0x34,"QUOTE"},{0x35,"GRAVE"},{0x36,"COMMA"},{0x37,"PERIOD"},
    {0x38,"SLASH"},{0x39,"CAPSLOCK"},
    {0x3a,"F1"},{0x3b,"F2"},{0x3c,"F3"},{0x3d,"F4"},{0x3e,"F5"},{0x3f,"F6"},{0x40,"F7"},
    {0x41,"F8"},{0x42,"F9"},{0x43,"F10"},{0x44,"F11"},{0x45,"F12"},
    {0x46,"PRINTSCREEN"},{0x47,"SCROLLLOCK"},{0x48,"PAUSE"},{0x49,"INSERT"},{0x4a,"HOME"},
    {0x4b,"PAGEUP"},{0x4c,"DELETE"},{0x4d,"END"},{0x4e,"PAGEDOWN"},
    {0x4f,"RIGHT"},{0x50,"LEFT"},{0x51,"DOWN"},{0x52,"UP"},
    {0x53,"NUMLOCK"},{0x54,"KP_SLASH"},{0x55,"KP_STAR"},{0x56,"KP_MINUS"},{0x57,"KP_PLUS"},
    {0x58,"KP_ENTER"},{0x59,"KP1"},{0x5a,"KP2"},{0x5b,"KP3"},{0x5c,"KP4"},{0x5d,"KP5"},
    {0x5e,"KP6"},{0x5f,"KP7"},{0x60,"KP8"},{0x61,"KP9"},{0x62,"KP0"},{0x63,"KP_DOT"},
    {0x64,"ISO_BACKSLASH"},{0x65,"MENU"},
    {0xe0,"LCTRL"},{0xe1,"LSHIFT"},{0xe2,"LALT"},{0xe3,"LGUI"},
    {0xe4,"RCTRL"},{0xe5,"RSHIFT"},{0xe6,"RALT"},{0xe7,"RGUI"},
    {KBM_KEY_MOUSE1,"MOUSE1"},{KBM_KEY_MOUSE2,"MOUSE2"},{KBM_KEY_MOUSE3,"MOUSE3"},
    {KBM_KEY_MOUSE4,"MOUSE4"},{KBM_KEY_MOUSE5,"MOUSE5"},
    {KBM_KEY_WHEEL_UP,"WHEEL_UP"},{KBM_KEY_WHEEL_DOWN,"WHEEL_DOWN"},
};
#define N_NAMES (sizeof(NAMES) / sizeof(NAMES[0]))

const char *kbm_key_name(uint8_t code) {
    for (size_t i = 0; i < N_NAMES; ++i) if (NAMES[i].code == code) return NAMES[i].name;
    return NULL;
}

static int ieq(const char *a, const char *b) {
    while (*a && *b) {
        if (toupper((unsigned char)*a) != toupper((unsigned char)*b)) return 0;
        ++a; ++b;
    }
    return *a == 0 && *b == 0;
}

int kbm_key_from_name(const char *name) {
    if (!name || !*name || ieq(name, "NONE")) return KBM_KEY_NONE;
    for (size_t i = 0; i < N_NAMES; ++i) if (ieq(NAMES[i].name, name)) return NAMES[i].code;
    return -1;
}

int kbm_key_reserved(uint8_t code) { return code == KBM_KEY_F8 || code == KBM_KEY_F9 || code == KBM_KEY_F10 || code == KBM_KEY_F11 || code == KBM_KEY_F12; }
