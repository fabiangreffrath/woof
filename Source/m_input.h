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
#include "d_event.h"

#define NUMKEYS 256

#define MAX_INPUT_KEYS 4

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
  input_autorun,
  input_reverse,
  input_use,
  input_fire,
  input_prevweapon,
  input_nextweapon,

  input_weapon1,
  input_weapon2,
  input_weapon3,
  input_weapon4,
  input_weapon5,
  input_weapon6,
  input_weapon7,
  input_weapon8,
  input_weapon9,
  input_weapontoggle,

  input_menu_up,
  input_menu_down,
  input_menu_left,
  input_menu_right,
  input_menu_backspace,
  input_menu_enter,
  input_menu_escape,
  input_menu_clear,
  input_menu_reloadlevel,
  input_menu_nextlevel,

  input_help,
  input_escape,
  input_savegame,
  input_loadgame,
  input_soundvolume,
  input_quicksave,
  input_endgame,
  input_messages,
  input_quickload,
  input_quit,
  input_hud,
  input_gamma,
  input_zoomin,
  input_zoomout,
  input_screenshot,
  input_setup,
  input_pause,
  input_spy,

  input_map,
  input_map_up,
  input_map_down,
  input_map_left,
  input_map_right,
  input_map_follow,
  input_map_zoomin,
  input_map_zoomout,
  input_map_mark,
  input_map_clear,
  input_map_gobig,
  input_map_grid,
  input_map_overlay,
  input_map_rotate,

  input_chat,
  input_chat_dest0,
  input_chat_dest1,
  input_chat_dest2,
  input_chat_dest3,
  input_chat_backspace,
  input_chat_enter,

  NUM_INPUT_ID
};

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

void    M_InputTrackEvent(event_t *ev);
boolean M_InputActivated(int input);
boolean M_InputDeactivated(int input);

boolean M_InputGameActive(int input);
boolean M_InputGameKeyActive(int input);
boolean M_InputGameMouseBActive(int input);
boolean M_InputGameJoyBActive(int input);
void    M_InputGameDeactivate(int input);

void    M_InputReset(int input);
void    M_InputSet(int input, input_default_t *pd);

void    M_InputFlush(void);

#endif
