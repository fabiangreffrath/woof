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
#include "v_video.h"
#include "w_wad.h"
#include "z_zone.h"

// [FG] stretch short skies
boolean stretchsky;

//
// sky mapping
//

int skyflatnum;
int skytexture = -1; // [crispy] initialize
int skytexturemid;

sky_t *sky = NULL;

// PSX fire sky, description: https://fabiensanglard.net/doom_fire_psx/

static byte fire_indices[FIRE_WIDTH * FIRE_HEIGHT];

static byte fire_pixels[FIRE_WIDTH * FIRE_HEIGHT];

static void PrepareFirePixels(fire_t *fire)
{
    byte *rover = fire_pixels;
    for (int x = 0; x < FIRE_WIDTH; x++)
    {
        byte *src = fire_indices + x;
        for (int y = 0; y < FIRE_HEIGHT; y++)
        {
            *rover++ = fire->palette[*src];
            src += FIRE_WIDTH;
        }
    }
}

static void SpreadFire(void)
{
    for (int x = 0; x < FIRE_WIDTH; ++x)
    {
        for (int y = 1; y < FIRE_HEIGHT; ++y)
        {
            int src = y * FIRE_WIDTH + x;

            int index = fire_indices[src];

            if (!index)
            {
                fire_indices[src - FIRE_WIDTH] = 0;
            }
            else
            {
                int rand_index = M_Random() & 3;
                int dst = src - rand_index + 1;
                fire_indices[dst - FIRE_WIDTH] = index - (rand_index & 1);
            }
        }
    }
}

static void SetupFire(fire_t *fire)
{
    memset(fire_indices, 0, FIRE_WIDTH * FIRE_HEIGHT);

    int last = array_size(fire->palette) - 1;

    for (int i = 0; i < FIRE_WIDTH; ++i)
    {
        fire_indices[(FIRE_HEIGHT - 1) * FIRE_WIDTH + i] = last;
    }

    for (int i = 0; i < 64; ++i)
    {
        SpreadFire();
    }
    PrepareFirePixels(fire);
}

byte *R_GetFireColumn(int col)
{
    while (col < 0)
    {
        col += FIRE_WIDTH;
    }
    col %= FIRE_WIDTH;
    return &fire_pixels[col * FIRE_HEIGHT];
}

static void InitSky(void)
{
    static skydefs_t *skydefs;

    static boolean run_once = true;
    if (run_once)
    {
        skydefs = R_ParseSkyDefs();
        run_once = false;
    }

    if (!skydefs)
    {
        return;
    }

    flatmap_t *flatmap = NULL;
    array_foreach(flatmap, skydefs->flatmapping)
    {
        int flatnum = R_FlatNumForName(flatmap->flat);
        int skytex = R_TextureNumForName(flatmap->sky);

        for (int i = 0; i < numsectors; i++)
        {
            if (sectors[i].floorpic == flatnum)
            {
                sectors[i].floorpic = skyflatnum;
                sectors[i].floorsky = skytex | PL_FLATMAPPING;
                R_GetSkyColor(skytex);
            }
            if (sectors[i].ceilingpic == flatnum)
            {
                sectors[i].ceilingpic = skyflatnum;
                sectors[i].ceilingsky = skytex | PL_FLATMAPPING;
                R_GetSkyColor(skytex);
            }
        }
    }

    array_foreach(sky, skydefs->skies)
    {
        if (skytexture == sky->skytex.texture)
        {
            if (sky->type == SkyType_Fire)
            {
                SetupFire(&sky->fire);
            }
            return;
        }
    }

    sky = NULL;
}

void R_UpdateSky(void)
{
    if (!sky)
    {
        return;
    }

    if (sky->type == SkyType_Fire)
    {
        fire_t *fire = &sky->fire;

        if (fire->tics_left == 0)
        {
            SpreadFire();
            PrepareFirePixels(fire);
            fire->tics_left = fire->updatetime;
        }

        fire->tics_left--;

        return;
    }

    skytex_t *background = &sky->skytex;
    background->prevx = background->currx;
    background->prevy = background->curry;
    background->currx += background->scrollx;
    background->curry += background->scrolly;

    if (sky->type == SkyType_WithForeground)
    {
        skytex_t *foreground = &sky->foreground;
        foreground->prevx = foreground->currx;
        foreground->prevy = foreground->curry;
        foreground->currx += foreground->scrollx;
        foreground->curry += foreground->scrolly;
    }
}

//
// R_InitSkyMap
// Called whenever the view size changes.
//
void R_InitSkyMap (void)
{
  InitSky();

  // [crispy] initialize
  if (skytexture == -1)
    return;

  // Pre-calculate sky color
  R_GetSkyColor(skytexture);

  // [FG] stretch short skies
  int skyheight = textureheight[skytexture] >> FRACBITS;

  if (stretchsky && skyheight < 200)
    skytexturemid = -28*FRACUNIT;
  else if (skyheight > 200)
    skytexturemid = 200*FRACUNIT;
  else
  skytexturemid = 100*FRACUNIT;
}

static int CompareSkyColors(const void *a, const void *b)
{
  const rgb_t *rgb_a = (const rgb_t *) a;
  const rgb_t *rgb_b = (const rgb_t *) b;

  int red_a = rgb_a->r, grn_a = rgb_a->g, blu_a = rgb_a->b;
  int red_b = rgb_b->r, grn_b = rgb_b->g, blu_b = rgb_b->b;

  int sum_a = red_a*red_a + grn_a*grn_a + blu_a*blu_a;
  int sum_b = red_b*red_b + grn_b*grn_b + blu_b*blu_b;

  return sum_a - sum_b;
}

static byte R_SkyBlendColor(int tex)
{
  byte *pal = W_CacheLumpName("PLAYPAL", PU_STATIC);
  int i, r = 0, g = 0, b = 0;

  const int width = texturewidth[tex];

  rgb_t *colors = Z_Malloc(sizeof(rgb_t)*width, PU_STATIC, 0);

  // [FG] count colors
  for (i = 0; i < width; i++)
  {
    byte *c = R_GetColumn(tex, i);
    colors[i] = (rgb_t) {pal[3 * c[0] + 0], pal[3 * c[0] + 1], pal[3 * c[0] + 2]};
  }

  qsort(colors, width, sizeof(rgb_t), CompareSkyColors);

  r = colors[width/3].r;
  g = colors[width/3].g;
  b = colors[width/3].b;
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
