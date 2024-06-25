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
#include "u_scanner.h"
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

static void ReadColormask(u_scanner_t *s, byte *colormask)
{
    memset(colormask, 0, COLORMASK_SIZE);
    do
    {
        unsigned int color1 = 0, color2 = 0;

        if (U_MustGetInteger(s))
        {
            color1 = s->number;
            if (color1 >= 0 && color1 < COLORMASK_SIZE)
            {
                colormask[color1] = 1;
            }
        }

        if (!U_CheckToken(s, '-'))
        {
            continue;
        }

        if (U_MustGetInteger(s))
        {
            color2 = s->number;
            if (color2 >= 0 && color2 < COLORMASK_SIZE)
            {
                int i;
                for (i = color1 + 1; i <= color2; ++i)
                {
                    colormask[i] = 1;
                }
            }
        }
    } while (U_CheckToken(s, ','));
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

static boolean ParseProperty(u_scanner_t *s, elem_t *elem)
{
    char *name;
    int idx;

    int game = DOOM1AND2;

    U_GetString(s);
    name = M_StringDuplicate(s->string);
    U_MustGetToken(s, TK_Identifier);
    idx = GetBrightmap(s->string);
    if (idx < 0)
    {
        U_Error(s, "brightmap '%s' not found", s->string);
        free(name);
        return false;
    }
    if (U_CheckToken(s, TK_Identifier))
    {
        if (!strcasecmp("DOOM", s->string) || !strcasecmp("DOOM1", s->string))
        {
            game = DOOM1ONLY;
            if (U_CheckToken(s, '|'))
            {
                if (U_MustGetIdentifier(s, "DOOM2"))
                {
                    game = DOOM1AND2;
                }
            }
        }
        else if (!strcasecmp("DOOM2", s->string))
        {
            game = DOOM2ONLY;
        }
        else
        {
            U_Unget(s);
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
    u_scanner_t *s;
    const char *data = W_CacheLumpNum(lumpnum, PU_CACHE);
    int length = W_LumpLength(lumpnum);

    force_brightmaps = W_IsWADLump(lumpnum);

    if (!array_size(brightmaps_array))
    {
        brightmap_t brightmap;
        brightmap.name = "NOBRIGHTMAP";
        memset(brightmap.colormask, 0, COLORMASK_SIZE);
        array_push(brightmaps_array, brightmap);
    }

    s = U_ScanOpen(data, length, "BRGHTMPS");

    while (U_HasTokensLeft(s))
    {
        if (!U_CheckToken(s, TK_Identifier))
        {
            U_GetNextToken(s, true);
            continue;
        }
        if (!strcasecmp("BRIGHTMAP", s->string))
        {
            brightmap_t brightmap;
            U_MustGetToken(s, TK_Identifier);
            brightmap.name = M_StringDuplicate(s->string);
            ReadColormask(s, brightmap.colormask);
            array_push(brightmaps_array, brightmap);
        }
        else if (!strcasecmp("TEXTURE", s->string))
        {
            elem_t elem;
            if (ParseProperty(s, &elem))
            {
                array_push(textures_bm, elem);
            }
        }
        else if (!strcasecmp("SPRITE", s->string))
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
        else if (!strcasecmp("FLAT", s->string))
        {
            elem_t elem;
            if (ParseProperty(s, &elem))
            {
                array_push(flats_bm, elem);
            }
        }
        else if (!strcasecmp("STATE", s->string))
        {
            elem_t elem;
            elem.name = NULL;
            U_MustGetInteger(s);
            elem.num = s->number;
            if (elem.num < 0 || elem.num >= num_states)
            {
                U_Error(s, "state '%d' not found", elem.num);
            }
            U_MustGetToken(s, TK_Identifier);
            elem.idx = GetBrightmap(s->string);
            if (elem.idx >= 0)
            {
                array_push(states_bm, elem);
            }
            else
            {
                U_Error(s, "brightmap '%s' not found", s->string);
            }
        }
    }
    U_ScanClose(s);

    brightmaps_found = (array_size(brightmaps_array) > 1);
}
