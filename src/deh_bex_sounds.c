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
#include "sounds.h"

//
// DSDHacked Sounds
//

sfxinfo_t *S_sfx = NULL;
int num_sfx;
static int sfx_index;
static char **deh_soundnames = NULL;
static byte *sfx_state = NULL;

void DEH_InitSFX(void)
{
    S_sfx = original_S_sfx;
    num_sfx = NUMSFX;
    sfx_index = NUMSFX - 1;

    array_grow(deh_soundnames, num_sfx);
    for (int i = 1; i < num_sfx; i++)
    {
        deh_soundnames[i] = S_sfx[i].name ? strdup(S_sfx[i].name) : NULL;
    }

    array_grow(sfx_state, num_sfx);
    memset(sfx_state, 0, num_sfx * sizeof(*sfx_state));
}

void DEH_FreeSFX(void)
{
    for (int i = 1; i < array_capacity(deh_soundnames); i++)
    {
        if (deh_soundnames[i])
        {
            free(deh_soundnames[i]);
        }
    }
    array_free(deh_soundnames);
    array_free(sfx_state);
}

void DEH_SoundsEnsureCapacity(int limit)
{
    if (limit > sfx_index)
    {
        sfx_index = limit;
    }

    if (limit < num_sfx)
    {
        return;
    }

    const int old_num_sfx = num_sfx;

    static int first_allocation = true;
    if (first_allocation)
    {
        S_sfx = NULL;
        array_grow(S_sfx, old_num_sfx + limit);
        memcpy(S_sfx, original_S_sfx, old_num_sfx * sizeof(*S_sfx));
        first_allocation = false;
    }
    else
    {
        array_grow(S_sfx, limit);
    }

    num_sfx = array_capacity(S_sfx);
    const int size_delta = num_sfx - old_num_sfx;
    memset(S_sfx + old_num_sfx, 0, size_delta * sizeof(*S_sfx));

    if (sfx_state)
    {
        array_grow(sfx_state, size_delta);
        memset(sfx_state + old_num_sfx, 0, size_delta * sizeof(*sfx_state));
    }

    for (int i = old_num_sfx; i < num_sfx; ++i)
    {
        S_sfx[i].priority = 127;
        S_sfx[i].lumpnum = -1;
    }
}

int DEH_SoundsGetIndex(const char *key, size_t length)
{
    for (int i = 1; i < num_sfx; ++i)
    {
        if (S_sfx[i].name
            && strlen(S_sfx[i].name) == length
            && !strncasecmp(S_sfx[i].name, key, length)
            && !sfx_state[i])
        {
            sfx_state[i] = true; // sfx has been edited
            return i;
        }
    }

    return -1;
}

int DEH_SoundsGetOriginalIndex(const char *key)
{
    for (int i = 1; i < array_capacity(deh_soundnames); ++i)
    {
        if (deh_soundnames[i] && !strncasecmp(deh_soundnames[i], key, 6))
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
    DEH_SoundsEnsureCapacity(i);

    return i;
}

int DEH_SoundsGetNewIndex(void)
{
    sfx_index++;
    DEH_SoundsEnsureCapacity(sfx_index);
    return sfx_index;
}

//
// The actual parser
//

static void *DEH_BEXSoundsStart(deh_context_t *context, char *line)
{
    char s[9];

    if (sscanf(line, "%8s", s) == 0 || strcmp("[SOUNDS]", s))
    {
        DEH_Warning(context, "Parse error on section start");
    }

    return NULL;
}

static void DEH_BEXSoundsParseLine(deh_context_t *context, char *line, void *tag)
{
    char *soundnum, *value;

    if (!DEH_ParseAssignment(line, &soundnum, &value))
    {
        DEH_Warning(context, "Failed to parse sound assignment");
        return;
    }

    const int len = strlen(value);
    if (len < 1 || len > 6)
    {
        DEH_Warning(context, "Invalid sound string length");
        return;
    }

    const int match = DEH_SoundsGetOriginalIndex(soundnum);
    if (match >= 0)
    {
        S_sfx[match].name = strdup(value);
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
