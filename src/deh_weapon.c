//
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2021 Roman Fomin
// Copyright(C) 2025 Guilherme Miranda
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
//
//
// Parses "Weapon" sections in dehacked files
//

#include <stdio.h>
#include <stdlib.h>

#include "d_items.h"
#include "deh_defs.h"
#include "deh_io.h"
#include "deh_main.h"
#include "deh_mapping.h"
#include "m_misc.h"

const bex_bitflags_t mbf21_flags[] = {
    {"NOTHRUST",       WPF_NOTHRUST      },
    {"SILENT",         WPF_SILENT        },
    {"NOAUTOFIRE",     WPF_NOAUTOFIRE    },
    {"FLEEMELEE",      WPF_FLEEMELEE     },
    {"AUTOSWITCHFROM", WPF_AUTOSWITCHFROM},
    {"NOAUTOSWITCHTO", WPF_NOAUTOSWITCHTO},
};

DEH_BEGIN_MAPPING(weapon_mapping, weaponinfo_t)
    DEH_MAPPING("Ammo type", ammo)
    DEH_MAPPING("Deselect frame", upstate)
    DEH_MAPPING("Select frame", downstate)
    DEH_MAPPING("Bobbing frame", readystate)
    DEH_MAPPING("Shooting frame", atkstate)
    DEH_MAPPING("Firing frame", flashstate)
    // mbf21
    DEH_MAPPING("MBF21 Bits", flags)
    DEH_MAPPING("Ammo per shot", ammopershot)
    // id24
    DEH_UNSUPPORTED_MAPPING("Slot")
    DEH_UNSUPPORTED_MAPPING("Slot Priority")
    DEH_UNSUPPORTED_MAPPING("Switch Priority")
    DEH_UNSUPPORTED_MAPPING("Initial Owned")
    DEH_UNSUPPORTED_MAPPING("Initial Raised")
    DEH_MAPPING("Carousel Icon", carouselicon)
    DEH_UNSUPPORTED_MAPPING("Allow switch with owned weapon")
    DEH_UNSUPPORTED_MAPPING("No switch with owned weapon")
    DEH_UNSUPPORTED_MAPPING("Allow switch with owned item")
    DEH_UNSUPPORTED_MAPPING("No switch with owned item")
    // mbf2y
    DEH_UNSUPPORTED_MAPPING("MBF2y Bits")
DEH_END_MAPPING

//
// Notable, unsupported properties:
//
// From ZDoom:
// * "Decal"
// * "Ammo use"
// * "Min ammo"
//

#define SILENTLY_IGNORE_WEAPON_PROP(str) \
    (!strcasecmp(str, "Decal") || !strcasecmp(str, "Ammo use") || !strcasecmp(str, "Min ammo"))

static void *DEH_WeaponStart(deh_context_t *context, char *line)
{
    int weapon_number = -1;

    if (sscanf(line, "Weapon %i", &weapon_number) != 1)
    {
        DEH_Warning(context, "Parse error on section start");
        return NULL;
    }

    if (weapon_number < 0 || weapon_number >= NUMWEAPONS)
    {
        DEH_Warning(context, "Invalid weapon number: %i", weapon_number);
        return NULL;
    }

    return &weaponinfo[weapon_number];
}

static void DEH_WeaponParseLine(deh_context_t *context, char *line, void *tag)
{
    if (tag == NULL)
    {
        return;
    }

    weaponinfo_t *weapon = (weaponinfo_t *)tag;

    // Pasre the assignment
    char *variable_name, *value;
    if (!DEH_ParseAssignment(line, &variable_name, &value))
    {
        DEH_Warning(context, "Failed to parse assignment");
        return;
    }

    if (!strcasecmp(variable_name, "Carousel Icon"))
    {
        weapon->carouselicon = M_StringDuplicate(value);
        return;
    }
    else if (!strcasecmp(variable_name, "Ammo per shot"))
    {
        weapon->intflags |= WIF_ENABLEAPS;
    }
    else if (SILENTLY_IGNORE_WEAPON_PROP(variable_name))
    {
        return;
    }

    // most values are integers
    int ivalue = atoi(value);
    DEH_SetMapping(context, &weapon_mapping, weapon, variable_name, ivalue);
}

static void DEH_WeaponSHA1Sum(sha1_context_t *context)
{
    for (int i = 0; i < NUMWEAPONS; ++i)
    {
        DEH_StructSHA1Sum(context, &weapon_mapping, &weaponinfo[i]);
    }
}

deh_section_t deh_section_weapon =
{
    "Weapon",
    NULL,
    DEH_WeaponStart,
    DEH_WeaponParseLine,
    NULL,
    DEH_WeaponSHA1Sum,
};
