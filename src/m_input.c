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

#include "d_event.h"
#include "doomkeys.h"
#include "g_game.h"
#include "m_input.h"
#include "m_config.h"
#include "m_misc.h"

#define M_ARRAY_INIT_CAPACITY NUM_INPUTS
#include "m_array.h"

static input_t *composite_inputs[NUM_INPUT_ID];

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
            array_delete(inputs, i);
        }
    }
}

static boolean InputAdd(int id, input_type_t type, int value)
{
    if (InputMatch(id, type, value))
    {
        return true;
    }

    input_t *inputs = composite_inputs[id];

    if (array_size(inputs) >= NUM_INPUTS)
    {
        array_clear(inputs);
    }

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
            return InputMatch(id, INPUT_KEY, event->data1.i);
        case ev_mouseb_down:
            return InputMatch(id, INPUT_MOUSEB, event->data1.i);
        case ev_joyb_down:
            return InputMatch(id, INPUT_JOYB, event->data1.i);
        default:
            return false;
    }
}

boolean M_InputDeactivated(int id)
{
    switch (event->type)
    {
        case ev_keyup:
            return InputMatch(id, INPUT_KEY, event->data1.i);
        case ev_mouseb_up:
            return InputMatch(id, INPUT_MOUSEB, event->data1.i);
        case ev_joyb_up:
            return InputMatch(id, INPUT_JOYB, event->data1.i);
        default:
            return false;
    }
}

boolean M_InputAddActivated(int id)
{
    switch (event->type)
    {
        case ev_keydown:
            return M_InputAddKey(id, event->data1.i);
        case ev_mouseb_down:
            return M_InputAddMouseB(id, event->data1.i);
        case ev_joyb_down:
            return M_InputAddJoyB(id, event->data1.i);
        default:
            return false;
    }
}

