//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// DESCRIPTION:
//       Key definitions
//

#ifndef __DOOMKEYS__
#define __DOOMKEYS__

// DOOM keyboard definition.
// This is the stuff configured by Setup.Exe.
// Most key data are simple ascii (uppercased).

#define KEYD_RIGHTARROW 0xae
#define KEYD_LEFTARROW  0xac
#define KEYD_UPARROW    0xad
#define KEYD_DOWNARROW  0xaf
#define KEYD_ESCAPE     27
#define KEYD_ENTER      13
#define KEYD_TAB        9
#define KEYD_F1         (0x80+0x3b)
#define KEYD_F2         (0x80+0x3c)
#define KEYD_F3         (0x80+0x3d)
#define KEYD_F4         (0x80+0x3e)
#define KEYD_F5         (0x80+0x3f)
#define KEYD_F6         (0x80+0x40)
#define KEYD_F7         (0x80+0x41)
#define KEYD_F8         (0x80+0x42)
#define KEYD_F9         (0x80+0x43)
#define KEYD_F10        (0x80+0x44)
#define KEYD_F11        (0x80+0x57)
#define KEYD_F12        (0x80+0x58)
#define KEYD_BACKSPACE  0x7f
#define KEYD_PAUSE      0xff
#define KEYD_EQUALS     0x3d
#define KEYD_MINUS      0x2d
#define KEYD_RSHIFT     (0x80+0x36)
#define KEYD_RCTRL      (0x80+0x1d)
#define KEYD_RALT       (0x80+0x38)
#define KEYD_LALT       KEYD_RALT

// [FG] updated from Chocolate Doom 3.0 (src/doomkeys.h)

// new keys:

#define KEYD_CAPSLOCK   (0x80+0x3a)
#define KEYD_NUMLOCK    (0x80+0x45)
#define KEYD_SCRLCK     (0x80+0x46)
#define KEYD_PRTSCR     (0x80+0x59)

#define KEYD_HOME       (0x80+0x47)
#define KEYD_END        (0x80+0x4f)
#define KEYD_PGUP       (0x80+0x49)
#define KEYD_PGDN       (0x80+0x51)
#define KEYD_INS        (0x80+0x52)
#define KEYD_DEL        (0x80+0x53)

#define KEYP_0          KEYD_INS
#define KEYP_1          KEYD_END
#define KEYP_2          KEYD_DOWNARROW
#define KEYP_3          KEYD_PGDN
#define KEYP_4          KEYD_LEFTARROW
#define KEYP_5          (0x80+0x4c)
#define KEYP_6          KEYD_RIGHTARROW
#define KEYP_7          KEYD_HOME
#define KEYP_8          KEYD_UPARROW
#define KEYP_9          KEYD_PGUP

#define KEYP_DIVIDE     '/'
#define KEYP_PLUS       '+'
#define KEYP_MINUS      '-'
#define KEYP_MULTIPLY   '*'
#define KEYP_PERIOD     0
#define KEYP_EQUALS     KEYD_EQUALS
#define KEYP_ENTER      KEYD_ENTER

#define KEYD_SPACEBAR   ' '
#define KEYD_SCROLLLOCK KEYD_SCRLCK
#define KEYD_PAGEUP     KEYD_PGUP
#define KEYD_PAGEDOWN   KEYD_PGDN
#define KEYD_INSERT     KEYD_INS

