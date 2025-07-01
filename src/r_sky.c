//
//  Copyright (C) 1999 by
//  id Software, Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//
// DESCRIPTION:
//  Sky rendering. The DOOM sky is a texture map like any
//  wall, wrapping around. A 1024 columns equal 360 degrees.
//  The default sky map is 256 columns and repeats 4 times
//  on a 320 screen?
//
//-----------------------------------------------------------------------------

#include <stdlib.h>
#include <string.h>

#include "doomstat.h"
#include "doomtype.h"
#include "i_video.h"
#include "m_array.h"
#include "m_fixed.h"
#include "m_random.h"
#include "r_data.h"
#include "r_sky.h"
#include "r_skydefs.h"
#include "r_state.h"
#include "w_wad.h"
#include "z_zone.h"

// [FG] stretch short skies
boolean stretchsky;

//
// sky mapping
//
int skyflatnum;
sky_t *levelskies = NULL;
static skydefs_t *skydefs;
static fixed_t defaultskytexturemid;

// PSX fire sky, description: https://fabiensanglard.net/doom_fire_psx/
// [EA] done away with original spreadFire, in favor of Odamex's texture-
// replacing implementation.
static void SpreadFire(int src, byte *fire, int width)
{
    const byte pixel = fire[src];
    const int copyloc0 = src - width;
    if (pixel == 0)
    {
        if (copyloc0 >= 0)
        {
            fire[copyloc0] = 0;
        }
    }
    else
    {
        const int rand = M_Random() & 3;

        const int copyloc1 = copyloc0 - rand + 1;
        if (copyloc1 >= 0)
        {
            fire[copyloc1] = pixel - (rand & 1);
        }
    }
}

static void UpdateFireSky(sky_t *sky)
{
    const int texnum = sky->background.texture;
    int height = textures[texnum]->height;
    int width = textures[texnum]->width;
    byte *coldata;

    for (int x = 0; x < width; x++)
    {
        for (int y = 1; y < height; y++)
        {
            int src = y * width + x;
            SpreadFire(src, sky->fire, width);
        }
    }

    for (int x = 0; x < width; x++)
    {
        coldata = R_GetColumn(texnum, x);
        for (int y = 0; y < height; y++)
        {
            int src = y * width + x;
            coldata[y] = sky->palette[sky->fire[src]];
        }
    }

    // [EA] [RF]
    // the fire algorithm affects multiple collums at once, therefore both XY
    // loops _must_ be separated, the following will cause noticeable visual
    // disturbance on the resulting fire effect

    // for (int x = 0; x < width; x++)
    // {
    //     byte *coldata = R_GetColumn(texnum, x);
    //     coldata[0] = sky->palette[sky->fire[x]];
    //     for (int y = 1; y < height; y++)
    //     {
    //         int src = y * width + x;
    //         SpreadFire(src, sky->fire, width);
    //         coldata[y] = sky->palette[sky->fire[src]];
    //     }
    // }
}

void R_InitFireSky(sky_t *sky)
{
    int texnum = sky->background.texture;
    const texture_t *tex = textures[texnum];
    size_t size = tex->width * tex->height;
    sky->fire = Z_Calloc(1, size, PU_STATIC, NULL);

    for (int i = 0; i < tex->width * tex->height; i++)
    {
        sky->fire[i] = 0;
    }

    for (int i = 0; i < tex->width; i++)
    {
        sky->fire[(tex->height - 1) * tex->width + i] = sky->palette_num - 1;
    }

    for (int i = 0; i < 64; i++)
    {
        UpdateFireSky(sky);
    }
}

void R_InitSkyMap(void)
{
    static boolean run_once = true;
    if (run_once)
    {
        skydefs = R_ParseSkyDefs();
        run_once = false;
    }

    if (skydefs)
    {
        flatmap_t *flatmap = NULL;
        array_foreach(flatmap, skydefs->flatmapping)
        {
            int flatnum = R_FlatNumForName(flatmap->flat);
            int skytex = R_TextureNumForName(flatmap->sky);
            skyindex_t skyindex = R_AddLevelsky(skytex);

            for (int i = 0; i < numsectors; i++)
            {
                if (sectors[i].floorpic == flatnum)
                {
                    sectors[i].floorpic = skyflatnum;
                    sectors[i].floorsky = skyindex | PL_SKYFLAT;
                }
                if (sectors[i].ceilingpic == flatnum)
                {
                    sectors[i].ceilingpic = skyflatnum;
                    sectors[i].ceilingsky = skyindex | PL_SKYFLAT;
                }
            }
        }

        sky_t *sky;
        int fg_count = 0;
        array_foreach(sky, levelskies)
        {
            if (sky->type == SkyType_WithForeground)
            {
                fg_count++;
            }
        }

        if (fg_count + array_size(levelskies) >= array_capacity(levelskies))
        {
            array_grow(levelskies, fg_count);
        }

        array_foreach(sky, levelskies)
        {
            if (sky->type == SkyType_WithForeground)
            {
                sky->linked_sky = R_AddLevelsky(sky->foreground.texture);
                sky_t *foregroundsky = R_GetLevelsky(sky->linked_sky);
                foregroundsky->background = sky->foreground;
                foregroundsky->usedefaultmid = false;
            }
        }
    }

    R_UpdateStretchSkies();
}

