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
#include <string.h>

#include "deh_bex_sounds.h"
#include "deh_defs.h"
#include "deh_io.h"
#include "deh_main.h"
#include "m_array.h"
#include "m_hashmap.h"
#include "m_misc.h"
#include "sounds.h"

//
// DSDHacked Sounds
//

sfxinfo_t *S_sfx = NULL;
int num_sfx;
static int max_sfx_number;
static char **original_soundnames;

static hashmap_t *translate;

void DEH_InitSFX(void)
{
    S_sfx = original_S_sfx;
    num_sfx = NUMSFX;
    max_sfx_number = NUMSFX - 1;

    original_soundnames = calloc(NUMSFX, sizeof(*original_soundnames));
    for (int i = 1; i < NUMSFX; ++i)
    {
        original_soundnames[i] =
            S_sfx[i].name ? M_StringDuplicate(S_sfx[i].name) : NULL;
    }
}

void DEH_FreeSFX(void)
{
    for (int i = 1; i < NUMSFX; i++)
    {
        if (original_soundnames[i])
        {
            free(original_soundnames[i]);
        }
    }
    free(original_soundnames);
}

int DEH_SoundsTranslate(int sfx_number)
{
    max_sfx_number = MAX(max_sfx_number, sfx_number);

    if (sfx_number < NUMSFX)
    {
        return sfx_number;
    }

    if (!translate)
    {
        translate = hashmap_init(1024);

        S_sfx = NULL;
        array_resize(S_sfx, num_sfx);
        memcpy(S_sfx, original_S_sfx, num_sfx * sizeof(sfxinfo_t));
    }

    int index;
    if (hashmap_get(translate, sfx_number, &index))
    {
        return index;
    }

    index = num_sfx;
    hashmap_put(translate, sfx_number, &index);

    sfxinfo_t sfx = {.priority = 127, .lumpnum = -1};
    array_push(S_sfx, sfx);

    ++num_sfx;

    return index;
}

static int SoundsGetIndex(const char *key)
{
    for (int i = 1; i < NUMSFX; ++i)
    {
        if (original_soundnames[i]
            && !strncasecmp(original_soundnames[i], key, 6))
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
    i = DEH_SoundsTranslate(i);

    return i;
}

int DEH_SoundsGetNewIndex(void)
{
    ++max_sfx_number;
    return DEH_SoundsTranslate(max_sfx_number);
}

//
// The actual parser
//

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
