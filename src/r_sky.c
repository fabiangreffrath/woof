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

#include "doomtype.h"
#include "i_video.h"
#include "m_array.h"
#include "m_fixed.h"
#include "m_random.h"
#include "r_data.h"
#include "r_sky.h"
#include "r_skydefs.h"
#include "r_state.h" // [FG] textureheight[]
#include "w_wad.h"
#include "z_zone.h"

// [FG] stretch short skies
boolean stretchsky;

//
// sky mapping
//

int skyflatnum;

sky_t *levelskies = NULL;
static fixed_t defaultskytexturemid;

// PSX fire sky, description: https://fabiensanglard.net/doom_fire_psx/

static void PrepareFirePixels(fire_t *fire)
{
    byte *rover = fire->pixels;
    for (int x = 0; x < FIRE_WIDTH; x++)
    {
        byte *src = fire->indices + x;
        for (int y = 0; y < FIRE_HEIGHT; y++)
        {
            *rover++ = fire->palette[*src];
            src += FIRE_WIDTH;
        }
    }
}

static void SpreadFire(fire_t *fire)
{
    for (int x = 0; x < FIRE_WIDTH; ++x)
    {
        for (int y = 1; y < FIRE_HEIGHT; ++y)
        {
            int src = y * FIRE_WIDTH + x;

            int index = fire->indices[src];

            if (!index)
            {
                fire->indices[src - FIRE_WIDTH] = 0;
            }
            else
            {
                int rand_index = M_Random() & 3;
                int dst = src - rand_index + 1;
                fire->indices[dst - FIRE_WIDTH] = index - (rand_index & 1);
            }
        }
    }
}

static void SetupFire(fire_t *fire)
{
    fire->indices = Z_Malloc(FIRE_WIDTH * FIRE_HEIGHT, PU_LEVEL, 0);
    fire->pixels = Z_Malloc(FIRE_WIDTH * FIRE_HEIGHT, PU_LEVEL, 0);

    memset(fire->indices, 0, FIRE_WIDTH * FIRE_HEIGHT);

    int last = array_size(fire->palette) - 1;

    for (int i = 0; i < FIRE_WIDTH; ++i)
    {
        fire->indices[(FIRE_HEIGHT - 1) * FIRE_WIDTH + i] = last;
    }

    for (int i = 0; i < 64; ++i)
    {
        SpreadFire(fire);
    }
    PrepareFirePixels(fire);
}

byte *R_GetFireColumn(fire_t *fire, int col)
{
    while (col < 0)
    {
        col += FIRE_WIDTH;
    }
    col %= FIRE_WIDTH;
    return &fire->pixels[col * FIRE_HEIGHT];
}

static skydefs_t *skydefs;

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
                sky->foreground_sky = R_AddLevelsky(sky->foreground.texture);
                sky_t *foregroundsky = R_GetLevelsky(sky->foreground_sky);
                foregroundsky->skytex = sky->foreground;
                foregroundsky->usedefaultmid = false;
            }
        }
    }

    R_UpdateStretchSkies();
}

void UpdateSkyFromLine(sky_t *sky)
{
    skytex_t *const skytex = &sky->skytex;
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
    skytex->scaley = levelskies->skytex.scaley;

    const int skyheight = textureheight[skytex->texture] >> FRACBITS;
    if (stretchsky && skyheight < 200)
    {
        skytex->mid = skytex->mid * skyheight / SKYSTRETCH_HEIGHT;
    }

    if (sky->linked_sky >= 0)
    {
        sky_t *const linked_sky = R_GetLevelsky(sky->linked_sky);
        linked_sky->skytex.mid = skytex->mid;
    }
}

void R_UpdateSkies(void)
{
    sky_t *sky;
    array_foreach(sky, levelskies)
    {
        if (sky->type == SkyType_Fire)
        {
            fire_t *fire = &sky->fire;

            fire->prevx = fire->currx;
            fire->prevy = fire->curry;
            fire->currx += fire->scrollx;
            fire->curry += fire->scrolly;

            if (fire->tics_left == 0)
            {
                SpreadFire(fire);
                PrepareFirePixels(fire);
                fire->tics_left = fire->updatetime;
            }

            fire->tics_left--;
        }
        else if (sky->line)
        {
            UpdateSkyFromLine(sky);
        }
        else if (!sky->usedefaultmid)
        {
            skytex_t *background = &sky->skytex;
            background->prevx = background->currx;
            background->prevy = background->curry;
            background->currx += background->scrollx;
            background->curry += background->scrolly;
        }
    }
}

static void StretchSky(sky_t *sky)
{
    const int skyheight = textureheight[sky->skytex.texture] >> FRACBITS;

    if (sky == levelskies)
    {
        defaultskytexturemid = (stretchsky && skyheight < 200) ? -28 * FRACUNIT
                               : (skyheight > 200)             ? 200 * FRACUNIT
                                                               : 100 * FRACUNIT;
    }

    if (stretchsky && skyheight < 200)
    {
        sky->skytex.mid = defaultskytexturemid * skyheight / SKYSTRETCH_HEIGHT;
        sky->skytex.scaley = FRACUNIT * skyheight / SKYSTRETCH_HEIGHT;
    }
    else
    {
        sky->skytex.mid = defaultskytexturemid;
        sky->skytex.scaley = FRACUNIT;
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
        else if (sky->usedefaultmid)
        {
            StretchSky(sky);
        }
    }
}

void R_ClearLevelskies(void)
{
    array_free(levelskies);
}

skyindex_t AddLevelsky(int texture, line_t *line)
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
        if (sky->skytex.texture == texture && sky->line == line)
        {
            return (skyindex_t)(sky - levelskies);
        }
    }

    if (skydefs)
    {
        array_foreach(sky, skydefs->skies)
        {
            if (sky->skytex.texture == texture)
            {
                memcpy(&new_sky, sky, sizeof(*sky));
                break;
            }
        }
    }

    if (new_sky.type == SkyType_Indetermined)
    {
        new_sky.type = SkyType_Normal;

        new_sky.skytex.texture = texture;
        new_sky.skytex.scalex = FRACUNIT;
        new_sky.skytex.scaley = FRACUNIT;

        new_sky.usedefaultmid = true;
    }

    new_sky.line = line;
    new_sky.linked_sky = -1;

    if (new_sky.line)
    {
        array_foreach(sky, levelskies)
        {
            if (sky->skytex.texture == new_sky.skytex.texture
                && sky->usedefaultmid && new_sky.usedefaultmid)
            {
                new_sky.linked_sky = (skyindex_t)(sky - levelskies);
                break;
            }
        }

        UpdateSkyFromLine(&new_sky);
    }
    else if (new_sky.usedefaultmid)
    {
        StretchSky(&new_sky);
    }

    if (new_sky.type == SkyType_Fire)
    {
        SetupFire(&new_sky.fire);
    }
    else
    {
        R_GetSkyColor(new_sky.skytex.texture);
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
