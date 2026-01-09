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
// Parses [MUSIC] sections in BEX files
//

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "deh_bex_music.h"
#include "deh_defs.h"
#include "deh_io.h"
#include "deh_main.h"
#include "m_array.h"
#include "m_misc.h"
#include "sounds.h"

//
//   Music
//

musicinfo_t *S_music = NULL;
int num_music;
static char **deh_musicnames = NULL;
static byte *music_state = NULL;

void DEH_InitMusic(void)
{
    S_music = original_S_music;
    num_music = NUMMUSIC;

    array_grow(deh_musicnames, num_music);
    for (int i = 1; i < num_music; i++)
    {
        deh_musicnames[i] = S_music[i].name ? strdup(S_music[i].name) : NULL;
    }

    array_grow(music_state, num_music);
    memset(music_state, 0, num_music * sizeof(*music_state));
}

void DEH_FreeMusic(void)
{
    array_free(music_state);
}

int DEH_MusicGetIndex(const char *key, int length)
{
    for (int i = 1; i < num_music; ++i)
    {
        if (S_music[i].name
            && strlen(S_music[i].name) == length
            && !strncasecmp(S_music[i].name, key, length)
            && !music_state[i])
        {
            music_state[i] = true; // music has been edited
            return i;
        }
    }

    return -1;
}

int DEH_MusicGetOriginalIndex(const char *key)
{
    for (int i = 1; i < array_capacity(deh_musicnames); ++i)
    {
        if (deh_musicnames[i] && !strncasecmp(deh_musicnames[i], key, 6))
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
    // no DSDHacked for music
    // DEH_MusicEnsureCapacity(i);
    if (i >= num_music)
    {
        return -1;
    }
    return i;
}

//
// The actual parser
//

static void *DEH_BEXMusicStart(deh_context_t *context, char *line)
{
    char s[9];

    if (sscanf(line, "%8s", s) == 0 || strcmp("[MUSIC]", s))
    {
        DEH_Warning(context, "Parse error on section start");
    }

    return NULL;
}

static void DEH_BEXMusicParseLine(deh_context_t *context, char *line, void *tag)
{
    char *music_mnemonic, *value;

    if (!DEH_ParseAssignment(line, &music_mnemonic, &value))
    {
        DEH_Warning(context, "Failed to parse music assignment");
        return;
    }

    const int len = strlen(value);
    if (len < 1 || len > 6)
    {
        DEH_Warning(context, "Invalid music string length");
        return;
    }

    int index = DEH_MusicGetOriginalIndex(music_mnemonic);
    if (index >= 0)
    {
        S_music[index].name = M_StringDuplicate(value);
    }
    else
    {
        DEH_Warning(context, "Unknown music replacemnt %s for %s", value, music_mnemonic);
    }
}

deh_section_t deh_section_bex_music =
{
    "[MUSIC]",
    NULL,
    DEH_BEXMusicStart,
    DEH_BEXMusicParseLine,
    NULL,
    NULL,
};
