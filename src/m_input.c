//
//  Copyright(C) 2021 Roman Fomin
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
// DESCRIPTION:
//      Custom input

#include <string.h>
#include "m_input.h"
#include "doomkeys.h"

#define M_ARRAY_INIT_CAPACITY NUM_INPUTS
#include "m_array.h"

static input_t *composite_inputs[NUM_INPUT_ID];

extern boolean gamekeydown[];
extern boolean *mousebuttons;
extern boolean *joybuttons;

static event_t *event;

static boolean InputMatch(int id, input_type_t type, int value)
{
    input_t *inputs = composite_inputs[id];

    for (int i = 0; i < array_size(inputs); ++i)
    {
        if (inputs[i].type == type && inputs[i].value == value)
        {
            return true;
        }
    }
    return false;
}

static void InputRemove(int id, input_type_t type, int value)
{
    input_t *inputs = composite_inputs[id];

    for (int i = 0; i < array_size(inputs); ++i)
    {
        if (inputs[i].type == type && inputs[i].value == value)
        {
            int left = array_size(inputs) - i - 1;
            if (left > 0)
            {
                memmove(inputs + i, inputs + i + 1, left * sizeof(*inputs));
            }
            array_ptr(inputs)->size--;
        }
    }
}

static boolean InputAdd(int id, input_type_t type, int value)
{
    if (InputMatch(id, type, value))
        return true;

    input_t *inputs = composite_inputs[id];

    if (array_size(inputs) >= NUM_INPUTS)
        return false;

    input_t input = {type, value};
    array_push(inputs, input);
    composite_inputs[id] = inputs;
    return true;
}

const input_t *M_Input(int id)
{
    return composite_inputs[id];
}

boolean M_InputAddKey(int id, int value)
{
    return InputAdd(id, INPUT_KEY, value);
}

boolean M_InputAddMouseB(int id, int value)
{
    return InputAdd(id, INPUT_MOUSEB, value);
}

boolean M_InputAddJoyB(int id, int value)
{
    return InputAdd(id, INPUT_JOYB, value);
}

void M_InputTrackEvent(event_t *ev)
{
    event = ev;
}

boolean M_InputActivated(int id)
{
    switch (event->type)
    {
        case ev_keydown:
            return InputMatch(id, INPUT_KEY, event->data1);
        case ev_mouseb_down:
            return InputMatch(id, INPUT_MOUSEB, event->data1);
        case ev_joyb_down:
            return InputMatch(id, INPUT_JOYB, event->data1);
        default:
            return false;
    }
}

boolean M_InputDeactivated(int id)
{
    switch (event->type)
    {
        case ev_keyup:
            return InputMatch(id, INPUT_KEY, event->data1);
        case ev_mouseb_up:
            return InputMatch(id, INPUT_MOUSEB, event->data1);
        case ev_joyb_up:
            return InputMatch(id, INPUT_JOYB, event->data1);
        default:
            return false;
    }
}

boolean M_InputAddActivated(int id)
{
    switch (event->type)
    {
        case ev_keydown:
            return M_InputAddKey(id, event->data1);
        case ev_mouseb_down:
            return M_InputAddMouseB(id, event->data1);
        case ev_joyb_down:
            return M_InputAddJoyB(id, event->data1);
        default:
            return false;
    }
}

void M_InputRemoveActivated(int id)
{
    switch (event->type)
    {
        case ev_keydown:
            InputRemove(id, INPUT_KEY, event->data1);
            break;
        case ev_mouseb_down:
            InputRemove(id, INPUT_MOUSEB, event->data1);
            break;
        case ev_joyb_down:
            InputRemove(id, INPUT_JOYB, event->data1);
            break;
        default:
            break;
    }
}

static boolean InputActive(boolean *buttons, int id, input_type_t type)
{
    input_t *inputs = composite_inputs[id];

    for (int i = 0; i < array_size(inputs); ++i)
    {
        if (inputs[i].type == type && buttons[inputs[i].value])
        {
            return true;
        }
    }
    return false;
}

