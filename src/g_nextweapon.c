//
// Copyright(C) 2005-2014 Simon Howard
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
#include "doomstat.h"
#include "doomtype.h"
#include "i_system.h"
#include "m_input.h"
#include "st_carousel.h"

boolean doom_weapon_cycle;

weapontype_t vanilla_nextweapon;

static const weapontype_t weapon_order[] = {
    wp_fist,
    wp_chainsaw,
    wp_pistol,
    wp_shotgun,
    wp_supershotgun,
    wp_chaingun,
    wp_missile,
    wp_plasma,
    wp_bfg
};

const weapontype_t nextweapon_translate[] = {
    [wp_fist] = wp_fist,
    [wp_chainsaw] = wp_fist,
    [wp_pistol] = wp_pistol,
    [wp_shotgun] = wp_shotgun,
    [wp_supershotgun] = wp_shotgun,
    [wp_chaingun] = wp_chaingun,
    [wp_missile] = wp_missile,
    [wp_plasma] = wp_plasma,
    [wp_bfg] = wp_bfg
};

boolean G_WeaponSelectable(weapontype_t weapon)
{
    // Can't select the super shotgun in Doom 1.

    if (weapon == wp_supershotgun && !ALLOW_SSG)
    {
        return false;
    }

    // These weapons aren't available in shareware.

    if ((weapon == wp_plasma || weapon == wp_bfg)
        && gamemission == doom && gamemode == shareware)
    {
        return false;
    }

    // Can't select a weapon if we don't own it.

    if (!players[consoleplayer].weaponowned[weapon])
    {
        return false;
    }

    // Can't select the fist if we have the chainsaw, unless
    // we also have the berserk pack.

    if ((demo_compatibility || (!demo_compatibility && doom_weapon_cycle))
        && weapon == wp_fist
        && players[consoleplayer].weaponowned[wp_chainsaw]
        && !players[consoleplayer].powers[pw_strength])
    {
        return false;
    }

    return true;
}

weapontype_t G_AdjustSelection(weapontype_t weapon)
{
    if (!demo_compatibility && !doom_weapon_cycle)
    {
        return weapon;
    }

    const player_t *player = &players[consoleplayer];

    if (weapon == wp_fist && player->weaponowned[wp_chainsaw]
        && (player->nextweapon != wp_chainsaw || !player->powers[pw_strength]))
    {
        weapon = wp_chainsaw;
    }
    else if (ALLOW_SSG && weapon == wp_shotgun
             && player->weaponowned[wp_supershotgun]
             && player->nextweapon != wp_supershotgun)
    {
        weapon = wp_supershotgun;
    }

    return weapon;
}

static weapontype_t NextWeapon(int direction)
{
    int i;
    weapontype_t weapon = players[consoleplayer].nextweapon;

    for (i = 0; i < arrlen(weapon_order); ++i)
    {
        if (weapon_order[i] == weapon)
        {
            break;
        }
    }

    if (i == arrlen(weapon_order))
    {
        I_Error("Invalid weapon type %d", (int)weapon);
    }

    // Switch weapon. Don't loop forever.

    int start_i = i;
    do
    {
        i += direction;
        i = (i + arrlen(weapon_order)) % arrlen(weapon_order);
    } while (i != start_i && !G_WeaponSelectable(weapon_order[i]));

    return G_AdjustSelection(weapon_order[i]);
}

typedef enum
{
    nw_state_none,
    nw_state_activate,
    nw_state_deactivate,
    nw_state_cmd
} next_weapon_state_t;

static next_weapon_state_t state;
static boolean currently_active;

void G_NextWeaponUpdate(void)
{
    weapontype_t weapon = wp_nochange;

    if (M_InputActivated(input_prevweapon))
    {
        weapon = NextWeapon(-1);
    }
    else if (M_InputActivated(input_nextweapon))
    {
        weapon = NextWeapon(1);
    }
    else if (M_InputDeactivated(input_prevweapon)
             || M_InputDeactivated(input_nextweapon))
    {
        if (currently_active)
        {
            currently_active = false;
            state = nw_state_deactivate;
        }
    }

    if (weapon != wp_nochange)
    {
        currently_active = true;
        state = nw_state_activate;
        players[consoleplayer].nextweapon = weapon;
    }
}

boolean G_NextWeaponActivate(void)
{
    if (state == nw_state_activate)
    {
        state = nw_state_none;
        return true;
    }
    return false;
}

boolean G_NextWeaponDeactivate(void)
{
    if (state == nw_state_deactivate)
    {
        state = nw_state_cmd;
        return true;
    }
    return false;
}

void G_NextWeaponResendCmd(void)
{
    weapontype_t weapon;

    if (players[consoleplayer].pendingweapon == wp_nochange)
    {
        weapon = players[consoleplayer].readyweapon;
    }
    else
    {
        weapon = players[consoleplayer].pendingweapon;
    }

    if (state == nw_state_cmd
        && players[consoleplayer].nextweapon != weapon
        && players[consoleplayer].switching != weapswitch_none)
    {
        state = nw_state_deactivate;
    }
}

void G_NextWeaponReset(weapontype_t weapon)
{
    currently_active = false;
    state = nw_state_none;
    players[consoleplayer].nextweapon = players[consoleplayer].readyweapon;
    players[consoleplayer].nextweapon = G_AdjustSelection(weapon);
    ST_ResetCarousel();
}
