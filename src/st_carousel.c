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

#include <string.h>

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

static weapontype_t curr, next1, next2, prev1, prev2;
static int duration;
static player_t local_player;

static weapontype_t NextWeapon(weapontype_t type, int direction)
{
    local_player.pendingweapon = type;
    local_player.readyweapon = type;
    return G_NextWeapon(&local_player, direction);
}

void ST_UpdateCarousel(player_t *player)
{
    if (player->pendingweapon != wp_nochange
        && G_Carousel())
    {
        duration = TICRATE * 2;
    }

    if (duration == 0)
    {
        return;
    }

    if (player->pendingweapon != wp_nochange)
    {
        curr = player->pendingweapon;
    }
    else
    {
        curr = player->readyweapon;
    }

    memcpy(local_player.weaponowned, player->weaponowned, sizeof(player->weaponowned));
    memcpy(local_player.powers, player->powers, sizeof(player->powers));

    next1 = NextWeapon(curr, 1);
    next2 = NextWeapon(next1, 1);

    prev1 = NextWeapon(curr, -1);
    prev2 = NextWeapon(prev1, -1);

    if (next2 == curr)
    {
        next2 = wp_nochange;
    }
    if (prev2 == curr)
    {
        prev2 = wp_nochange;
    }

    --duration;
}

static void DrawItem(int x, int y, weapontype_t weapon, int state)
{
    if (weapon == wp_nochange)
    {
        return;
    }

    char lump[9] = {0};
    M_snprintf(lump, sizeof(lump), "%s%d", names[weapon], state);
    V_DrawPatch(x, y, V_CachePatchName(lump, PU_CACHE));
}

void ST_DrawCarousel(int x, int y)
{
    if (duration == 0)
    {
        return;
    }

    DrawItem(SCREENWIDTH / 2, y, curr, 1);

    DrawItem(SCREENWIDTH / 2 + 64, y, next1, 0);
    DrawItem(SCREENWIDTH / 2 + 2 * 64, y, next2, 0);

    DrawItem(SCREENWIDTH / 2 - 64, y, prev1, 0);
    DrawItem(SCREENWIDTH / 2 - 2 * 64, y, prev2, 0);
}
