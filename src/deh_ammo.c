//
// Copyright(C) 2005-2014 Simon Howard
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
// Parses "Ammo" sections in dehacked files
//

#include <stdio.h>
#include <stdlib.h>

#include "deh_defs.h"
#include "deh_io.h"
#include "deh_main.h"
// #include "deh_mapping.h"
#include "doomdef.h"
#include "p_inter.h"

// TODO:

/*
DEH_BEGIN_MAPPING(ammo_mapping, ammoinfo_t)
    DEH_MAPPING("Per ammo", clipammo)
    DEH_MAPPING("Max ammo", maxammo)
    // id24
    DEH_UNSUPPORTED_MAPPING("Initial ammo")
    DEH_UNSUPPORTED_MAPPING("Max upgraded ammo")
    DEH_UNSUPPORTED_MAPPING("Box ammo")
    DEH_UNSUPPORTED_MAPPING("Backpack ammo")
    DEH_UNSUPPORTED_MAPPING("Weapon ammo")
    DEH_UNSUPPORTED_MAPPING("Dropped ammo")
    DEH_UNSUPPORTED_MAPPING("Dropped box ammo")
    DEH_UNSUPPORTED_MAPPING("Dropped backpack ammo")
    DEH_UNSUPPORTED_MAPPING("Dropped weapon ammo")
    DEH_UNSUPPORTED_MAPPING("Deathmatch weapon ammo")
    DEH_UNSUPPORTED_MAPPING("Skill 1 multiplier")
    DEH_UNSUPPORTED_MAPPING("Skill 2 multiplier")
    DEH_UNSUPPORTED_MAPPING("Skill 3 multiplier")
    DEH_UNSUPPORTED_MAPPING("Skill 4 multiplier")
    DEH_UNSUPPORTED_MAPPING("Skill 5 multiplier")
DEH_END_MAPPING
*/

static void *DEH_AmmoStart(deh_context_t *context, char *line)
{
    int ammo_number = -1;

    if (sscanf(line, "Ammo %i", &ammo_number) != 1)
    {
        DEH_Warning(context, "Parse error on section start");
        return NULL;
    }

    if (ammo_number < 0 || ammo_number >= NUMAMMO)
    {
        DEH_Warning(context, "Invalid ammo number: %i", ammo_number);
        return NULL;
    }

    return &maxammo[ammo_number];
}

static void DEH_AmmoParseLine(deh_context_t *context, char *line, void *tag)
{
    if (tag == NULL)
    {
        return;
    }

    int ammo_number = ((int *)tag) - maxammo;

    // Parse the assignment
    char *variable_name, *value;
    if (!DEH_ParseAssignment(line, &variable_name, &value))
    {
        // Failed to parse
        DEH_Warning(context, "Failed to parse assignment");
        return;
    }

    int ivalue = atoi(value);

    // maxammo
    if (!strcasecmp(variable_name, "Per ammo"))
    {
        clipammo[ammo_number] = ivalue;
    }
    else if (!strcasecmp(variable_name, "Max ammo"))
    {
        maxammo[ammo_number] = ivalue;
    }
    else
    {
        DEH_Debug(context, "Field named '%s' not found", variable_name);
    }
}

static void DEH_AmmoSHA1Hash(sha1_context_t *context)
{
    for (int i = 0; i < NUMAMMO; ++i)
    {
        SHA1_UpdateInt32(context, clipammo[i]);
        SHA1_UpdateInt32(context, maxammo[i]);
    }
}

deh_section_t deh_section_ammo =
{
    "Ammo",
    NULL,
    DEH_AmmoStart,
    DEH_AmmoParseLine,
    NULL,
    DEH_AmmoSHA1Hash,
};
