//
// Copyright(C) 2024 Roman Fomin
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

#include "d_player.h"
#include "doomdef.h"
#include "g_game.h"
#include "m_misc.h"
#include "v_fmt.h"
#include "v_video.h"

static const char *names[] = {
    [wp_fist] = "SMFIST",
    [wp_pistol] = "SMPISG",
    [wp_shotgun] = "SMSHOT",
    [wp_chaingun] = "SMMGUN",
    [wp_missile] = "SMLAUN",
    [wp_plasma] = "SMPLAS",
    [wp_bfg] = "SMBFGG",
    [wp_chainsaw] = "SMCSAW",
    [wp_supershotgun] = "SMSGN2",
};

static weapontype_t curr, next, prev;
static int duration;

void ST_UpdateNextWeap(player_t *player)
{
    if (player->pendingweapon != wp_nochange)
    {
        curr = player->pendingweapon;
        duration = 2 * TICRATE / 5;
    }
    else
    {
        curr = player->readyweapon;
    }

    if (duration == 0)
    {
        return;
    }

    next = G_GetNextWeapon(1);
    prev = G_GetNextWeapon(-1);

    --duration;
}

void ST_DrawNextWeap(int x, int y)
{
    if (duration == 0)
    {
        return;
    }

    char lump[9] = {0};

    M_snprintf(lump, sizeof(lump), "%s%d", names[curr], 1);
    V_DrawPatch(SCREENWIDTH / 2, y, V_CachePatchName(lump, PU_CACHE));

    M_snprintf(lump, sizeof(lump), "%s%d", names[next], 0);
    V_DrawPatch(SCREENWIDTH / 2 + 64, y, V_CachePatchName(lump, PU_CACHE));

    M_snprintf(lump, sizeof(lump), "%s%d", names[prev], 0);
    V_DrawPatch(SCREENWIDTH / 2 - 64, y, V_CachePatchName(lump, PU_CACHE));
}
