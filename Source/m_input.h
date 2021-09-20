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

#ifndef __M_INPUT__
#define __M_INPUT__

#include "doomtype.h"

enum
{
  input_null,
  input_forward,
  input_backward,
  input_turnleft,
  input_turnright,
  input_speed,
  input_strafeleft,
  input_straferight,
  input_strafe,
  input_reverse,
  input_use,
  input_fire,
  input_prevweapon,
  input_nextweapon,

  NUM_INPUT_ID
};

#define MAX_INPUT_KEYS   4

typedef struct
{
  int keys[MAX_INPUT_KEYS];
  int num_keys;
  int mouseb;
  int joyb;
} input_t;

typedef struct
{
  int keys[MAX_INPUT_KEYS];
  int mouseb;
  int joyb;
} input_default_t;

input_t* M_Input(int input);

boolean M_InputMatchKey(int input, int value);
void    M_InputRemoveKey(int input, int value);
boolean M_InputAddKey(int input, int value);

boolean M_InputMatchMouseB(int input, int value);
void    M_InputRemoveMouseB(int input, int value);
void    M_InputAddMouseB(int input, int value);

boolean M_InputMatchJoyB(int input, int value);
void    M_InputRemoveJoyB(int input, int value);
void    M_InputAddJoyB(int input, int value);

boolean M_InputActive(int input);
boolean M_InputMouseBActive(int input);
boolean M_InputJoyBActive(int input);

void    M_InputDeactivate(int input);

void    M_InputReset(int input);
void    M_InputSet(int input, input_default_t *pd);

#endif