boolean M_InputGameActive(int id)
{
    return InputActive(gamekeydown, id, INPUT_KEY) ||
           InputActive(mousebuttons, id, INPUT_MOUSEB) ||
           InputActive(joybuttons, id, INPUT_JOYB);
}

void M_InputGameDeactivate(int id)
{
    input_t *inputs = composite_inputs[id];

    for (int i = 0; i < array_size(inputs); ++i)
    {
        switch (inputs[i].type)
        {
            case INPUT_KEY:
                gamekeydown[inputs[i].value] = false;
                break;
            case INPUT_MOUSEB:
                mousebuttons[inputs[i].value] = false;
                break;
            case INPUT_JOYB:
                joybuttons[inputs[i].value] = false;
                break;
            default:
                break;
        }
    }
}

void M_InputReset(int id)
{
    input_t *inputs = composite_inputs[id];

    array_clear(inputs);
}

void M_InputSet(int id, input_t *inputs)
{
    input_t *local_inputs = composite_inputs[id];

    array_clear(local_inputs);

    for (int i = 0; i < NUM_INPUTS; ++i)
    {
        if (inputs[i].type > INPUT_NULL)
        {
            array_push(local_inputs, inputs[i]);
        }
    }
    composite_inputs[id] = local_inputs;
}

static const struct
{
    int key;
    const char* name;
} key_names[] = {
    { 0,              "none" },
    { KEY_TAB,        "tab" },
    { KEY_ENTER,      "enter" },
    { KEY_ESCAPE,     "esc" },
    { ' ',            "spacebar" },
    { KEY_BACKSPACE,  "backspace" },
    { KEY_RCTRL,      "ctrl" },
    { KEY_LEFTARROW,  "leftarrow" },
    { KEY_UPARROW,    "uparrow" },
    { KEY_RIGHTARROW, "rightarrow" },
    { KEY_DOWNARROW,  "downarrow" },
    { KEY_RSHIFT,     "shift"},
    { KEY_RALT,       "alt" },
    { KEY_CAPSLOCK,   "capslock" },
    { KEY_F1,         "f1" },
    { KEY_F2,         "f2" },
    { KEY_F3,         "f3" },
    { KEY_F4,         "f4" },
    { KEY_F5,         "f5" },
    { KEY_F6,         "f6" },
    { KEY_F7,         "f7" },
    { KEY_F8,         "f8" },
    { KEY_F9,         "f9" },
    { KEY_F10,        "f10" },
    { KEY_F11,        "f11" },
    { KEY_F12,        "f12" },
    { KEY_SCRLCK,     "scrolllock" },
    { KEY_HOME,       "home" },
    { KEY_PGUP,       "pageup" },
    { KEY_END,        "end"  },
    { KEY_PGDN,       "pagedown" },
    { KEY_INS,        "insert" },
    { KEY_PAUSE,      "pause" },
    { KEY_DEL,        "del" },
    { KEY_PRTSCR,     "prtscr" },
    { KEYP_0,         "num0" },
    { KEYP_1,         "num1" },
    { KEYP_2,         "num2" },
    { KEYP_3,         "num3" },
    { KEYP_4,         "num4" },
    { KEYP_5,         "num5" },
    { KEYP_6,         "num6" },
    { KEYP_7,         "num7" },
    { KEYP_8,         "num8" },
    { KEYP_9,         "num9" },
    { KEYP_DIVIDE,    "num/" },
    { KEYP_PLUS,      "num+" },
    { KEYP_MINUS,     "num-" },
    { KEYP_MULTIPLY,  "num*" },
    { KEYP_PERIOD,    "num." },
};

