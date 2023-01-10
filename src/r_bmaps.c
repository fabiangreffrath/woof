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

#include "doomtype.h"
#include "doomstat.h"
#include "r_data.h"
#include "w_wad.h"
#include "m_misc2.h"
#include "u_scanner.h"

int brightmaps;

#define COLORMASK_SIZE 256

static const byte nobrightmap[COLORMASK_SIZE] = { 0 };

const byte *dc_brightmap = nobrightmap;

typedef struct {
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
                colormask[color1] = 1;
        }

        if (!U_CheckToken(s, '-'))
            continue;

        if (U_MustGetInteger(s))
        {
            color2 = s->number;
            if (color2 >= 0 && color2 < COLORMASK_SIZE)
            {
                int i;
                for (i = color1 + 1; i <= color2; ++i)
                    colormask[i] = 1;
            }
        }
    } while (U_CheckToken(s, ','));
}

#define BRIGHTMAPS_INITIAL_SIZE 32
static brightmap_t *brightmaps_array;
static int num_brightmaps;

static void AddBrightmap(brightmap_t *brightmap)
{
    static int size;

    if (num_brightmaps >= size)
    {
        size = (size ? size * 2 : BRIGHTMAPS_INITIAL_SIZE);
        brightmaps_array = I_Realloc(brightmaps_array,
                                     size * sizeof(brightmap_t));
    }

    memcpy(brightmaps_array + num_brightmaps, brightmap, sizeof(brightmap_t));
    num_brightmaps++;
}

typedef struct {
    brightmap_t *brightmap;
    const char *name;
} texture_bm_t;

#define TEXTURES_INITIAL_SIZE 32
static texture_bm_t *textures_bm;
static int num_textures_bm;

static void AddTexture(const char *name, brightmap_t *brightmap)
{
    static int size;
    texture_bm_t texture;

    texture.name = M_StringDuplicate(name);
    texture.brightmap = brightmap;

    if (num_textures_bm >= size)
    {
        size = (size ? size * 2 : TEXTURES_INITIAL_SIZE);
        textures_bm = I_Realloc(textures_bm, size * sizeof(texture_bm_t));
    }

    textures_bm[num_textures_bm++] = texture;
}

typedef struct
{
    brightmap_t *brightmap;
    int type;
} sprite_bm_t;

#define SPRITES_INITIAL_SIZE 32
static sprite_bm_t *sprites_bm;
static int num_sprites_bm;

static void AddSprite(const char *name, brightmap_t *brightmap)
{
    int i;
    static int size;
    sprite_bm_t sprite;

    for (i = 0; i < num_sprites; ++i)
    {
        if (!strcasecmp(name, sprnames[i]))
            break;
    }
    if (i == num_sprites)
        return;

    sprite.type = i;
    sprite.brightmap = brightmap;

    if (num_sprites_bm >= size)
    {
        size = (size ? size * 2 : SPRITES_INITIAL_SIZE);
        sprites_bm = I_Realloc(sprites_bm, size * sizeof(sprite_bm_t));
    }

    sprites_bm[num_sprites_bm++] = sprite;
}

typedef struct
{
    brightmap_t *brightmap;
    const char *name;
    int num;
} flat_bm_t;

#define FLATS_INITIAL_SIZE 32
static flat_bm_t *flats_bm;
static int num_flats_bm;

static void AddFlat(const char *name, brightmap_t *brightmap)
{
    static int size;
    flat_bm_t flat;

    flat.name = M_StringDuplicate(name);
    flat.num = -1;
    flat.brightmap = brightmap;

    if (num_flats_bm >= size)
    {
        size = (size ? size * 2 : SPRITES_INITIAL_SIZE);
        flats_bm = I_Realloc(flats_bm, size * sizeof(flat_bm_t));
    }

    flats_bm[num_flats_bm++] = flat;
}

typedef struct
{
    brightmap_t *brightmap;
    int num;
} state_bm_t;

#define STATES_INITIAL_SIZE 32
static state_bm_t *states_bm;
static int num_states_bm;

static void AddState(int num, brightmap_t *brightmap)
{
    static int size;
    state_bm_t state;

    state.num = num;
    state.brightmap = brightmap;

    if (num_states_bm >= size)
    {
        size = (size ? size * 2 : SPRITES_INITIAL_SIZE);
        states_bm = I_Realloc(states_bm, size * sizeof(state_bm_t));
    }

    states_bm[num_states_bm++] = state;
}