void UpdateSkyFromLine(sky_t *sky)
{
    skytex_t *const skytex = &sky->background;
    const line_t *const line = sky->line;
    const side_t *const side = &sides[*line->sidenum];

    skytex->texture = texturetranslation[side->toptexture];

    skytex->prevx = skytex->currx;
    skytex->prevy = skytex->curry;
    skytex->currx = side->basetextureoffset >> (ANGLETOSKYSHIFT - FRACBITS);
    skytex->curry = 0; // side->baserowoffset;

    skytex->scrolly = side->baserowoffset - side->oldrowoffset;
    skytex->scalex = (line->special == 272) ? FRACUNIT : -FRACUNIT;

    skytex->mid = side->baserowoffset - 28 * FRACUNIT;
    skytex->scaley = levelskies->background.scaley;

    const int skyheight = textureheight[skytex->texture] >> FRACBITS;
    if (stretchsky && skyheight < 200)
    {
        skytex->mid = skytex->mid * skyheight / SKYSTRETCH_HEIGHT;
    }

    if (sky->linked_sky >= 0)
    {
        sky_t *const linked_sky = R_GetLevelsky(sky->linked_sky);
        linked_sky->background.mid = skytex->mid;
    }
}

void R_UpdateSkies(void)
{
    sky_t *sky;
    array_foreach(sky, levelskies)
    {
        sky->background.prevx = sky->background.currx;
        sky->background.prevy = sky->background.curry;
        sky->background.currx += sky->background.scrollx;
        sky->background.curry += sky->background.scrolly;

        sky->foreground.prevx = sky->foreground.currx;
        sky->foreground.prevy = sky->foreground.curry;
        sky->foreground.currx += sky->foreground.scrollx;
        sky->foreground.curry += sky->foreground.scrolly;

        if (sky->line)
        {
            UpdateSkyFromLine(sky);
        }

        if (sky->type == SkyType_Fire)
        {
            UpdateFireSky(sky);
        }
    }
}

static void StretchSky(sky_t *sky)
{
    const int skyheight = textureheight[sky->background.texture] >> FRACBITS;

    if (sky == levelskies)
    {
        defaultskytexturemid = (stretchsky && skyheight < 200) ? -28 * FRACUNIT
                               : (skyheight > 200)             ? 200 * FRACUNIT
                                                               : 100 * FRACUNIT;
    }

    if (stretchsky && skyheight < 200)
    {
        sky->background.mid =
            defaultskytexturemid * skyheight / SKYSTRETCH_HEIGHT;
        sky->background.scaley = FRACUNIT * skyheight / SKYSTRETCH_HEIGHT;
    }
    else
    {
        sky->background.mid = defaultskytexturemid;
        sky->background.scaley = FRACUNIT;
    }
}

void R_UpdateStretchSkies(void)
{
    sky_t *sky;
    array_foreach(sky, levelskies)
    {
        if (sky->line)
        {
            UpdateSkyFromLine(sky);
        }
        if (sky->usedefaultmid)
        {
            StretchSky(sky);
        }
    }
}

void R_ClearLevelskies(void)
{
    array_free(levelskies);
}

static skyindex_t AddLevelsky(int texture, line_t *line)
{
    sky_t new_sky = {.type = SkyType_Indetermined};
    const skyindex_t index = array_size(levelskies);

    if (line)
    {
        // Sky transferred from first sidedef
        const side_t *const side = *line->sidenum + sides;
        // Texture comes from upper texture of reference sidedef
        texture = texturetranslation[side->toptexture];
    }

    sky_t *sky;
    array_foreach(sky, levelskies)
    {
        if (sky->background.texture == texture && sky->line == line)
        {
            return (skyindex_t)(sky - levelskies);
        }
    }

    if (skydefs)
    {
        array_foreach(sky, skydefs->skies)
        {
            if (sky->background.texture == texture)
            {
                memcpy(&new_sky, sky, sizeof(*sky));
                break;
            }
        }
    }

    if (new_sky.type == SkyType_Indetermined)
    {
        new_sky.type = SkyType_Normal;
        new_sky.background.texture = texture;
        new_sky.background.scalex = FRACUNIT;
        new_sky.background.scaley = FRACUNIT;
        new_sky.usedefaultmid = true;
    }

    new_sky.line = line;
    new_sky.linked_sky = -1;
    if (new_sky.line)
    {
        array_foreach(sky, levelskies)
        {
            if (sky->background.texture == new_sky.background.texture
                && sky->usedefaultmid && new_sky.usedefaultmid)
            {
                new_sky.linked_sky = (skyindex_t)(sky - levelskies);
                break;
            }
        }

        UpdateSkyFromLine(&new_sky);
    }
    if (new_sky.usedefaultmid)
    {
        StretchSky(&new_sky);
    }

    if (new_sky.type == SkyType_Fire)
    {
        R_InitFireSky(&new_sky);
    }
    else
    {
        R_GetSkyColor(new_sky.background.texture);
    }
    array_push(levelskies, new_sky);
    return index;
}

