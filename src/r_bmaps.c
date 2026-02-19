//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2013-2017 Brad Harding
// Copyright(C) 2017 Fabian Greffrath
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// DESCRIPTION:
//	Brightmaps for wall textures
//	Adapted from doomretro/src/r_data.c:97-209
//

#include <stdlib.h>
#include <string.h>

#include "doomdef.h"
#include "doomstat.h"
#include "doomtype.h"
#include "info.h"
#include "m_array.h"
#include "m_hashmap.h"
#include "m_misc.h"
#include "mn_menu.h"
#include "r_data.h"
#include "m_scanner.h"
#include "w_wad.h"
#include "z_zone.h"

boolean brightmaps;
boolean force_brightmaps;

#define COLORMASK_SIZE 256

const byte nobrightmap[COLORMASK_SIZE] = {0};

const byte *dc_brightmap = nobrightmap;

typedef struct
{
    const char *name;
    byte colormask[COLORMASK_SIZE];
} brightmap_t;

static void ReadColormask(scanner_t *s, byte *colormask)
{
    memset(colormask, 0, COLORMASK_SIZE);
    do
    {
        unsigned int color1 = 0, color2 = 0;

        SC_MustGetToken(s, TK_IntConst);
        color1 = SC_GetNumber(s);
        if (color1 >= 0 && color1 < COLORMASK_SIZE)
        {
            colormask[color1] = 1;
        }

        if (!SC_CheckToken(s, '-'))
        {
            continue;
        }

        SC_MustGetToken(s, TK_IntConst);
        color2 = SC_GetNumber(s);
        if (color2 >= 0 && color2 < COLORMASK_SIZE)
        {
            for (int i = color1 + 1; i <= color2; ++i)
            {
                colormask[i] = 1;
            }
        }
    } while (SC_CheckToken(s, ','));
}

static brightmap_t *allbrightmaps;

static hashmap_t *textures_bm;
static hashmap_t *flats_bm;
static hashmap_t *sprites_bm;
static hashmap_t *states_bm;

static int GetBrightmap(const char *name)
{
    for (int i = array_size(allbrightmaps) - 1; i >= 0; --i)
    {
        if (!strcasecmp(allbrightmaps[i].name, name))
        {
            return i;
        }
    }
    return -1;
}

enum
{
    DOOM1AND2,
    DOOM1ONLY,
    DOOM2ONLY
};

static int ParseProperty(scanner_t *s)
{
    SC_MustGetToken(s, TK_Identifier);
    int idx = GetBrightmap(SC_GetString(s));
    if (idx < 0)
    {
        SC_Error(s, "brightmap '%s' not found", SC_GetString(s));
        return -1;
    }

    int game = DOOM1AND2;
    if (SC_CheckToken(s, TK_Identifier))
    {
        if (!strcasecmp("DOOM", SC_GetString(s))
            || !strcasecmp("DOOM1", SC_GetString(s)))
        {
            game = DOOM1ONLY;
            if (SC_CheckToken(s, '|'))
            {
                SC_MustGetToken(s, TK_Identifier);
                if (!strcasecmp("DOOM2", SC_GetString(s)))
                {
                    game = DOOM1AND2;
                }
                else
                {
                    SC_Error(s, "Expected 'DOOM2' but got '%s' instead.",
                             SC_GetString(s));
                }
            }
        }
        else if (!strcasecmp("DOOM2", SC_GetString(s)))
        {
            game = DOOM2ONLY;
        }
        else
        {
            SC_Rewind(s);
        }
    }
    if ((gamemission == doom && game == DOOM2ONLY)
        || (gamemission == doom2 && game == DOOM1ONLY))
    {
        return -1;
    }

    return idx;
}

const byte *R_BrightmapForTexName(const char *texname)
{
    if (textures_bm)
    {
        int *idx = hashmap_get_str(textures_bm, texname);
        if (idx)
        {
            return allbrightmaps[*idx].colormask;
        }
    }
    return nobrightmap;
}

const byte *R_BrightmapForSprite(const int type)
{
    if ((STRICTMODE(brightmaps) || force_brightmaps) && sprites_bm)
    {
        int *idx = hashmap_get(sprites_bm, type);
        if (idx)
        {
            return allbrightmaps[*idx].colormask;
        }
    }
    return nobrightmap;
}