static brightmap_t *GetBrightmap(const char *name)
{
    int i;
    for (i = 0; i < num_brightmaps; ++i)
    {
        if (!strcasecmp(brightmaps_array[i].name, name))
            return &brightmaps_array[i];
    }
    return NULL;
}

enum
{
    DOOM1AND2,
    DOOM1ONLY,
    DOOM2ONLY
};

static void ParseProperty(u_scanner_t *s,
                     void (*AddElem)(const char *name, brightmap_t *brightmap))
{
    int game = DOOM1AND2;
    char *name;
    brightmap_t *brightmap;

    U_MustGetToken(s, TK_Identifier);
    name = M_StringDuplicate(s->string);
    U_MustGetToken(s, TK_Identifier);
    brightmap = GetBrightmap(s->string);
    if (!brightmap)
    {
        free(name);
        U_Error(s, "brightmap '%s' not found", s->string);
    }
    if (U_CheckToken(s, TK_Identifier))
    {
        if (!strcasecmp("DOOM", s->string) || !strcasecmp("DOOM1", s->string))
        {
            game = DOOM1ONLY;
            if (U_CheckToken(s, '|'))
            {
                if (U_MustGetIdentifier(s, "DOOM2"))
                    game = DOOM1AND2;
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
    if ((gamemission == doom && game == DOOM2ONLY) ||
        (gamemission == doom2 && game == DOOM1ONLY))
    {
        free(name);
        return;
    }

    AddElem(name, brightmap);
    free(name);
}

void R_InitFlatBrightmaps(void)
{
    int i;
    for (i = 0; i < num_flats_bm; ++i)
    {
        flats_bm[i].num = R_FlatNumForName(flats_bm[i].name);
    }
}

const byte *R_BrightmapForTexName(const char *texname)
{
    int i;

    for (i = 0; i < num_textures_bm; i++)
    {
        if (!strncasecmp(textures_bm[i].name, texname, 8))
        {
            return textures_bm[i].brightmap->colormask;
        }
    }

    return nobrightmap;
}

const byte *R_BrightmapForSprite(const int type)
{
    if (STRICTMODE(brightmaps))
    {
        int i;
        for (i = 0; i < num_sprites_bm; i++)
        {
            if (sprites_bm[i].type == type)
            {
                return sprites_bm[i].brightmap->colormask;
            }
        }
    }

    return nobrightmap;
}

const byte *R_BrightmapForFlatNum(const int num)
{
    if (STRICTMODE(brightmaps))
    {
        int i;
        for (i = 0; i < num_flats_bm; i++)
        {
            if (flats_bm[i].num == num)
            {
                return flats_bm[i].brightmap->colormask;
            }
        }
    }

    return nobrightmap;
}

const byte *R_BrightmapForState(const int state)
{
    if (STRICTMODE(brightmaps))
    {
        int i;
        for (i = 0; i < num_states_bm; i++)
        {
            if (states_bm[i].num == state)
            {
                return states_bm[i].brightmap->colormask;
            }
        }
    }

    return nobrightmap;
}

void R_ParseBrightmaps(int lumpnum)
{
    u_scanner_t scanner, *s;
    const char *data = W_CacheLumpNum(lumpnum, PU_CACHE);
    int length = W_LumpLength(lumpnum);

    if (!num_brightmaps)
    {
        brightmap_t brightmap;
        brightmap.name = "NOBRIGHTMAP";
        memset(brightmap.colormask, 0, COLORMASK_SIZE);
        AddBrightmap(&brightmap);
    }

    scanner = U_ScanOpen(data, length, "BRGHTMPS");
    s = &scanner;
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
            AddBrightmap(&brightmap);
        }
        else if (!strcasecmp("TEXTURE", s->string))
        {
            ParseProperty(s, AddTexture);
        }
        else if (!strcasecmp("SPRITE", s->string))
        {
            ParseProperty(s, AddSprite);
        }
        else if (!strcasecmp("FLAT", s->string))
        {
            ParseProperty(s, AddFlat);
        }
        else if (!strcasecmp("STATE", s->string))
        {
            int num;
            brightmap_t *brightmap;

            U_MustGetInteger(s);
            num = s->number;
            if (num < 0 || num >= num_states)
            {
                U_Error(s, "state '%d' not found", num);
            }
            U_MustGetToken(s, TK_Identifier);
            brightmap = GetBrightmap(s->string);
            if (brightmap)
            {
                AddState(num, brightmap);
            }
            else
            {
                U_Error(s, "brightmap '%s' not found", s->string);
            }
        }
    }
    U_ScanClose(s);
}