skyindex_t R_AddLevelsky(int texture)
{
    return AddLevelsky(texture, NULL);
}

skyindex_t R_AddLevelskyFromLine(line_t *line)
{
    return AddLevelsky(-1, line);
}

sky_t *R_GetLevelsky(skyindex_t index)
{
#ifdef RANGECHECK
    if (index < 0 || index >= array_size(levelskies))
    {
        I_Error("Invalid sky index %d", index);
    }
#endif
    return &levelskies[index];
}

typedef struct rgb_s
{
    int r;
    int g;
    int b;
} rgb_t;

static int CompareSkyColors(const void *a, const void *b)
{
    const rgb_t *rgb_a = (const rgb_t *)a;
    const rgb_t *rgb_b = (const rgb_t *)b;

    int red_a = rgb_a->r, grn_a = rgb_a->g, blu_a = rgb_a->b;
    int red_b = rgb_b->r, grn_b = rgb_b->g, blu_b = rgb_b->b;

    int sum_a = red_a * red_a + grn_a * grn_a + blu_a * blu_a;
    int sum_b = red_b * red_b + grn_b * grn_b + blu_b * blu_b;

    return sum_a - sum_b;
}

static byte R_SkyBlendColor(int tex)
{
    byte *pal = W_CacheLumpName("PLAYPAL", PU_STATIC);
    int i, r = 0, g = 0, b = 0;

    const int width = texturewidth[tex];

    rgb_t *colors = Z_Malloc(sizeof(rgb_t) * width, PU_STATIC, 0);

    // [FG] count colors
    for (i = 0; i < width; i++)
    {
        byte *c = R_GetColumn(tex, i);
        colors[i] =
            (rgb_t){pal[3 * c[0] + 0], pal[3 * c[0] + 1], pal[3 * c[0] + 2]};
    }

    qsort(colors, width, sizeof(rgb_t), CompareSkyColors);

    r = colors[width / 3].r;
    g = colors[width / 3].g;
    b = colors[width / 3].b;
    Z_Free(colors);

    return I_GetNearestColor(pal, r, g, b);
}

typedef struct skycolor_s
{
    int texturenum;
    byte color;
    struct skycolor_s *next;
} skycolor_t;

// the sky colors hash table
#define NUMSKYCHAINS 13
static skycolor_t *skycolors[NUMSKYCHAINS];
#define skycolorkey(a) ((a) % NUMSKYCHAINS)

byte R_GetSkyColor(int texturenum)
{
    int key;
    skycolor_t *target = NULL;

    key = skycolorkey(texturenum);

    if (skycolors[key])
    {
        // search in chain
        skycolor_t *rover = skycolors[key];

        while (rover)
        {
            if (rover->texturenum == texturenum)
            {
                target = rover;
                break;
            }

            rover = rover->next;
        }
    }

    if (target == NULL)
    {
        target = Z_Malloc(sizeof(skycolor_t), PU_STATIC, 0);

        target->texturenum = texturenum;
        target->color = R_SkyBlendColor(texturenum);

        // use head insertion
        target->next = skycolors[key];
        skycolors[key] = target;
    }

    return target->color;
}

//----------------------------------------------------------------------------
//
// $Log: r_sky.c,v $
// Revision 1.6  1998/05/03  23:01:06  killough
// beautification
//
// Revision 1.5  1998/05/01  14:14:24  killough
// beautification
//
// Revision 1.4  1998/02/05  12:14:31  phares
// removed dummy comment
//
// Revision 1.3  1998/01/26  19:24:49  phares
// First rev with no ^Ms
//
// Revision 1.2  1998/01/19  16:17:59  rand
// Added dummy line to be removed later.
//
// Revision 1.1.1.1  1998/01/19  14:03:07  rand
// Lee's Jan 19 sources
//
//----------------------------------------------------------------------------