static const struct
{
    int joyb;
    const char* name;
} joyb_names[] = {
    { CONTROLLER_A,                 "pada" },
    { CONTROLLER_B,                 "padb" },
    { CONTROLLER_X,                 "padx" },
    { CONTROLLER_Y,                 "pady" },
    { CONTROLLER_BACK,              "back" },
    { CONTROLLER_GUIDE,             "guide" },
    { CONTROLLER_START,             "start" },
    { CONTROLLER_LEFT_STICK,        "ls" },
    { CONTROLLER_RIGHT_STICK,       "rs" },
    { CONTROLLER_LEFT_SHOULDER,     "lb" },
    { CONTROLLER_RIGHT_SHOULDER,    "rb" }, 
    { CONTROLLER_DPAD_UP,           "padup" },
    { CONTROLLER_DPAD_DOWN,         "paddown" },
    { CONTROLLER_DPAD_LEFT,         "padleft" },
    { CONTROLLER_DPAD_RIGHT,        "padright" },
    { CONTROLLER_MISC1,             "misc1" },
    { CONTROLLER_PADDLE1,           "paddle1" },
    { CONTROLLER_PADDLE2,           "paddle2" },
    { CONTROLLER_PADDLE3,           "paddle3" },
    { CONTROLLER_PADDLE4,           "paddle4" },
    { CONTROLLER_TOUCHPAD,          "touch" },
    { CONTROLLER_LEFT_TRIGGER,      "lt" },
    { CONTROLLER_RIGHT_TRIGGER,     "rt" },
    { CONTROLLER_LEFT_STICK_UP,     "lsup" },
    { CONTROLLER_LEFT_STICK_DOWN,   "lsdown" },
    { CONTROLLER_LEFT_STICK_LEFT,   "lsleft" },
    { CONTROLLER_LEFT_STICK_RIGHT,  "lsright" },
    { CONTROLLER_RIGHT_STICK_UP,    "rsup" },
    { CONTROLLER_RIGHT_STICK_DOWN,  "rsdown" },
    { CONTROLLER_RIGHT_STICK_LEFT,  "rsleft" },
    { CONTROLLER_RIGHT_STICK_RIGHT, "rsright" },
};

static const struct
{
    int mouseb;
    const char* name;
} mouseb_names[] = {
    { MOUSE_BUTTON_LEFT,      "mouse1" },
    { MOUSE_BUTTON_RIGHT,     "mouse2" },
    { MOUSE_BUTTON_MIDDLE,    "mouse3" },
    { MOUSE_BUTTON_X1,        "mouse4" },
    { MOUSE_BUTTON_X2,        "mouse5" },
    { MOUSE_BUTTON_WHEELUP,   "wheelup" },
    { MOUSE_BUTTON_WHEELDOWN, "wheeldown" },
    { MOUSE_BUTTON_WHEELLEFT, "wheelleft" },
    { MOUSE_BUTTON_WHEELRIGHT,"wheelright" },
};

const char* M_GetNameForKey(int key)
{
    for (int i = 0; i < arrlen(key_names); ++i)
    {
        if (key_names[i].key == key)
            return key_names[i].name;
    }
    return NULL;
}

int M_GetKeyForName(const char* name)
{
    for (int i = 0; i < arrlen(key_names); ++i)
    {
        if (strcasecmp(name, key_names[i].name) == 0)
            return key_names[i].key;
    }
    return 0;
}

const char* M_GetNameForJoyB(int joyb)
{
    for (int i = 0; i < arrlen(joyb_names); ++i)
    {
        if (joyb_names[i].joyb == joyb)
            return joyb_names[i].name;
    }
    return NULL;
}

int M_GetJoyBForName(const char* name)
{
    for (int i = 0; i < arrlen(joyb_names); ++i)
    {
        if (strcasecmp(name, joyb_names[i].name) == 0)
            return joyb_names[i].joyb;
    }
    return -1;
}

const char* M_GetNameForMouseB(int mouseb)
{
    for (int i = 0; i < arrlen(mouseb_names); ++i)
    {
        if (mouseb_names[i].mouseb == mouseb)
            return mouseb_names[i].name;
    }
    return NULL;
}

int M_GetMouseBForName(const char* name)
{
    for (int i = 0; i < arrlen(mouseb_names); ++i)
    {
        if (strcasecmp(name, mouseb_names[i].name) == 0)
            return mouseb_names[i].mouseb;
    }
    return -1;
}

boolean M_IsMouseWheel(int mouseb)
{
    return mouseb >= MOUSE_BUTTON_WHEELUP &&
           mouseb <= MOUSE_BUTTON_WHEELRIGHT;
}
