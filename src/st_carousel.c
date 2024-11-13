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
#include "doomtype.h"
#include "g_game.h"
#include "m_array.h"
#include "m_misc.h"
#include "r_defs.h"
#include "st_sbardef.h"
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

static const weapontype_t weapon_order[] = {
    wp_fist,
    wp_chainsaw,
    wp_pistol,
    wp_shotgun,
    wp_supershotgun,
    wp_chaingun,
    wp_missile,
    wp_plasma,
    wp_bfg,
};

static weapontype_t curr;
static int curr_pos;

static int duration;

static weapontype_t *weapon_list;

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

    --duration;

    if (player->pendingweapon != wp_nochange)
    {
        curr = player->pendingweapon;
    }
    else
    {
        curr = player->readyweapon;
    }

    curr_pos = 0;
    array_clear(weapon_list);

    for (int i = 0; i < arrlen(weapon_order); ++i)
    {
        weapontype_t weapon = weapon_order[i];

        if (G_WeaponSelectable(weapon))
        {
            if (weapon == curr)
            {
                curr_pos = array_size(weapon_list);
            }
            array_push(weapon_list, weapon);
        }
    }
}

static void DrawItem(int x, int y, sbarelem_t *elem, weapontype_t weapon,
                     int state)
{
    char lump[9] = {0};
    M_snprintf(lump, sizeof(lump), "%s%d", names[weapon], state);

    patch_t *patch = V_CachePatchName(lump, PU_CACHE);

    byte *outr = colrngs[elem->cr];

    if (outr && elem->tranmap)
    {
        V_DrawPatchTRTL(x, y, patch, outr, elem->tranmap);
    }
    else if (elem->tranmap)
    {
        V_DrawPatchTL(x, y, patch, elem->tranmap);
    }
    else
    {
        V_DrawPatchTranslated(x, y, patch, outr);
    }
}

void ST_DrawCarousel(int x, int y, sbarelem_t *elem)
{
    if (duration == 0)
    {
        return;
    }

    DrawItem(SCREENWIDTH / 2, y, elem, curr, 1);

    for (int i = curr_pos + 1, k = 1; i < array_size(weapon_list) && k < 3;
         ++i, ++k)
    {
        DrawItem(SCREENWIDTH / 2 + k * 64, y, elem, weapon_list[i], 0);
    }

    for (int i = curr_pos - 1, k = 1; i >= 0 && k < 3; --i, ++k)
    {
        DrawItem(SCREENWIDTH / 2 - k * 64, y, elem, weapon_list[i], 0);
    }
}