#define SCANCODE_TO_KEYS_ARRAY {                                             \
    0,   0,   0,   0,   'a',                                   /* 0-9 */     \
    'b', 'c', 'd', 'e', 'f',                                                 \
    'g', 'h', 'i', 'j', 'k',                                   /* 10-19 */   \
    'l', 'm', 'n', 'o', 'p',                                                 \
    'q', 'r', 's', 't', 'u',                                   /* 20-29 */   \
    'v', 'w', 'x', 'y', 'z',                                                 \
    '1', '2', '3', '4', '5',                                   /* 30-39 */   \
    '6', '7', '8', '9', '0',                                                 \
    KEYD_ENTER, KEYD_ESCAPE, KEYD_BACKSPACE, KEYD_TAB, ' ',    /* 40-49 */   \
    KEYD_MINUS, KEYD_EQUALS, '[', ']', '\\',                                 \
    0,   ';', '\'', '`', ',',                                  /* 50-59 */   \
    '.', '/', KEYD_CAPSLOCK, KEYD_F1, KEYD_F2,                               \
    KEYD_F3, KEYD_F4, KEYD_F5, KEYD_F6, KEYD_F7,               /* 60-69 */   \
    KEYD_F8, KEYD_F9, KEYD_F10, KEYD_F11, KEYD_F12,                          \
    KEYD_PRTSCR, KEYD_SCRLCK, KEYD_PAUSE, KEYD_INS, KEYD_HOME, /* 70-79 */   \
    KEYD_PGUP, KEYD_DEL, KEYD_END, KEYD_PGDN, KEYD_RIGHTARROW,               \
    KEYD_LEFTARROW, KEYD_DOWNARROW, KEYD_UPARROW,              /* 80-89 */   \
    KEYD_NUMLOCK, KEYP_DIVIDE,                                               \
    KEYP_MULTIPLY, KEYP_MINUS, KEYP_PLUS, KEYP_ENTER, KEYP_1,                \
    KEYP_2, KEYP_3, KEYP_4, KEYP_5, KEYP_6,                    /* 90-99 */   \
    KEYP_7, KEYP_8, KEYP_9, KEYP_0, KEYP_PERIOD,                             \
    0, 0, 0, KEYP_EQUALS,                                      /* 100-103 */ \
}

enum
{
    CONTROLLER_A,
    CONTROLLER_B,
    CONTROLLER_X,
    CONTROLLER_Y,
    CONTROLLER_BACK,
    CONTROLLER_GUIDE,
    CONTROLLER_START,
    CONTROLLER_LEFT_STICK,
    CONTROLLER_RIGHT_STICK,
    CONTROLLER_LEFT_SHOULDER,
    CONTROLLER_RIGHT_SHOULDER,
    CONTROLLER_DPAD_UP,
    CONTROLLER_DPAD_DOWN,
    CONTROLLER_DPAD_LEFT,
    CONTROLLER_DPAD_RIGHT,
    CONTROLLER_MISC1,
    CONTROLLER_PADDLE1,
    CONTROLLER_PADDLE2,
    CONTROLLER_PADDLE3,
    CONTROLLER_PADDLE4,
    CONTROLLER_TOUCHPAD,
    CONTROLLER_LEFT_TRIGGER,
    CONTROLLER_RIGHT_TRIGGER,
    CONTROLLER_LEFT_STICK_UP,
    CONTROLLER_LEFT_STICK_DOWN,
    CONTROLLER_LEFT_STICK_LEFT,
    CONTROLLER_LEFT_STICK_RIGHT,
    CONTROLLER_RIGHT_STICK_UP,
    CONTROLLER_RIGHT_STICK_DOWN,
    CONTROLLER_RIGHT_STICK_LEFT,
    CONTROLLER_RIGHT_STICK_RIGHT,

    NUM_CONTROLLER_BUTTONS
};

enum
{
    MOUSE_BUTTON_LEFT,
    MOUSE_BUTTON_RIGHT,
    MOUSE_BUTTON_MIDDLE,
    MOUSE_BUTTON_X1,
    MOUSE_BUTTON_X2,
    MOUSE_BUTTON_WHEELUP,
    MOUSE_BUTTON_WHEELDOWN,

    NUM_MOUSE_BUTTONS
};

enum
{
    AXIS_LEFTX,
    AXIS_LEFTY,
    AXIS_RIGHTX,
    AXIS_RIGHTY,

    NUM_AXES
};

#endif          // __DOOMKEYS__
