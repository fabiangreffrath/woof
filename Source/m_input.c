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

typedef struct
{
  boolean on;
  int activated_at;
  int deactivated_at;
} input_state_t;

static int event_counter;

static input_state_t keys_state[NUMKEYS];
static input_state_t mouseb_state[MAX_MB];
static input_state_t joyb_state[MAX_JSB];

static void SetButtons(input_state_t *buttons, int max, int data)
{
  int i;

  for (i = 0; i < max; ++i)
  {
    boolean button_on = (data & (1 << i)) != 0;

    if (!buttons[i].on && button_on)
      buttons[i].activated_at = event_counter;

   if (buttons[i].on && !button_on)
      buttons[i].deactivated_at = event_counter;

    buttons[i].on = button_on;
  }
}

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

  if (M_InputMatchKey(input, value))
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
  event_counter++;

  if (ev->type == ev_keydown)
  {
    keys_state[ev->data1].on = true;
    keys_state[ev->data1].activated_at = event_counter;
  }
  else if (ev->type == ev_keyup)
  {
    keys_state[ev->data1].on = false;
    keys_state[ev->data1].deactivated_at = event_counter;
  }
  else if (ev->type == ev_joystick)
    SetButtons(joyb_state, MAX_JSB, ev->data1);
  else if (ev->type == ev_mouse)
    SetButtons(mouseb_state, MAX_MB, ev->data1);
}

boolean M_InputActivated(int input)
{
  int i;
  input_t *p = &composite_inputs[input];

  for (i = 0; i < p->num_keys; ++i)
  {
    if (keys_state[p->keys[i]].activated_at == event_counter)
      return true;
  }

  return
    (p->mouseb >= 0 && mouseb_state[p->mouseb].activated_at == event_counter) ||
    (p->joyb >= 0 && joyb_state[p->joyb].activated_at == event_counter);
}

boolean M_InputDeactivated(int input)
{
  int i;
  boolean deactivated = false;
  input_t *p = &composite_inputs[input];

  for (i = 0; i < p->num_keys; ++i)
  {
    if (keys_state[p->keys[i]].on)
      return false;
    else if (keys_state[p->keys[i]].deactivated_at == event_counter)
      deactivated = true;
  }

  return
      deactivated ||
      (p->mouseb >= 0 && !mouseb_state[p->mouseb].on &&
       mouseb_state[p->mouseb].deactivated_at == event_counter) ||
      (p->joyb >= 0 && !joyb_state[p->joyb].on &&
       joyb_state[p->joyb].deactivated_at == event_counter);
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

void M_InputFlush(void)
{
  memset(keys_state, 0, sizeof(keys_state));
  memset(mouseb_state, 0, sizeof(mouseb_state));
  memset(joyb_state, 0, sizeof(joyb_state));
  event_counter = 0;
}
