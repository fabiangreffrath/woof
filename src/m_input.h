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

#ifndef __M_INPUT__
#define __M_INPUT__

#include "doomtype.h"
#include "i_gamepad.h"

struct event_s;

#define NUM_INPUTS 4

enum
{
    input_null,
    input_forward,
    input_backward,
    input_turnleft,
    input_turnright,
    input_strafeleft,
    input_straferight,
    input_speed,
    input_strafe,
    input_autorun,
    input_reverse,
    input_gyro,
    input_use,
    input_fire,
    input_prevweapon,
    input_nextweapon,

    input_novert,
    input_freelook,

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
    input_lastweapon,

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
    input_menu_prevlevel,

    input_hud_timestats,

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
    input_clean_screenshot,
    input_pause,
    input_spy,

    input_demo_quit,
    input_demo_fforward,
    input_demo_join,
    input_speed_up,
    input_speed_down,
    input_speed_default,
    input_rewind,

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
    input_netgame_stats,
    input_msgreview,

    input_iddqd,
    input_idkfa,
    input_idfa,
    input_idclip,
    input_idbeholdh,
    input_idbeholdm,
    input_idbeholdv,
    input_idbeholds,
    input_idbeholdi,
    input_idbeholdr,
    input_idbeholdl,
    input_iddt,
    input_notarget,
    input_freeze,
    input_avj,

    NUM_INPUT_ID
};

typedef enum
{
    INPUT_NULL,
    INPUT_KEY,
    INPUT_MOUSEB,
    INPUT_JOYB,
} input_type_t;

typedef struct
{
    input_type_t type;
    int value;
} input_t;

const input_t *M_Input(int id);

boolean M_InputAddKey(int id, int value);
boolean M_InputAddMouseB(int id, int value);
boolean M_InputAddJoyB(int id, int value);

void M_InputTrackEvent(struct event_s *ev);
boolean M_InputActivated(int id);
boolean M_InputDeactivated(int id);
boolean M_InputAddActivated(int id);
void M_InputRemoveActivated(int id);

boolean M_InputGameActive(int id);
void M_InputGameDeactivate(int id);

void M_InputReset(int id);
void M_InputSetDefault(int id);

const char *M_GetNameForKey(int key);
int M_GetKeyForName(const char *name);

void M_UpdatePlatform(joy_platform_t platform);
const char *M_GetPlatformName(int joyb);
const char *M_GetNameForJoyB(int joyb);
int M_GetJoyBForName(const char *name);

const char *M_GetNameForMouseB(int joyb);
int M_GetMouseBForName(const char *name);

boolean M_IsMouseWheel(int mouseb);

void M_InputPredefined(void);

void M_BindInputVariables(void);

#endif
