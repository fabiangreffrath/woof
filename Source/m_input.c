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
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 
//  02111-1307, USA.
//
// DESCRIPTION:
//      Custom input

#include <string.h>
#include "m_input.h"
#include "doomdef.h"
#include "d_io.h"
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
    return false;

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

void M_InputAddMouseB(int ident, int value)
{
  InputAdd(ident, input_type_mouseb, value);
}

boolean M_InputMatchJoyB(int ident, int value)
{
  return value >= 0 && InputMatch(ident, input_type_joyb, value);
}

void M_InputRemoveJoyB(int ident, int value)
{
  InputRemove(ident, input_type_joyb, value);
}

void M_InputAddJoyB(int ident, int value)
{
  InputAdd(ident, input_type_joyb, value);
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

struct
{
  int key;
  char* name;
} key_names[] = {
  { 0,              "NONE" },
  { KEYD_TAB,       "TAB"  },
  { KEYD_ENTER,     "ENTR" },
  { KEYD_ESCAPE,    "ESC"  },
  { KEYD_SPACEBAR,  "SPAC" },
  { KEYD_BACKSPACE, "BACK" },
  { KEYD_RCTRL,     "CTRL" },
  { KEYD_LEFTARROW, "LARR" },
  { KEYD_UPARROW,   "UARR" },
  { KEYD_RIGHTARROW,"RARR" },
  { KEYD_DOWNARROW, "DARR" },
  { KEYD_RSHIFT,    "SHFT" },
  { KEYD_RALT,      "ALT"  },
  { KEYD_CAPSLOCK,  "CAPS" },
  { KEYD_F1,        "F1"   },
  { KEYD_F2,        "F2"   },
  { KEYD_F3,        "F3"   },
  { KEYD_F4,        "F4"   },
  { KEYD_F5,        "F5"   },
  { KEYD_F6,        "F6"   },
  { KEYD_F7,        "F7"   },
  { KEYD_F8,        "F8"   },
  { KEYD_F9,        "F9"   },
  { KEYD_F10,       "F10"  },
  { KEYD_F11,       "F11"  },
  { KEYD_F12,       "F12"  },
  { KEYD_SCROLLLOCK,"SCRL" },
  { KEYD_HOME,      "HOME" },
  { KEYD_PAGEUP,    "PGUP" },
  { KEYD_END,       "END"  },
  { KEYD_PAGEDOWN,  "PGDN" },
  { KEYD_INSERT,    "INST" },
  { KEYD_PAUSE,     "PAUS" },
  { KEYD_DEL,       "DEL"  }
};

char* M_GetNameFromKey(int key)
{
  int i;
  for (i = 0; i < arrlen(key_names); ++i)
  {
    if (key_names[i].key == key)
      return key_names[i].name;
  }
  return NULL;
}

int M_GetKeyFromName(char* name)
{
  int i;
  for (i = 0; i < arrlen(key_names); ++i)
  {
    if (strcasecmp(name, key_names[i].name) == 0)
      return key_names[i].key;
  }
  return 0;
}
