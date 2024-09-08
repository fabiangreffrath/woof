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

#include "i_video.h"
#include "m_fixed.h"
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
int skytexture = -1; // [crispy] initialize
int skytexturemid;

static skydef_t *skydef;

//
// R_InitSkyMap
// Called whenever the view size changes.
//
void R_InitSkyMap (void)
{
  skydef = R_ParseSkyDefs();
  if (skydef)
  {
      return;
  }

  // [crispy] initialize
  if (skytexture == -1)
    return;

  // Pre-calculate sky color
  R_GetSkyColor(skytexture);

  // [FG] stretch short skies
  int skyheight = textureheight[skytexture] >> FRACBITS;

  if (stretchsky && skyheight < 200)
    skytexturemid = -28*FRACUNIT;
  else if (skyheight >= 200)
    skytexturemid = 200*FRACUNIT;
  else
  skytexturemid = 100*FRACUNIT;
}

typedef struct rgb_s {
    int r;
    int g;
    int b;
} rgb_t;

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
