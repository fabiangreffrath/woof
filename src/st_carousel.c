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

#include <math.h>

#include "d_items.h"
#include "d_player.h"
#include "doomdef.h"
#include "doomtype.h"
#include "doomstat.h"
#include "g_nextweapon.h"
#include "i_timer.h"
#include "m_array.h"
#include "m_misc.h"
#include "r_defs.h"
#include "r_draw.h"
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

typedef enum
{
    wpi_none = -1,
    wpi_regular,
    wpi_selected,
    wpi_disabled
} weapon_icon_state_t;

typedef struct
{
    weapontype_t weapon;
    weapon_icon_state_t state;
} weapon_icon_t;

static weapon_icon_t *weapon_icons;
static int selected_index = 0;

static int last_index = -1;
static int last_time;
static int distance;

static int duration;

void ST_ResetCarousel(void)
{
    last_index = -1;
    last_time = 0;
    distance = 0;
    duration = 0;
}

static void BuildWeaponIcons(const player_t *player)
{
    array_clear(weapon_icons);

    for (int i = 0; i < arrlen(weapon_order); ++i)
    {
        weapontype_t weapon = weapon_order[i];
        weapon_icon_state_t state = wpi_none;

        if (last_index == -1 && weapon == player->readyweapon)
        {
            last_index = array_size(weapon_icons);
        }

        if (G_WeaponSelectable(weapon))
        {
            if (player->nextweapon == weapon)
            {
                selected_index = array_size(weapon_icons);
                state = wpi_selected;
            }
            else if (G_AdjustSelection(weapon) != weapon)
            {
                state = wpi_disabled;
            }
            else
            {
                state = wpi_regular;
            }
        }
        else if (player->weaponowned[weapon])
        {
            state = wpi_disabled;
        }

        if (state != wpi_none)
        {
            weapon_icon_t icon = {weapon, state};
            array_push(weapon_icons, icon);
        }
    }
}

void ST_UpdateCarousel(player_t *player)
{
    if (G_NextWeaponActivate())
    {
        duration = TICRATE / 2;
    }

    if (duration == 0)
    {
        return;
    }

    if (automap_on || menuactive || paused || player->playerstate == PST_DEAD
        || player->health <= 0 || consoleplayer != displayplayer)
    {
        ST_ResetCarousel();
        return;
    }

    if (player->switching == weapswitch_none
        && player->pendingweapon == wp_nochange)
    {
        --duration;
    }

    BuildWeaponIcons(player);

    if (last_index != selected_index)
    {
        distance = selected_index - last_index;
        distance = 64 * CLAMP(distance, -2, 2);
        last_index = selected_index;
        last_time = I_GetTimeMS();
    }
}

static void DrawIcon(int x, int y, sbarelem_t *elem, weapon_icon_t icon)
{
    char lump[9] = {0};
    const char *name;
    if (weaponinfo[icon.weapon].carouselicon)
    {
        name = weaponinfo[icon.weapon].carouselicon;
    }
    else
    {
        name = names[icon.weapon];
    }

    M_snprintf(lump, sizeof(lump), "%s%d", name, icon.state == wpi_selected);

    patch_t *patch = V_CachePatchName(lump, PU_CACHE);

    byte *cr = icon.state == wpi_disabled ? cr_dark : NULL;

    if (cr && elem->tranmap)
    {
        V_DrawPatchTRTL(x, y, (crop_t){0}, patch, cr, elem->tranmap);
    }
    else if (elem->tranmap)
    {
        V_DrawPatchTL(x, y, (crop_t){0}, patch, elem->tranmap);
    }
    else
    {
        V_DrawPatchTR(x, y, (crop_t){0}, patch, cr);
    }
}

static int CalcOffset(void)
{
    if (distance)
    {
        const int delta = I_GetTimeMS() - last_time;

        if (delta < 125)
        {
            const float x = 1.0f - delta / 125.0f;
            return lroundf(distance * x * x);
        }

        distance = 0;
    }

    return 0;
}

void ST_EraseCarousel(int y)
{
    static boolean erase;

    if (duration > 0)
    {
        R_VideoErase(0, y - 16, video.unscaledw, 32);
        erase = true;
    }
    else if (erase)
    {
        R_VideoErase(0, y - 16, video.unscaledw, 32);
        erase = false;
    }
}

void ST_DrawCarousel(int x, int y, sbarelem_t *elem)
{
    if (duration == 0)
    {
        return;
    }

    const int offset = SCREENWIDTH / 2 + CalcOffset();
    DrawIcon(offset, y, elem, weapon_icons[selected_index]);

    for (int i = selected_index + 1, k = 1;
         i < array_size(weapon_icons) && k < 3; ++i, ++k)
    {
        DrawIcon(offset + k * 64, y, elem, weapon_icons[i]);
    }

    for (int i = selected_index - 1, k = 1; i >= 0 && k < 3; --i, ++k)
    {
        DrawIcon(offset - k * 64, y, elem, weapon_icons[i]);
    }
}
