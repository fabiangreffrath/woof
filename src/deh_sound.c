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
// Parses "Sound" sections in dehacked files
//

#include <stdio.h>
#include <stdlib.h>

#include "deh_bex_sounds.h"
#include "deh_defs.h"
#include "deh_io.h"
#include "deh_main.h"
#include "deh_mapping.h"
#include "sounds.h"

//
// So what's going on in here?
// Most of these options actually pose potential security risks,
// and/or aren't very useful, anyway.
//
// See: https://eternity.youfailit.net/wiki/DeHackEd_/_BEX_Reference/Sound_Block
//

DEH_BEGIN_MAPPING(sound_mapping, sfxinfo_t)
    DEH_UNSUPPORTED_MAPPING("Offset")
    DEH_MAPPING("Zero/One", singularity)
    DEH_MAPPING("Value", priority)
    DEH_UNSUPPORTED_MAPPING("Zero 1")
    DEH_UNSUPPORTED_MAPPING("Zero 2")
    DEH_UNSUPPORTED_MAPPING("Zero 3")
    DEH_UNSUPPORTED_MAPPING("Zero 4")
    DEH_UNSUPPORTED_MAPPING("Neg. One 1")
    DEH_UNSUPPORTED_MAPPING("Neg. One 2")
DEH_END_MAPPING

static void *DEH_SoundStart(deh_context_t *context, char *line)
{
    int sound_number = -1;

    if (sscanf(line, "Sound %i", &sound_number) != 1)
    {
        DEH_Warning(context, "Parse error on section start");
        return NULL;
    }

    if (sound_number < 0)
    {
        DEH_Warning(context, "Invalid sound number: %i", sound_number);
        return NULL;
    }

    // DSDHacked
    DEH_SoundsEnsureCapacity(sound_number);

    return &S_sfx[sound_number];
}

static void DEH_SoundParseLine(deh_context_t *context, char *line, void *tag)
{
    if (tag == NULL)
    {
        return;
    }

    sfxinfo_t *sfx = (sfxinfo_t *)tag;

    // Parse the assignment
    char *variable_name, *value;
    if (!DEH_ParseAssignment(line, &variable_name, &value))
    {
        DEH_Warning(context, "Failed to parse assignment");
        return;
    }

    // all values are integers
    int ivalue = atoi(value);

    // Set the field value
    DEH_SetMapping(context, &sound_mapping, sfx, variable_name, ivalue);
}

deh_section_t deh_section_sound =
{
    "Sound",
    NULL,
    DEH_SoundStart,
    DEH_SoundParseLine,
    NULL,
    NULL,
};