void M_InputRemoveActivated(int id)
{
    switch (event->type)
    {
        case ev_keydown:
            InputRemove(id, INPUT_KEY, event->data1.i);
            break;
        case ev_mouseb_down:
            InputRemove(id, INPUT_MOUSEB, event->data1.i);
            break;
        case ev_joyb_down:
            InputRemove(id, INPUT_JOYB, event->data1.i);
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
    return InputActive(gamekeydown, id, INPUT_KEY)
           || InputActive(mousebuttons, id, INPUT_MOUSEB)
           || InputActive(joybuttons, id, INPUT_JOYB);
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

static void InputSet(int id, input_t *inputs, int size)
{
    input_t *local_inputs = composite_inputs[id];

    array_clear(local_inputs);

    for (int i = 0; i < size; ++i)
    {
        if (inputs[i].type > INPUT_NULL)
        {
            array_push(local_inputs, inputs[i]);
        }
        else
        {
            break;
        }
    }
    composite_inputs[id] = local_inputs;
}

static const struct
{
    int key;
    const char *name;
} key_names[] = {
    {0,              "none"      },
    {KEY_TAB,        "tab"       },
    {KEY_ENTER,      "enter"     },
    {KEY_ESCAPE,     "esc"       },
    {' ',            "spacebar"  },
    {KEY_BACKSPACE,  "backspace" },
    {KEY_RCTRL,      "ctrl"      },
    {KEY_LEFTARROW,  "leftarrow" },
    {KEY_UPARROW,    "uparrow"   },
    {KEY_RIGHTARROW, "rightarrow"},
    {KEY_DOWNARROW,  "downarrow" },
    {KEY_RSHIFT,     "shift"     },
    {KEY_RALT,       "alt"       },
    {KEY_CAPSLOCK,   "capslock"  },
    {KEY_F1,         "f1"        },
    {KEY_F2,         "f2"        },
    {KEY_F3,         "f3"        },
    {KEY_F4,         "f4"        },
    {KEY_F5,         "f5"        },
    {KEY_F6,         "f6"        },
    {KEY_F7,         "f7"        },
    {KEY_F8,         "f8"        },
    {KEY_F9,         "f9"        },
    {KEY_F10,        "f10"       },
    {KEY_F11,        "f11"       },
    {KEY_F12,        "f12"       },
    {KEY_SCRLCK,     "scrolllock"},
    {KEY_HOME,       "home"      },
    {KEY_PGUP,       "pageup"    },
    {KEY_END,        "end"       },
    {KEY_PGDN,       "pagedown"  },
    {KEY_INS,        "insert"    },
    {KEY_PAUSE,      "pause"     },
    {KEY_DEL,        "del"       },
    {KEY_PRTSCR,     "prtscr"    },
    {KEYP_0,         "num0"      },
    {KEYP_1,         "num1"      },
    {KEYP_2,         "num2"      },
    {KEYP_3,         "num3"      },
    {KEYP_4,         "num4"      },
    {KEYP_5,         "num5"      },
    {KEYP_6,         "num6"      },
    {KEYP_7,         "num7"      },
    {KEYP_8,         "num8"      },
    {KEYP_9,         "num9"      },
    {KEYP_DIVIDE,    "num/"      },
    {KEYP_PLUS,      "num+"      },
    {KEYP_MINUS,     "num-"      },
    {KEYP_MULTIPLY,  "num*"      },
    {KEYP_PERIOD,    "num."      },
    {KEYP_BACKSLASH, "num\\"     },
};

#define JOYB_LEN 16
#define JOYB_COPY(x, y) M_StringCopy(joyb_platform_names[(x)], (y), JOYB_LEN)
static char joyb_platform_names[NUM_GAMEPAD_BUTTONS][JOYB_LEN];

static const char joyb_names[NUM_GAMEPAD_BUTTONS][JOYB_LEN] = {
    [GAMEPAD_SOUTH]                = "pada",
    [GAMEPAD_EAST]                 = "padb",
    [GAMEPAD_WEST]                 = "padx",
    [GAMEPAD_NORTH]                = "pady",
    [GAMEPAD_BACK]                 = "back",
    [GAMEPAD_GUIDE]                = "guide",
    [GAMEPAD_START]                = "start",
    [GAMEPAD_LEFT_STICK]           = "ls",
    [GAMEPAD_RIGHT_STICK]          = "rs",
    [GAMEPAD_LEFT_SHOULDER]        = "lb",
    [GAMEPAD_RIGHT_SHOULDER]       = "rb",
    [GAMEPAD_DPAD_UP]              = "padup",
    [GAMEPAD_DPAD_DOWN]            = "paddown",
    [GAMEPAD_DPAD_LEFT]            = "padleft",
    [GAMEPAD_DPAD_RIGHT]           = "padright",
    [GAMEPAD_MISC1]                = "misc1",
    [GAMEPAD_RIGHT_PADDLE1]        = "rpaddle1",
    [GAMEPAD_LEFT_PADDLE1]         = "lpaddle1",
    [GAMEPAD_RIGHT_PADDLE2]        = "rpaddle2",
    [GAMEPAD_LEFT_PADDLE2]         = "lpaddle2",
    [GAMEPAD_TOUCHPAD_PRESS]       = "tppress",
    [GAMEPAD_MISC2]                = "misc2",
    [GAMEPAD_MISC3]                = "misc3",
    [GAMEPAD_MISC4]                = "misc4",
    [GAMEPAD_MISC5]                = "misc5",
    [GAMEPAD_MISC6]                = "misc6",
    [GAMEPAD_TOUCHPAD_TOUCH]       = "tptouch",
    [GAMEPAD_LEFT_TRIGGER]         = "lt",
    [GAMEPAD_RIGHT_TRIGGER]        = "rt",
    [GAMEPAD_LEFT_STICK_UP]        = "lsup",
    [GAMEPAD_LEFT_STICK_DOWN]      = "lsdown",
    [GAMEPAD_LEFT_STICK_LEFT]      = "lsleft",
    [GAMEPAD_LEFT_STICK_RIGHT]     = "lsright",
    [GAMEPAD_RIGHT_STICK_UP]       = "rsup",
    [GAMEPAD_RIGHT_STICK_DOWN]     = "rsdown",
    [GAMEPAD_RIGHT_STICK_LEFT]     = "rsleft",
    [GAMEPAD_RIGHT_STICK_RIGHT]    = "rsright",
};

static void M_UpdateConfirmCancel(boolean swap_confirm);

void M_UpdatePlatform(joy_platform_t platform)
{
    boolean swap_confirm = false;

    switch ((int)joy_confirm)
    {
        case CONFIRM_AUTO:
            switch ((int)platform)
            {
                case PLATFORM_SWITCH_PRO:
                case PLATFORM_SWITCH_JOYCON_LEFT:
                case PLATFORM_SWITCH_JOYCON_RIGHT:
                case PLATFORM_SWITCH_JOYCON_PAIR:
                    swap_confirm = true;
                    break;
            }
            break;

        case CONFIRM_EAST:
            swap_confirm = true;
            break;
    }

    M_UpdateConfirmCancel(swap_confirm);

    for (int i = 0; i < arrlen(joyb_names); i++)
    {
        JOYB_COPY(i, joyb_names[i]);
    }

    switch ((int)platform)
    {
        case PLATFORM_XBOXONE:
            JOYB_COPY(GAMEPAD_BACK, "view");
            JOYB_COPY(GAMEPAD_GUIDE, "xbbutton");
            JOYB_COPY(GAMEPAD_START, "menu");
            break;

        case PLATFORM_PS3:
        case PLATFORM_PS4:
        case PLATFORM_PS5:
            JOYB_COPY(GAMEPAD_SOUTH, "cross");
            JOYB_COPY(GAMEPAD_EAST, "circle");
            JOYB_COPY(GAMEPAD_WEST, "square");
            JOYB_COPY(GAMEPAD_NORTH, "triangle");
            JOYB_COPY(GAMEPAD_GUIDE, "psbutton");
            JOYB_COPY(GAMEPAD_LEFT_STICK, "L3");
            JOYB_COPY(GAMEPAD_RIGHT_STICK, "R3");
            JOYB_COPY(GAMEPAD_LEFT_SHOULDER, "L1");
            JOYB_COPY(GAMEPAD_RIGHT_SHOULDER, "R1");
            JOYB_COPY(GAMEPAD_LEFT_TRIGGER, "L2");
            JOYB_COPY(GAMEPAD_RIGHT_TRIGGER, "R2");
            switch ((int)platform)
            {
                case PLATFORM_PS3:
                    JOYB_COPY(GAMEPAD_BACK, "select");
                    JOYB_COPY(GAMEPAD_START, "start");
                    break;

                case PLATFORM_PS4:
                    JOYB_COPY(GAMEPAD_BACK, "share");
                    JOYB_COPY(GAMEPAD_START, "options");
                    break;

                case PLATFORM_PS5:
                    JOYB_COPY(GAMEPAD_BACK, "create");
                    JOYB_COPY(GAMEPAD_START, "options");
                    JOYB_COPY(GAMEPAD_MISC1, "mute");
                    JOYB_COPY(GAMEPAD_RIGHT_PADDLE1, "RB");
                    JOYB_COPY(GAMEPAD_LEFT_PADDLE1, "LB");
                    JOYB_COPY(GAMEPAD_RIGHT_PADDLE2, "R.Fn");
                    JOYB_COPY(GAMEPAD_LEFT_PADDLE2, "L.Fn");
                    break;
            }
            break;

        case PLATFORM_SWITCH_PRO:
        case PLATFORM_SWITCH_JOYCON_LEFT:
        case PLATFORM_SWITCH_JOYCON_RIGHT:
        case PLATFORM_SWITCH_JOYCON_PAIR:
            JOYB_COPY(GAMEPAD_SOUTH, "padb");
            JOYB_COPY(GAMEPAD_EAST, "pada");
            JOYB_COPY(GAMEPAD_WEST, "pady");
            JOYB_COPY(GAMEPAD_NORTH, "padx");
            JOYB_COPY(GAMEPAD_BACK, "pad-");
            JOYB_COPY(GAMEPAD_GUIDE, "padhome");
            JOYB_COPY(GAMEPAD_START, "pad+");
            JOYB_COPY(GAMEPAD_MISC1, "capture");
            JOYB_COPY(GAMEPAD_LEFT_TRIGGER, "ZL");
            JOYB_COPY(GAMEPAD_RIGHT_TRIGGER, "ZR");
            switch ((int)platform)
            {
                case PLATFORM_SWITCH_JOYCON_LEFT:
                    JOYB_COPY(GAMEPAD_GUIDE, "capture");
                    JOYB_COPY(GAMEPAD_START, "pad-");
                    JOYB_COPY(GAMEPAD_LEFT_SHOULDER, "L.SL");
                    JOYB_COPY(GAMEPAD_RIGHT_SHOULDER, "L.SR");
                    JOYB_COPY(GAMEPAD_LEFT_PADDLE1, "L.LB");
                    JOYB_COPY(GAMEPAD_LEFT_PADDLE2, "L.ZL");
                    break;

                case PLATFORM_SWITCH_JOYCON_RIGHT:
                    JOYB_COPY(GAMEPAD_GUIDE, "padhome");
                    JOYB_COPY(GAMEPAD_START, "pad+");
                    JOYB_COPY(GAMEPAD_LEFT_SHOULDER, "R.SL");
                    JOYB_COPY(GAMEPAD_RIGHT_SHOULDER, "R.SR");
                    JOYB_COPY(GAMEPAD_RIGHT_PADDLE1, "R.RB");
                    JOYB_COPY(GAMEPAD_RIGHT_PADDLE2, "R.ZR");
                    break;

                case PLATFORM_SWITCH_JOYCON_PAIR:
                    JOYB_COPY(GAMEPAD_RIGHT_PADDLE1, "R.SR");
                    JOYB_COPY(GAMEPAD_LEFT_PADDLE1, "L.SL");
                    JOYB_COPY(GAMEPAD_RIGHT_PADDLE2, "R.SL");
                    JOYB_COPY(GAMEPAD_LEFT_PADDLE2, "L.SR");
                    break;
            }
            break;
    }
}

static const char *mouseb_names[] = {
    [MOUSE_BUTTON_LEFT]       = "mouse1",
    [MOUSE_BUTTON_RIGHT]      = "mouse2",
    [MOUSE_BUTTON_MIDDLE]     = "mouse3",
    [MOUSE_BUTTON_X1]         = "mouse4",
    [MOUSE_BUTTON_X2]         = "mouse5",
    [MOUSE_BUTTON_WHEELUP]    = "wheelup",
    [MOUSE_BUTTON_WHEELDOWN]  = "wheeldown",
    [MOUSE_BUTTON_WHEELLEFT]  = "wheelleft",
    [MOUSE_BUTTON_WHEELRIGHT] = "wheelright",
};

const char *M_GetNameForKey(int key)
{
    for (int i = 0; i < arrlen(key_names); ++i)
    {
        if (key_names[i].key == key)
        {
            return key_names[i].name;
        }
    }
    return NULL;
}

int M_GetKeyForName(const char *name)
{
    for (int i = 0; i < arrlen(key_names); ++i)
    {
        if (strcasecmp(name, key_names[i].name) == 0)
        {
            return key_names[i].key;
        }
    }
    return 0;
}

const char *M_GetPlatformName(int joyb)
{
    if (joyb >= 0 && joyb < arrlen(joyb_platform_names))
    {
        return joyb_platform_names[joyb];
    }
    return NULL;
}

const char *M_GetNameForJoyB(int joyb)
{
    if (joyb >= 0 && joyb < arrlen(joyb_names))
    {
        return joyb_names[joyb];
    }
    return NULL;
}

int M_GetJoyBForName(const char *name)
{
    for (int i = 0; i < arrlen(joyb_names); ++i)
    {
        if (strcasecmp(name, joyb_names[i]) == 0)
        {
            return i;
        }
    }
    return -1;
}

const char *M_GetNameForMouseB(int mouseb)
{
    if (mouseb >= 0 && mouseb < arrlen(mouseb_names))
    {
        return mouseb_names[mouseb];
    }
    return NULL;
}

int M_GetMouseBForName(const char *name)
{
    for (int i = 0; i < arrlen(mouseb_names); ++i)
    {
        if (strcasecmp(name, mouseb_names[i]) == 0)
        {
            return i;
        }
    }
    return -1;
}

boolean M_IsMouseWheel(int mouseb)
{
    return mouseb >= MOUSE_BUTTON_WHEELUP && mouseb <= MOUSE_BUTTON_WHEELRIGHT;
}

static void M_UpdateConfirmCancel(boolean swap_confirm)
{
    if (swap_confirm)
    {
        gamepad_confirm = GAMEPAD_EAST;
        gamepad_cancel = GAMEPAD_SOUTH;
    }
    else
    {
        gamepad_confirm = GAMEPAD_SOUTH;
        gamepad_cancel = GAMEPAD_EAST;
    }

    input_t back[] = {
        {INPUT_KEY,    KEY_BACKSPACE     },
        {INPUT_JOYB,   gamepad_cancel    },
        {INPUT_MOUSEB, MOUSE_BUTTON_RIGHT}
    };

    input_t enter[] = {
        {INPUT_KEY,    KEY_ENTER        },
        {INPUT_JOYB,   gamepad_confirm  },
        {INPUT_MOUSEB, MOUSE_BUTTON_LEFT}
    };

    InputSet(input_menu_backspace, back, arrlen(back));
    InputSet(input_menu_enter, enter, arrlen(enter));
}

void M_InputPredefined(void)
{
    input_t right[] = {
        {INPUT_KEY,    KEY_RIGHTARROW             },
        {INPUT_JOYB,   GAMEPAD_DPAD_RIGHT         },
        {INPUT_JOYB,   GAMEPAD_LEFT_STICK_RIGHT   },
        {INPUT_JOYB,   GAMEPAD_RIGHT_SHOULDER     },
        {INPUT_MOUSEB, MOUSE_BUTTON_WHEELDOWN     }
    };
    InputSet(input_menu_right, right, arrlen(right));

    input_t left[] = {
        {INPUT_KEY,    KEY_LEFTARROW             },
        {INPUT_JOYB,   GAMEPAD_DPAD_LEFT         },
        {INPUT_JOYB,   GAMEPAD_LEFT_STICK_LEFT   },
        {INPUT_JOYB,   GAMEPAD_LEFT_SHOULDER     },
        {INPUT_MOUSEB, MOUSE_BUTTON_WHEELUP      },
    };
    InputSet(input_menu_left, left, arrlen(left));

    input_t up[] = {
        {INPUT_KEY,  KEY_UPARROW             },
        {INPUT_JOYB, GAMEPAD_DPAD_UP         },
        {INPUT_JOYB, GAMEPAD_LEFT_STICK_UP   }
    };
    InputSet(input_menu_up, up, arrlen(up));

    input_t down[] = {
        {INPUT_KEY,  KEY_DOWNARROW             },
        {INPUT_JOYB, GAMEPAD_DPAD_DOWN         },
        {INPUT_JOYB, GAMEPAD_LEFT_STICK_DOWN   }
    };
    InputSet(input_menu_down, down, arrlen(down));

    input_t esc[] = {
        {INPUT_KEY,  KEY_ESCAPE      },
        {INPUT_JOYB, GAMEPAD_START   }
    };
    InputSet(input_menu_escape, esc, arrlen(esc));

    input_t clear[] = {
        {INPUT_KEY,  KEY_DEL      },
        {INPUT_JOYB, GAMEPAD_NORTH}
    };
    InputSet(input_menu_clear, clear, arrlen(clear));

    M_UpdateConfirmCancel(false);

    M_InputAddKey(input_help, KEY_F1);
    M_InputAddKey(input_escape, KEY_ESCAPE);
    M_InputAddKey(input_chat_backspace, KEY_BACKSPACE);
    M_InputAddKey(input_chat_enter, KEY_ENTER);
}

static input_t default_inputs[NUM_INPUT_ID][NUM_INPUTS] =
{
    [input_forward]     = { {INPUT_KEY, 'w'}, {INPUT_KEY, KEY_UPARROW} },
    [input_backward]    = { {INPUT_KEY, 's'}, {INPUT_KEY, KEY_DOWNARROW} },
    [input_turnright]   = { {INPUT_KEY, KEY_RIGHTARROW} },
    [input_turnleft]    = { {INPUT_KEY, KEY_LEFTARROW} },
    [input_strafeleft]  = { {INPUT_KEY, 'a'} },
    [input_straferight] = { {INPUT_KEY, 'd'} },
    [input_speed]       = { {INPUT_KEY, KEY_RSHIFT} },
    [input_strafe]      = { {INPUT_KEY, KEY_RALT},
                            {INPUT_MOUSEB, MOUSE_BUTTON_RIGHT} },
    [input_autorun]     = { {INPUT_KEY, KEY_CAPSLOCK},
                            {INPUT_JOYB, GAMEPAD_LEFT_STICK} },
    [input_reverse]     = { {INPUT_KEY, '/'} },
    [input_gyro]        = { {INPUT_JOYB, GAMEPAD_TOUCHPAD_TOUCH},
                            {INPUT_JOYB, GAMEPAD_LEFT_TRIGGER} },
    [input_use]         = { {INPUT_KEY,' '},
                            {INPUT_JOYB, GAMEPAD_SOUTH} },
    [input_fire]        = { {INPUT_KEY, KEY_RCTRL},
                            {INPUT_MOUSEB, MOUSE_BUTTON_LEFT},
                            {INPUT_JOYB, GAMEPAD_RIGHT_TRIGGER} },
    [input_prevweapon]  = { {INPUT_MOUSEB, MOUSE_BUTTON_WHEELDOWN},
                            {INPUT_JOYB, GAMEPAD_LEFT_SHOULDER} },
    [input_nextweapon]  = { {INPUT_MOUSEB, MOUSE_BUTTON_WHEELUP},
                            {INPUT_JOYB, GAMEPAD_RIGHT_SHOULDER} },
    [input_weapon1]     = { {INPUT_KEY, '1'} },
    [input_weapon2]     = { {INPUT_KEY, '2'} },
    [input_weapon3]     = { {INPUT_KEY, '3'} },
    [input_weapon4]     = { {INPUT_KEY, '4'} },
    [input_weapon5]     = { {INPUT_KEY, '5'} },
    [input_weapon6]     = { {INPUT_KEY, '6'} },
    [input_weapon7]     = { {INPUT_KEY, '7'} },
    [input_weapon8]     = { {INPUT_KEY, '8'} },
    [input_weapon9]     = { {INPUT_KEY, '9'} },
    [input_weapontoggle] = { {INPUT_KEY, '0'} },

    [input_savegame]    = { {INPUT_KEY, KEY_F2} },
    [input_loadgame]    = { {INPUT_KEY, KEY_F3} },
    [input_soundvolume] = { {INPUT_KEY, KEY_F4} },
    [input_hud]         = { {INPUT_KEY, KEY_F5} },
    [input_quicksave]   = { {INPUT_KEY, KEY_F6} },
    [input_endgame]     = { {INPUT_KEY, KEY_F7} },
    [input_messages]    = { {INPUT_KEY, KEY_F8} },
    [input_quickload]   = { {INPUT_KEY, KEY_F9} },
    [input_quit]        = { {INPUT_KEY, KEY_F10} },
    [input_gamma]       = { {INPUT_KEY, KEY_F11} },
    [input_spy]         = { {INPUT_KEY, KEY_F12} },
    [input_zoomin]      = { {INPUT_KEY, '='} },
    [input_zoomout]     = { {INPUT_KEY, '-'} },
    [input_pause]       = { {INPUT_KEY, KEY_PAUSE} },

    [input_map]         = { {INPUT_KEY, KEY_TAB},
                            {INPUT_JOYB, GAMEPAD_NORTH} },
    [input_map_up]      = { {INPUT_KEY, KEY_UPARROW} },
    [input_map_down]    = { {INPUT_KEY, KEY_DOWNARROW} },
    [input_map_left]    = { {INPUT_KEY, KEY_LEFTARROW} },
    [input_map_right]   = { {INPUT_KEY, KEY_RIGHTARROW} },
    [input_map_follow]  = { {INPUT_KEY, 'f'} },
    [input_map_zoomin]  = { {INPUT_KEY, '='},
                            {INPUT_MOUSEB, MOUSE_BUTTON_WHEELUP} },
    [input_map_zoomout] = { {INPUT_KEY, '-'},
                            {INPUT_MOUSEB, MOUSE_BUTTON_WHEELDOWN} },
    [input_map_mark]    = { {INPUT_KEY, 'm'} },
    [input_map_clear]   = { {INPUT_KEY, 'c'} },
    [input_map_gobig]   = { {INPUT_KEY, '0'} },
    [input_map_grid]    = { {INPUT_KEY, 'g'} },
    [input_map_overlay] = { {INPUT_KEY, 'o'} },
    [input_map_rotate]  = { {INPUT_KEY, 'r'} },

    [input_chat]        = { {INPUT_KEY, 't'} },
    [input_chat_dest0]  = { {INPUT_KEY, 'g'} },
    [input_chat_dest1]  = { {INPUT_KEY, 'i'} },
    [input_chat_dest2]  = { {INPUT_KEY, 'b'} },
    [input_chat_dest3]  = { {INPUT_KEY, 'r'} },
};

void M_InputSetDefault(int id)
{
    InputSet(id, default_inputs[id], NUM_INPUTS);
}

void M_BindInputVariables(void)
{
#define BIND_INPUT(id, help) M_BindInput(#id, id, help)

    BIND_INPUT(input_forward, "Move forward");
    BIND_INPUT(input_backward, "Move backward");
    BIND_INPUT(input_turnright, "Turn right");
    BIND_INPUT(input_turnleft, "Turn left");
    BIND_INPUT(input_strafeleft, "Strafe left (sideways left)");
    BIND_INPUT(input_straferight, "Strafe right (sideways right)");
    BIND_INPUT(input_speed, "Run (move fast)");
    BIND_INPUT(input_strafe, "Strafe modifier (hold to strafe instead of turning)");
    BIND_INPUT(input_autorun, "Toggle always-run mode");
    BIND_INPUT(input_reverse, "Spin 180 degrees instantly");
    BIND_INPUT(input_gyro, "Gyro button");
    BIND_INPUT(input_use, "Open a door, use a switch");
    BIND_INPUT(input_fire, "Fire current weapon");
    BIND_INPUT(input_prevweapon, "Cycle to the previous weapon");
    BIND_INPUT(input_nextweapon, "Cycle to the next weapon");

    BIND_INPUT(input_novert, "Toggle vertical mouse movement");
    BIND_INPUT(input_freelook, "Toggle free look");

    BIND_INPUT(input_weapon1, "Switch to weapon 1 (Fist/Chainsaw)");
    BIND_INPUT(input_weapon2, "Switch to weapon 2 (Pistol)");
    BIND_INPUT(input_weapon3, "Switch to weapon 3 (Super Shotgun/Shotgun)");
    BIND_INPUT(input_weapon4, "Switch to weapon 4 (Chaingun)");
    BIND_INPUT(input_weapon5, "Switch to weapon 5 (Rocket Launcher)");
    BIND_INPUT(input_weapon6, "Switch to weapon 6 (Plasma Rifle)");
    BIND_INPUT(input_weapon7, "Switch to weapon 7 (BFG9000)");
    BIND_INPUT(input_weapon8, "Switch to weapon 8 (Chainsaw)");
    BIND_INPUT(input_weapon9, "Switch to weapon 9 (Super Shotgun)");
    BIND_INPUT(input_weapontoggle, "Switch between the two most-preferred weapons with ammo");
    BIND_INPUT(input_lastweapon, "Switch to last used weapon");

    BIND_INPUT(input_rewind, "Rewind");
    BIND_INPUT(input_menu_reloadlevel, "Restart current level/demo");
    BIND_INPUT(input_menu_nextlevel, "Go to next level");
    BIND_INPUT(input_menu_prevlevel, "Go to previous level");

    BIND_INPUT(input_hud_timestats, "Toggle display of level stats and time");

    BIND_INPUT(input_savegame, "Save current game");
    BIND_INPUT(input_loadgame, "Load saved games");

    BIND_INPUT(input_soundvolume, "Bring up sound control panel");
    BIND_INPUT(input_hud, "Cycle through HUD layouts");
    BIND_INPUT(input_quicksave, "Save to last used save slot");
    BIND_INPUT(input_endgame, "End the game");
    BIND_INPUT(input_messages, "Toggle messages");
    BIND_INPUT(input_quickload, "Load from quick-saved game");
    BIND_INPUT(input_quit, "Quit game");
    BIND_INPUT(input_gamma, "Adjust screen brightness (gamma correction)");
    BIND_INPUT(input_spy, "View from another player's vantage");
    BIND_INPUT(input_zoomin, "Enlarge display");
    BIND_INPUT(input_zoomout, "Reduce display");
    BIND_INPUT(input_screenshot, "Take a screenshot");
    BIND_INPUT(input_clean_screenshot, "Take a clean screenshot");
    BIND_INPUT(input_pause, "Pause the game");

    BIND_INPUT(input_demo_quit, "Finish recording demo");
    BIND_INPUT(input_demo_join, "Continue recording current demo");
    BIND_INPUT(input_demo_fforward, "Fast-forward demo");
    BIND_INPUT(input_speed_up, "Increase game speed");
    BIND_INPUT(input_speed_down, "Decrease game speed");
    BIND_INPUT(input_speed_default, "Reset game speed");

    BIND_INPUT(input_map, "Toggle automap display");
    BIND_INPUT(input_map_up, "Shift automap up");
    BIND_INPUT(input_map_down, "Shift automap down");
    BIND_INPUT(input_map_left, "Shift automap left");
    BIND_INPUT(input_map_right, "Shift automap right");
    BIND_INPUT(input_map_follow, "Toggle scrolling/moving with automap");
    BIND_INPUT(input_map_zoomin, "Enlarge automap");
    BIND_INPUT(input_map_zoomout, "Reduce automap");
    BIND_INPUT(input_map_mark, "Drop a marker on automap");
    BIND_INPUT(input_map_clear, "Clear last marker on automap");
    BIND_INPUT(input_map_gobig, "Toggle max zoom on automap");
    BIND_INPUT(input_map_grid, "Toggle grid display over automap");
    BIND_INPUT(input_map_overlay, "Toggle automap overlay mode");
    BIND_INPUT(input_map_rotate, "Toggle automap rotation");

    BIND_INPUT(input_chat, "Enter a chat message");
    BIND_INPUT(input_chat_dest0, "Chat with player 1");
    BIND_INPUT(input_chat_dest1, "Chat with player 2");
    BIND_INPUT(input_chat_dest2, "Chat with player 3");
    BIND_INPUT(input_chat_dest3, "Chat with player 4");

    BIND_INPUT(input_iddqd, "Toggle god mode");
    BIND_INPUT(input_idkfa, "Give ammo and keys");
    BIND_INPUT(input_idfa, "Give ammo");
    BIND_INPUT(input_idclip, "Toggle no-clipping mode");
    BIND_INPUT(input_idbeholdh, "Give health");
    BIND_INPUT(input_idbeholdm, "Give megaarmor");
    BIND_INPUT(input_idbeholdv, "Give invulnerability");
    BIND_INPUT(input_idbeholds, "Give berserk");
    BIND_INPUT(input_idbeholdi, "Give partial invisibility");
    BIND_INPUT(input_idbeholdr, "Give radiation suit");
    BIND_INPUT(input_idbeholdl, "Give light amplification visor");
    BIND_INPUT(input_iddt, "Reveal map");
    BIND_INPUT(input_notarget, "No-target mode");
    BIND_INPUT(input_freeze, "Freeze mode");
    BIND_INPUT(input_avj, "Fake Archvile Jump");
}
