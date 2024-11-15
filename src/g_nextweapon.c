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
#include "doomstat.h"
#include "doomtype.h"
#include "m_input.h"

boolean doom_weapon_cycle;

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

static weapontype_t CurrentWeapon(void)
{
    return players[consoleplayer].nextweapon;
}

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

    if (ALLOW_SSG
        && weapon == wp_shotgun
        && players[consoleplayer].weaponowned[wp_supershotgun]
        && CurrentWeapon() != wp_supershotgun)
    {
        weapon = wp_supershotgun;
    }

    return weapon;
}

static weapontype_t NextWeapon(int direction)
{
    int i;
    weapontype_t weapon = CurrentWeapon();

    for (i = 0; i < arrlen(weapon_order); ++i)
    {
        if (weapon_order[i] == weapon)
        {
            break;
        }
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
    nw_state_begin,
    nw_state_end
} next_weapon_state_t;

static next_weapon_state_t state;

void G_NextWeaponUpdate(void)
{
    player_t *player = &players[consoleplayer];

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
        state = nw_state_end;
    }

    if (weapon != wp_nochange)
    {
        state = nw_state_begin;
        player->nextweapon = weapon;
    }
}

boolean G_NextWeaponBegin(void)
{
    if (state == nw_state_begin)
    {
        state = nw_state_none;
        return true;
    }
    return false;
}

boolean G_NextWeaponEnd(void)
{
    if (state == nw_state_end)
    {
        state = nw_state_none;
        return true;
    }
    return false;
}
