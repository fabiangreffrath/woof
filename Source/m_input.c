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
#include "i_video.h" // MAX_MB, MAX_JSB

static input_t composite_inputs[NUM_INPUT_ID];

extern boolean gamekeydown[];
extern boolean *mousebuttons;
extern boolean *joybuttons;

static event_t *event;

typedef struct
{
  boolean on;
  int activated_at;
  int deactivated_at;
} input_state_t;

input_t* M_Input(int input)
{
    return &composite_inputs[input];
}

boolean M_InputMatchKey(int input, int value)
{
  int i;
  input_t *p = &composite_inputs[input];
  for (i = 0; i < p->num_keys; ++i)
  {
    if (p->keys[i] == value)
      return true;
  }
  return false;
}

void M_InputRemoveKey(int input, int value)
{
  int i;
  input_t *p = &composite_inputs[input];

  for (i = 0; i < p->num_keys; ++i)
  {
    if (p->keys[i] == value)
    {
      int left = p->num_keys - i - 1;
      if (left > 0)
      {
        memmove(p->keys + i, p->keys + i + 1,  left * sizeof(int));
      }
      p->num_keys--;
    }
  }
}

boolean M_InputAddKey(int input, int value)
{
  input_t *p = &composite_inputs[input];

  if (!value || M_InputMatchKey(input, value))
    return false;

  if (p->num_keys < MAX_INPUT_KEYS)
  {
    p->keys[p->num_keys] = value;
    p->num_keys++;
    return true;
  }
  return false;
}

boolean M_InputMatchMouseB(int input, int value)
{
  return value >= 0 && composite_inputs[input].mouseb == value;
}

void M_InputRemoveMouseB(int input, int value)
{
  composite_inputs[input].mouseb = -1;
}

void  M_InputAddMouseB(int input, int value)
{
  composite_inputs[input].mouseb = value;
}

boolean M_InputMatchJoyB(int input, int value)
{
  return value >= 0 && composite_inputs[input].joyb == value;
}

void M_InputRemoveJoyB(int input, int value)
{
  composite_inputs[input].joyb = -1;
}

void M_InputAddJoyB(int input, int value)
{
  composite_inputs[input].joyb = value;
}

void M_InputTrackEvent(event_t *ev)
{
  event = ev;
}

boolean M_InputActivated(int input)
{
  switch (event->type)
  {
    case ev_keydown:
      return M_InputMatchKey(input, event->data1);
      break;
    case ev_mouseb_down:
      return M_InputMatchMouseB(input, event->data1);
      break;
    case ev_joyb_down:
      return M_InputMatchJoyB(input, event->data1);
      break;
  }
  return false;
}

boolean M_InputDeactivated(int input)
{
  switch (event->type)
  {
    case ev_keyup:
      return M_InputMatchKey(input, event->data1);
      break;
    case ev_mouseb_up:
      return M_InputMatchMouseB(input, event->data1);
      break;
    case ev_joyb_up:
      return M_InputMatchJoyB(input, event->data1);
      break;
  }
  return false;
}

boolean M_InputGameKeyActive(int input)
{
  int i;
  input_t *p = &composite_inputs[input];

  for (i = 0; i < p->num_keys; ++i)
  {
    if (gamekeydown[p->keys[i]])
      return true;
  }
  return false;
}

boolean M_InputGameMouseBActive(int input)
{
  input_t *p = &composite_inputs[input];

  return p->mouseb >= 0 && mousebuttons[p->mouseb];
}

boolean M_InputGameJoyBActive(int input)
{
  input_t *p = &composite_inputs[input];

  return p->joyb >= 0 && joybuttons[p->joyb];
}

boolean M_InputGameActive(int input)
{
  return M_InputGameKeyActive(input) ||
         M_InputGameMouseBActive(input) ||
         M_InputGameJoyBActive(input);
}

void M_InputGameDeactivate(int input)
{
  int i;
  input_t *p = &composite_inputs[input];

  for (i = 0; i < p->num_keys; ++i)
  {
    if (gamekeydown[p->keys[i]])
      gamekeydown[p->keys[i]] = false;
  }

  if (p->mouseb >= 0 && mousebuttons[p->mouseb])
    mousebuttons[p->mouseb] = false;
  if (p->joyb >= 0 && joybuttons[p->joyb])
    joybuttons[p->joyb] = false;
}

void M_InputReset(int input)
{
  input_t *p = &composite_inputs[input];

  p->num_keys = 0;
  p->mouseb = -1;
  p->joyb = -1;
}

void M_InputSet(int input, input_default_t *pd)
{
  int i;
  input_t *p = &composite_inputs[input];

  p->num_keys = 0;
  for (i = 0; i < MAX_INPUT_KEYS; ++i)
  {
    if (pd->keys[i] > 0)
    {
      p->keys[i] = pd->keys[i];
      p->num_keys++;
    }
  }

  p->mouseb = pd->mouseb;
  p->joyb = pd->joyb;
}
