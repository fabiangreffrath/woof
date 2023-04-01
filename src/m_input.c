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
#include "i_video.h" // MAX_MB, MAX_JSB

static input_t composite_inputs[NUM_INPUT_ID];

extern boolean gamekeydown[];
extern boolean *mousebuttons;
extern boolean *joybuttons;

static event_t *event;

static boolean InputMatch(int ident, input_type_t type, int value)
{
  int i;
  input_t *p = &composite_inputs[ident];
  for (i = 0; i < p->num_inputs; ++i)
  {
    if (p->inputs[i].type == type && p->inputs[i].value == value)
      return true;
  }
  return false;
}

static void InputRemove(int ident, input_type_t type, int value)
{
  int i;
  input_t *p = &composite_inputs[ident];

  for (i = 0; i < p->num_inputs; ++i)
  {
    if (p->inputs[i].type == type && p->inputs[i].value == value)
    {
      int left = p->num_inputs - i - 1;
      if (left > 0)
      {
        memmove(p->inputs + i, p->inputs + i + 1, left * sizeof(input_value_t));
      }
      p->num_inputs--;
    }
  }
}

static boolean InputAdd(int ident, input_type_t type, int value)
{
  input_t *p = &composite_inputs[ident];

  if (InputMatch(ident, type, value))
    return true;

  if (p->num_inputs < NUM_INPUTS)
  {
    input_value_t *v = &p->inputs[p->num_inputs];

    v->type = type;
    v->value = value;
    p->num_inputs++;
    return true;
  }
  return false;
}

input_t* M_Input(int ident)
{
  return &composite_inputs[ident];
}

boolean M_InputMatchKey(int ident, int value)
{
  return InputMatch(ident, input_type_key, value);
}

void M_InputRemoveKey(int ident, int value)
{
  InputRemove(ident, input_type_key, value);
}

boolean M_InputAddKey(int ident, int value)
{
  return InputAdd(ident, input_type_key, value);
}

boolean M_InputMatchMouseB(int ident, int value)
{
  return value >= 0 && InputMatch(ident, input_type_mouseb, value);
}

void M_InputRemoveMouseB(int ident, int value)
{
  InputRemove(ident, input_type_mouseb, value);
}

boolean M_InputAddMouseB(int ident, int value)
{
  return InputAdd(ident, input_type_mouseb, value);
}

boolean M_InputMatchJoyB(int ident, int value)
{
  return value >= 0 && InputMatch(ident, input_type_joyb, value);
}

void M_InputRemoveJoyB(int ident, int value)
{
  InputRemove(ident, input_type_joyb, value);
}

boolean M_InputAddJoyB(int ident, int value)
{
  return InputAdd(ident, input_type_joyb, value);
}

void M_InputTrackEvent(event_t *ev)
{
  event = ev;
}

boolean M_InputActivated(int ident)
{
  switch (event->type)
  {
    case ev_keydown:
      return M_InputMatchKey(ident, event->data1);
      break;
    case ev_mouseb_down:
      return M_InputMatchMouseB(ident, event->data1);
      break;
    case ev_joyb_down:
      return M_InputMatchJoyB(ident, event->data1);
      break;
    default:
      break;
  }
  return false;
}

boolean M_InputDeactivated(int ident)
{
  switch (event->type)
  {
    case ev_keyup:
      return M_InputMatchKey(ident, event->data1);
      break;
    case ev_mouseb_up:
      return M_InputMatchMouseB(ident, event->data1);
      break;
    case ev_joyb_up:
      return M_InputMatchJoyB(ident, event->data1);
      break;
    default:
      break;
  }
  return false;
}

static boolean InputActive(boolean *buttons, int ident, input_type_t type)
{
  int i;
  input_t *p = &composite_inputs[ident];

  for (i = 0; i < p->num_inputs; ++i)
  {
    input_value_t *v = &p->inputs[i];

    if (v->type == type && buttons[v->value])
      return true;
  }
  return false;
}

boolean M_InputGameKeyActive(int ident)
{
  return InputActive(gamekeydown, ident, input_type_key);
}

boolean M_InputGameMouseBActive(int ident)
{
  return InputActive(mousebuttons, ident, input_type_mouseb);
}

boolean M_InputGameJoyBActive(int ident)
{
  return InputActive(joybuttons, ident, input_type_joyb);
}

