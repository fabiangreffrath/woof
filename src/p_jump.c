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

#include <math.h>

#include "d_player.h"
#include "d_ticcmd.h"
#include "doomstat.h"
#include "doomtype.h"
#include "m_fixed.h"
#include "p_jump.h"
#include "p_mobj.h"
#include "tables.h"

#define JUMP_MOMZ (8 * FRACUNIT)

// Air control tuning, in map units per tic (a full run is about 15).
#define AIR_CAP 2.0
#define AIR_ACCEL 0.3
#define CMD_TO_SPEED 0.3
#define MAX_WISH_SPEED 15.0

static boolean jumped; // true from jump takeoff until the next landing

// Applies Quake-style air acceleration, adding thrust along the wish direction only below a small speed cap.
static void AirControl(const player_t *player)
{
    mobj_t *const mo = player->mo;
    const ticcmd_t *const cmd = &player->cmd;
    const angle_t fwd_angle = mo->angle >> ANGLETOFINESHIFT;
    const angle_t side_angle = (mo->angle - ANG90) >> ANGLETOFINESHIFT;
    const double wishx = (double)cmd->forwardmove * finecosine[fwd_angle]
                         + (double)cmd->sidemove * finecosine[side_angle];
    const double wishy = (double)cmd->forwardmove * finesine[fwd_angle]
                         + (double)cmd->sidemove * finesine[side_angle];
    const double norm = sqrt(wishx * wishx + wishy * wishy);

    if (norm == 0.0)
    {
        return;
    }

    const double dirx = wishx / norm;
    const double diry = wishy / norm;
    const double wishspeed = fmin(norm / FRACUNIT * CMD_TO_SPEED, MAX_WISH_SPEED);
    const double current = (mo->momx * dirx + mo->momy * diry) / FRACUNIT;
    const double addspeed = fmin(wishspeed, AIR_CAP) - current;

    if (addspeed <= 0.0)
    {
        return;
    }

    const double accelspeed = fmin(AIR_ACCEL * wishspeed, addspeed);

    mo->momx += (fixed_t)(accelspeed * dirx * FRACUNIT);
    mo->momy += (fixed_t)(accelspeed * diry * FRACUNIT);
}

// Starts a jump from the ground or, after a jump, applies air control.
void P_PlayerJump(player_t *player, const boolean onground)
{
    if (onground)
    {
        jumped = false;
    }

    if (player->cmd.jump && onground)
    {
        player->mo->momz += JUMP_MOMZ;
        jumped = true;
    }
    else if (!onground && CRITICAL(jumped))
    {
        AirControl(player);
    }
}
