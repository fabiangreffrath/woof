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

#ifndef G_NEXTWEAPON_H
#define G_NEXTWEAPON_H

#include "doomdef.h"
#include "doomtype.h"

extern boolean doom_weapon_cycle;

boolean G_WeaponSelectable(weapontype_t weapon);

weapontype_t G_AdjustSelection(weapontype_t weapon);

void G_NextWeaponUpdate(void);

boolean G_NextWeaponBegin(void);

boolean G_NextWeaponEnd(void);

void G_NextWeaponReset(void);

extern weapontype_t vanilla_nextweapon;

#endif
