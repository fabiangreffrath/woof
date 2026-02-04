//
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2014 Fabian Greffrath
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
// Parses [SOUNDS] sections in BEX files
//

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "deh_defs.h"
#include "deh_io.h"
#include "deh_main.h"
#include "dsdh_main.h"
#include "m_misc.h"
#include "sounds.h"

static int SoundsGetIndex(const char *key)
{
    for (int i = 1; i < NUMSFX; ++i)
    {
        if (original_S_sfx[i].name
            && !strncasecmp(original_S_sfx[i].name, key, 6))
        {
            return i;
        }
    }

    // is it a number?
    for (const char *c = key; *c; c++)
    {
        if (!isdigit(*c))
        {
            return -1;
        }
    }

    int i = atoi(key);
    i = DSDH_SoundTranslate(i);

    return i;
}

static int DEH_BEXSoundsStart(deh_context_t *context, char *line)
{
    char s[9];

    if (sscanf(line, "%8s", s) == 0 || strcmp("[SOUNDS]", s))
    {
        DEH_Warning(context, "Parse error on section start");
    }

    return 0;
}

static void DEH_BEXSoundsParseLine(deh_context_t *context, char *line, int tag)
{
    char *sound_key, *sound_name;

    if (!DEH_ParseAssignment(line, &sound_key, &sound_name))
    {
        DEH_Warning(context, "Failed to parse sound assignment");
        return;
    }

    const int len = strlen(sound_name);
    if (len < 1 || len > 6)
    {
        DEH_Warning(context, "Invalid sound string length");
        return;
    }

    const int match = SoundsGetIndex(sound_key);
    if (match >= 0)
    {
        if (S_sfx[match].name)
        {
            free(S_sfx[match].name);
        }
        S_sfx[match].name = M_StringDuplicate(sound_name);
    }
}

deh_section_t deh_section_bex_sounds =
{
    "[SOUNDS]",
    NULL,
    DEH_BEXSoundsStart,
    DEH_BEXSoundsParseLine,
    NULL,
    NULL,
};
