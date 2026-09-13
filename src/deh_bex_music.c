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
#include <stdlib.h>
#include <string.h>

#include "deh_defs.h"
#include "deh_io.h"
#include "deh_main.h"
#include "doomtype.h"
#include "m_misc.h"
#include "sounds.h"

static boolean modified[NUMMUSIC];

static int MusicGetIndex(const char *key)
{
    for (int i = 1; i < NUMMUSIC; ++i)
    {
        if (S_music[i].name && !strncasecmp(S_music[i].name, key, 6))
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
    if (i >= NUMMUSIC)
    {
        return -1;
    }
    return i;
}

//
// The actual parser
//

static int DEH_BEXMusicStart(deh_context_t *context, char *line)
{
    char s[9];

    if (sscanf(line, "%8s", s) == 0 || strcmp("[MUSIC]", s))
    {
        DEH_Warning(context, "Parse error on section start");
    }

    return 0;
}

static void DEH_BEXMusicParseLine(deh_context_t *context, char *line, int tag)
{
    char *music_key, *music_name;

    if (!DEH_ParseAssignment(line, &music_key, &music_name))
    {
        DEH_Warning(context, "Failed to parse music assignment");
        return;
    }

    const int len = strlen(music_name);
    if (len < 1 || len > 6)
    {
        DEH_Warning(context, "Invalid music string length");
        return;
    }

    int index = MusicGetIndex(music_key);
    if (index >= 0)
    {
        if (modified[index])
        {
            free(S_music[index].name);
        }
        S_music[index].name = M_StringDuplicate(music_name);
        modified[index] = true;
    }
    else
    {
        DEH_Warning(context, "Unknown music replacement %s for %s", music_name, music_key);
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
