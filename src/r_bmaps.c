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
#include "m_misc.h"
#include "r_data.h"
#include "m_scanner.h"
#include "w_wad.h"
#include "z_zone.h"

boolean brightmaps;
boolean brightmaps_found;
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

static brightmap_t *brightmaps_array = NULL;

typedef struct
{
    int idx;
    const char *name;
    int num;
} elem_t;

static elem_t *textures_bm = NULL;
static elem_t *flats_bm = NULL;
static elem_t *sprites_bm = NULL;
static elem_t *states_bm = NULL;

static int GetBrightmap(const char *name)
{
    int i;
    for (i = 0; i < array_size(brightmaps_array); ++i)
    {
        if (!strcasecmp(brightmaps_array[i].name, name))
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

static boolean ParseProperty(scanner_t *s, elem_t *elem)
{
    char *name;
    int idx;

    int game = DOOM1AND2;

    SC_GetNextTokenLumpName(s);
    name = M_StringDuplicate(SC_GetString(s));
    SC_MustGetToken(s, TK_Identifier);
    idx = GetBrightmap(SC_GetString(s));
    if (idx < 0)
    {
        SC_Error(s, "brightmap '%s' not found", SC_GetString(s));
        free(name);
        return false;
    }
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
        free(name);
        return false;
    }

    elem->name = name;
    elem->idx = idx;
    return true;
}

void R_InitFlatBrightmaps(void)
{
    int i;

    for (i = 0; i < array_size(flats_bm); ++i)
    {
        flats_bm[i].num = R_FlatNumForName(flats_bm[i].name);
    }
}

const byte *R_BrightmapForTexName(const char *texname)
{
    int i;

    for (i = array_size(textures_bm) - 1; i >= 0; i--)
    {
        if (!strncasecmp(textures_bm[i].name, texname, 8))
        {
            return brightmaps_array[textures_bm[i].idx].colormask;
        }
    }

    return nobrightmap;
}

const byte *R_BrightmapForSprite(const int type)
{
    if (STRICTMODE(brightmaps) || force_brightmaps)
    {
        int i;

        for (i = array_size(sprites_bm) - 1; i >= 0; i--)
        {
            if (sprites_bm[i].num == type)
            {
                return brightmaps_array[sprites_bm[i].idx].colormask;
            }
        }
    }

    return nobrightmap;
}

const byte *R_BrightmapForFlatNum(const int num)
{
    if (STRICTMODE(brightmaps) || force_brightmaps)
    {
        int i;

        for (i = array_size(flats_bm) - 1; i >= 0; i--)
        {
            if (flats_bm[i].num == num)
            {
                return brightmaps_array[flats_bm[i].idx].colormask;
            }
        }
    }

    return nobrightmap;
}

const byte *R_BrightmapForState(const int state)
{
    if (STRICTMODE(brightmaps) || force_brightmaps)
    {
        int i;

        for (i = array_size(states_bm) - 1; i >= 0; i--)
        {
            if (states_bm[i].num == state)
            {
                return brightmaps_array[states_bm[i].idx].colormask;
            }
        }
    }

    return nobrightmap;
}

void R_ParseBrightmaps(int lumpnum)
{
    force_brightmaps = W_IsWADLump(lumpnum);

    if (!array_size(brightmaps_array))
    {
        brightmap_t brightmap;
        brightmap.name = "NOBRIGHTMAP";
        memset(brightmap.colormask, 0, COLORMASK_SIZE);
        array_push(brightmaps_array, brightmap);
    }

    scanner_t *s = SC_Open("BRGHTMPS", W_CacheLumpNum(lumpnum, PU_CACHE),
                           W_LumpLength(lumpnum));

    while (SC_TokensLeft(s))
    {
        if (!SC_CheckToken(s, TK_Identifier))
        {
            SC_GetNextToken(s, true);
            continue;
        }
        if (!strcasecmp("BRIGHTMAP", SC_GetString(s)))
        {
            brightmap_t brightmap;
            SC_MustGetToken(s, TK_Identifier);
            brightmap.name = M_StringDuplicate(SC_GetString(s));
            ReadColormask(s, brightmap.colormask);
            array_push(brightmaps_array, brightmap);
        }
        else if (!strcasecmp("TEXTURE", SC_GetString(s)))
        {
            elem_t elem;
            if (ParseProperty(s, &elem))
            {
                array_push(textures_bm, elem);
            }
        }
        else if (!strcasecmp("SPRITE", SC_GetString(s)))
        {
            elem_t elem;
            if (ParseProperty(s, &elem))
            {
                int i;
                for (i = 0; i < num_sprites; ++i)
                {
                    if (!strcasecmp(elem.name, sprnames[i]))
                    {
                        elem.num = i;
                        array_push(sprites_bm, elem);
                        break;
                    }
                }
            }
        }
        else if (!strcasecmp("FLAT", SC_GetString(s)))
        {
            elem_t elem;
            if (ParseProperty(s, &elem))
            {
                array_push(flats_bm, elem);
            }
        }
        else if (!strcasecmp("STATE", SC_GetString(s)))
        {
            elem_t elem;
            elem.name = NULL;
            SC_MustGetToken(s, TK_IntConst);
            elem.num = SC_GetNumber(s);
            if (elem.num < 0 || elem.num >= num_states)
            {
                SC_Error(s, "state '%d' not found", elem.num);
            }
            SC_MustGetToken(s, TK_Identifier);
            elem.idx = GetBrightmap(SC_GetString(s));
            if (elem.idx >= 0)
            {
                array_push(states_bm, elem);
            }
            else
            {
                SC_Error(s, "brightmap '%s' not found", SC_GetString(s));
            }
        }
    }
    SC_Close(s);

    brightmaps_found = (array_size(brightmaps_array) > 1);
}
