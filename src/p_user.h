//
//  Copyright (C) 1999 by
//  id Software, Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman
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
//      Player related stuff.
//      Bobbing POV/weapon, movement.
//      Pending weapon.
//
//-----------------------------------------------------------------------------

#ifndef __P_USER__
#define __P_USER__

#include "doomdef.h"
#include "m_fixed.h"
#include "tables.h"

struct player_s;

void P_PlayerThink(struct player_s *player);
void P_CalcHeight(struct player_s *player);
void P_DeathThink(struct player_s *player);
void P_MovePlayer(struct player_s *player);
void P_Thrust(struct player_s *player, angle_t angle, fixed_t move);

typedef enum
{
  DEATH_USE_ACTION_DEFAULT,
  DEATH_USE_ACTION_LAST_SAVE,
  DEATH_USE_ACTION_NOTHING
} death_use_action_t;

extern death_use_action_t death_use_action;

typedef enum
{
  DEATH_USE_STATE_INACTIVE,
  DEATH_USE_STATE_PENDING,
  DEATH_USE_STATE_ACTIVE
} death_use_state_t;

extern death_use_state_t death_use_state;

boolean P_EvaluateItemOwned(itemtype_t item, struct player_s *player);
int P_GetPowerDuration(powertype_t power);

extern boolean onground; // whether player is on ground or in air

#endif // __P_USER__

//----------------------------------------------------------------------------
//
// $Log: p_user.h,v $
// Revision 1.2  1998/05/10  23:38:38  killough
// Add more prototypes
//
// Revision 1.1  1998/05/03  23:19:24  killough
// Move from obsolete p_local.h
//
//----------------------------------------------------------------------------
