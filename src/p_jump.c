//
//  Copyright (C) 2026 Fabian Greffrath
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

#include "p_jump.h"
#include "d_player.h"
#include "d_ticcmd.h"
#include "doomdef.h"
#include "doomstat.h"
#include "doomtype.h"
#include "m_fixed.h"
#include "p_mobj.h"
#include "p_user.h"
#include "tables.h"

#define JUMP_MOMZ  (8 * FRACUNIT)
#define JUMP_DELAY 7  // tics after landing before the next jump
#define JUMP_RESET 18 // tics in a jump after which touching the floor ends it
#define AIR_THRUST (ORIG_FRICTION_FACTOR / 256) // 1/256 of ground thrust

// Starts a jump, counts down the delay, and applies the small air thrust during
// a jump.
void P_PlayerJump(player_t *player, const boolean onground)
{
    const ticcmd_t *const cmd = &player->cmd;
    mobj_t *const mo = player->mo;

    if (player->jumptics != 0)
    {
        player->jumptics--;

        if (onground && player->jumptics < -JUMP_RESET)
        {
            player->jumptics = 0;
        }
    }

    if (cmd->jump && onground && player->jumptics == 0)
    {
        mo->momz += JUMP_MOMZ;
        player->jumptics = -1;
        // S_StartSound(mo, sfx_pljump);
    }
    else if (!onground && CRITICAL(player->jumptics < 0))
    {
        if (cmd->forwardmove)
        {
            P_Thrust(player, mo->angle, cmd->forwardmove * AIR_THRUST);
        }

        if (cmd->sidemove)
        {
            P_Thrust(player, mo->angle - ANG90, cmd->sidemove * AIR_THRUST);
        }
    }
}

// Delays the next jump after landing from a jump or after a hard fall.
void P_PlayerLanded(player_t *player, const boolean hard)
{
    if (player->jumptics < 0 || hard)
    {
        player->jumptics = JUMP_DELAY;
    }
}
