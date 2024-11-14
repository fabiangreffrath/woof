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

typedef struct
{
    weapontype_t weapon;
    int state;
    boolean darkened;
} weapon_icon_t;

static weapon_icon_t *weapon_icons;

static weapontype_t curr;
static int curr_pos;

static int duration;

void ST_ResetCarousel(void)
{
    duration = 0;
}

void ST_UpdateCarousel(player_t *player)
{
    if (G_Carousel())
    {
        duration = TICRATE / 2;
    }

    if (duration == 0)
    {
        return;
    }

    if (menuactive || paused || player->playerstate == PST_DEAD)
    {
        ST_ResetCarousel();
        return;
    }

    if (player->switching == weapswitch_none)
    {
        --duration;
    }

    if (player->pendingweapon == wp_nochange)
    {
        curr = player->readyweapon;
    }
    else
    {
        curr = player->pendingweapon;
    }

    curr_pos = 0;
    array_clear(weapon_icons);

    for (int i = 0; i < arrlen(weapon_order); ++i)
    {
        weapontype_t weapon = weapon_order[i];

        boolean selectable = G_WeaponSelectable(weapon);
        boolean disabled = !selectable && player->weaponowned[weapon];

        if (G_VanillaWeaponSelection(weapon) != weapon && weapon != curr)
        {
            disabled = true;
        }

        if (selectable || disabled)
        {
            weapon_icon_t icon = {.weapon = weapon,
                                  .state = 0,
                                  .darkened = disabled};
            if (weapon == curr)
            {
                icon.state = 1;
                curr_pos = array_size(weapon_icons);
            }
            array_push(weapon_icons, icon);
        }
    }
}

static void DrawIcon(int x, int y, sbarelem_t *elem, weapon_icon_t icon)
{
    char lump[9] = {0};
    M_snprintf(lump, sizeof(lump), "%s%d", names[icon.weapon], icon.state);

    patch_t *patch = V_CachePatchName(lump, PU_CACHE);

    byte *cr = icon.darkened ? cr_dark : NULL;

    if (cr && elem->tranmap)
    {
        V_DrawPatchTRTL(x, y, patch, cr, elem->tranmap);
    }
    else if (elem->tranmap)
    {
        V_DrawPatchTL(x, y, patch, elem->tranmap);
    }
    else
    {
        V_DrawPatchTranslated(x, y, patch, cr);
    }
}

void ST_DrawCarousel(int x, int y, sbarelem_t *elem)
{
    if (duration == 0)
    {
        return;
    }

    DrawIcon(SCREENWIDTH / 2, y, elem, weapon_icons[curr_pos]);

    for (int i = curr_pos + 1, k = 1; i < array_size(weapon_icons) && k < 3;
         ++i, ++k)
    {
        DrawIcon(SCREENWIDTH / 2 + k * 64, y, elem, weapon_icons[i]);
    }

    for (int i = curr_pos - 1, k = 1; i >= 0 && k < 3; --i, ++k)
    {
        DrawIcon(SCREENWIDTH / 2 - k * 64, y, elem, weapon_icons[i]);
    }
}