const byte *R_BrightmapForFlatNum(const int num)
{
    if ((STRICTMODE(brightmaps) || force_brightmaps) && flats_bm)
    {
        int *idx = hashmap_get(flats_bm, num);
        if (idx)
        {
            return allbrightmaps[*idx].colormask;
        }
    }
    return nobrightmap;
}

const byte *R_BrightmapForState(const int state)
{
    if ((STRICTMODE(brightmaps) || force_brightmaps) && states_bm)
    {
        int *idx = hashmap_get(states_bm, state);
        if (idx)
        {
            return allbrightmaps[*idx].colormask;
        }
    }
    return nobrightmap;
}

void R_ParseBrightmaps(int lumpnum)
{
    force_brightmaps = W_IsWADLump(lumpnum);

    if (!allbrightmaps)
    {
        brightmap_t brightmap;
        brightmap.name = "NOBRIGHTMAP";
        memset(brightmap.colormask, 0, COLORMASK_SIZE);
        array_push(allbrightmaps, brightmap);
    }

    scanner_t *s = SC_Open("BRGHTMPS", W_CacheLumpNum(lumpnum, PU_CACHE),
                           W_LumpLength(lumpnum));

    while (SC_TokensLeft(s))
    {
        if (!SC_CheckToken(s, TK_Identifier))
        {
            SC_GetNextLineToken(s);
            continue;
        }
        if (!strcasecmp("BRIGHTMAP", SC_GetString(s)))
        {
            brightmap_t brightmap;
            SC_MustGetToken(s, TK_Identifier);
            brightmap.name = M_StringDuplicate(SC_GetString(s));
            ReadColormask(s, brightmap.colormask);
            array_push(allbrightmaps, brightmap);
        }
        else if (!strcasecmp("TEXTURE", SC_GetString(s)))
        {
            if (!SC_CheckRawString(s))
            {
                SC_Error(s, "expected lump name");
            }
            char *name = M_StringDuplicate(SC_GetString(s));
            M_StringToUpper(name);
            int idx = ParseProperty(s);
            if (idx >= 0)
            {
                if (!textures_bm)
                {
                    textures_bm = hashmap_init_str(128, sizeof(int));
                }
                hashmap_put_str(textures_bm, name, &idx);
            }
            free(name);
        }
        else if (!strcasecmp("SPRITE", SC_GetString(s)))
        {
            if (!SC_CheckRawString(s))
            {
                SC_Error(s, "expected lump name");
            }
            char *name = M_StringDuplicate(SC_GetString(s));
            int idx = ParseProperty(s);
            if (idx >= 0)
            {
                for (int i = 0; i < num_sprites; ++i)
                {
                    if (!strcasecmp(name, sprnames[i]))
                    {
                        if (!sprites_bm)
                        {
                            sprites_bm = hashmap_init(128, sizeof(int));
                        }
                        hashmap_put(sprites_bm, i, &idx);
                        break;
                    }
                }
            }
            free(name);
        }
        else if (!strcasecmp("FLAT", SC_GetString(s)))
        {
            if (!SC_CheckRawString(s))
            {
                SC_Error(s, "expected lump name");
            }
            char *name = M_StringDuplicate(SC_GetString(s));
            int idx = ParseProperty(s);
            if (idx >= 0)
            {
                int num = R_FlatNumForName(name);
                if (num >= 0)
                {
                    if (!flats_bm)
                    {
                        flats_bm = hashmap_init(64, sizeof(int));
                    }
                    hashmap_put(flats_bm, num, &idx);
                }
            }
            free(name);
        }
        else if (!strcasecmp("STATE", SC_GetString(s)))
        {
            SC_MustGetToken(s, TK_IntConst);
            int num = SC_GetNumber(s);
            if (num < 0 || num >= num_states)
            {
                SC_Error(s, "state '%d' not found", num);
            }
            SC_MustGetToken(s, TK_Identifier);
            int idx = GetBrightmap(SC_GetString(s));
            if (idx >= 0)
            {
                if (!states_bm)
                {
                    states_bm = hashmap_init(64, sizeof(int));
                }
                hashmap_put(states_bm, num, &idx);
            }
            else
            {
                SC_Error(s, "brightmap '%s' not found", SC_GetString(s));
            }
        }
    }
    SC_Close(s);

    if (force_brightmaps || array_size(allbrightmaps) == 1)
    {
        MN_DisableBrightmapsItem();
    }
}