boolean M_InputGameActive(int ident)
{
  return M_InputGameKeyActive(ident) ||
         M_InputGameMouseBActive(ident) ||
         M_InputGameJoyBActive(ident);
}

void M_InputGameDeactivate(int ident)
{
  int i;
  input_t *p = &composite_inputs[ident];

  for (i = 0; i < p->num_inputs; ++i)
  {
    input_value_t *v = &p->inputs[i];

    switch (v->type)
    {
      case input_type_key:
        if (gamekeydown[v->value])
          gamekeydown[v->value] = false;
        break;
      case input_type_mouseb:
        if (mousebuttons[v->value])
          mousebuttons[v->value] = false;
        break;
      case input_type_joyb:
        if (joybuttons[v->value])
          joybuttons[v->value] = false;
        break;
      default:
        break;
    }
  }
}

void M_InputReset(int ident)
{
  input_t *p = &composite_inputs[ident];

  p->num_inputs = 0;
}

void M_InputAdd(int ident, input_value_t value)
{
  InputAdd(ident, value.type, value.value);
}

void M_InputSet(int ident, input_value_t *inputs)
{
  int i;
  input_t *p = &composite_inputs[ident];

  p->num_inputs = 0;
  for (i = 0; i < NUM_INPUTS; ++i)
  {
    if (inputs[i].type > input_type_null)
    {
      p->inputs[i].type = inputs[i].type;
      p->inputs[i].value = inputs[i].value;
      p->num_inputs++;
    }
  }
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
  { KEY_PRTSCR,     "prtscr" }
};

static const struct
{
  int joyb;
  const char* name;
} joyb_names[] = {
  { CONTROLLER_A,              "pada" },
  { CONTROLLER_B,              "padb" },
  { CONTROLLER_X,              "padx" },
  { CONTROLLER_Y,              "pady" },
  { CONTROLLER_BACK,           "back" },
  { CONTROLLER_GUIDE,          "guide" },
  { CONTROLLER_START,          "start" },
  { CONTROLLER_LEFT_STICK,     "ls" },
  { CONTROLLER_RIGHT_STICK,    "rs" },
  { CONTROLLER_LEFT_SHOULDER,  "lb" },
  { CONTROLLER_RIGHT_SHOULDER, "rb" }, 
  { CONTROLLER_DPAD_UP,        "padup" },
  { CONTROLLER_DPAD_DOWN,      "paddown" },
  { CONTROLLER_DPAD_LEFT,      "padleft" },
  { CONTROLLER_DPAD_RIGHT,     "padright" },
  { CONTROLLER_MISC1,          "misc1" },
  { CONTROLLER_PADDLE1,        "paddle1" },
  { CONTROLLER_PADDLE2,        "paddle2" },
  { CONTROLLER_PADDLE3,        "paddle3" },
  { CONTROLLER_PADDLE4,        "paddle4" },
  { CONTROLLER_TOUCHPAD,       "touch" },
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
  int i;
  for (i = 0; i < arrlen(key_names); ++i)
  {
    if (key_names[i].key == key)
      return key_names[i].name;
  }
  return NULL;
}

int M_GetKeyForName(const char* name)
{
  int i;
  for (i = 0; i < arrlen(key_names); ++i)
  {
    if (strcasecmp(name, key_names[i].name) == 0)
      return key_names[i].key;
  }
  return 0;
}

const char* M_GetNameForJoyB(int joyb)
{
  int i;
  for (i = 0; i < arrlen(joyb_names); ++i)
  {
    if (joyb_names[i].joyb == joyb)
      return joyb_names[i].name;
  }
  return NULL;
}

int M_GetJoyBForName(const char* name)
{
  int i;
  for (i = 0; i < arrlen(joyb_names); ++i)
  {
    if (strcasecmp(name, joyb_names[i].name) == 0)
      return joyb_names[i].joyb;
  }
  return -1;
}

const char* M_GetNameForMouseB(int mouseb)
{
  int i;
  for (i = 0; i < arrlen(mouseb_names); ++i)
  {
    if (mouseb_names[i].mouseb == mouseb)
      return mouseb_names[i].name;
  }
  return NULL;
}

int M_GetMouseBForName(const char* name)
{
  int i;
  for (i = 0; i < arrlen(mouseb_names); ++i)
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
