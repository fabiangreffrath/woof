//
//  Copyright (C) 1999 by
//  id Software, Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman
//  Copyright (C) 2006-2025 by
//  The Odamex Team.
//  Copyright (C) 2020 by Ethan Watson
//  Copyright (C) 2025 by
//  Fabian Greffrath, Roman Fomin, Guilherme Miranda
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
//      Here is a core component: drawing the floors and ceilings,
//       while maintaining a per column clipping list only.
//      Moreover, the sky areas have to be determined.
//
// MAXVISPLANES is no longer a limit on the number of visplanes,
// but a limit on the number of hash slots; larger numbers mean
// better performance usually but after a point they are wasted,
// and memory and time overheads creep in.
//
// For more information on visplanes, see:
//
// http://classicgaming.com/doom/editing/
//
// Lee Killough
//
//-----------------------------------------------------------------------------

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "doomdef.h"
#include "doomstat.h"
#include "doomtype.h"
#include "i_system.h"
#include "i_video.h"
#include "m_fixed.h"
#include "r_bmaps.h" // [crispy] R_BrightmapForTexName()
#include "r_data.h"
#include "r_defs.h"
#include "r_draw.h"
#include "r_main.h"
#include "r_plane.h"
#include "r_sky.h"
#include "r_skydefs.h"
#include "r_state.h"
#include "r_tranmap.h"
#include "r_swirl.h" // [crispy] R_DistortedFlat()
#include "tables.h"
#include "v_patch.h"
#include "v_video.h"
#include "w_wad.h"
#include "z_zone.h"

#define MAXVISPLANES 128    /* must be a power of 2 */

static visplane_t *visplanes[MAXVISPLANES];   // killough
static visplane_t *freetail;                  // killough
static visplane_t **freehead = &freetail;     // killough
visplane_t *floorplane, *ceilingplane;

// killough -- hash function for visplanes
// Empirically verified to be fairly uniform:

// added sector tinting, adapted from Doom Retro
#define visplane_hash(picnum, lightlevel, height, tint) \
  (((unsigned)(picnum) * 3 + (unsigned)(lightlevel) + (unsigned)(height) * 7 + (unsigned)(tint) * 11) & (MAXVISPLANES - 1))

// killough 8/1/98: set static number of openings to be large enough
// (a static limit is okay in this case and avoids difficulties in r_segs.c)
int maxopenings;
int *openings, *lastopening; // [FG] 32-bit integer math

// Clip values are the solid pixel bounding the range.
//  floorclip starts out SCREENHEIGHT
//  ceilingclip starts out -1

int *floorclip = NULL, *ceilingclip = NULL; // [FG] 32-bit integer math

// spanstart holds the start of a plane span; initialized to 0 at start

static int *spanstart = NULL;                // killough 2/8/98

//
// texture mapping
//

static fixed_t planeheight;

// killough 2/8/98: make variables static

static fixed_t *cachedheight = NULL;
static fixed_t *cacheddistance = NULL;
static fixed_t *cachedxstep = NULL;
static fixed_t *cachedystep = NULL;
static fixed_t *cachedrotation = NULL;
static fixed_t xoffs,yoffs;    // killough 2/28/98: flat offsets
static angle_t rotation;

static fixed_t angle_sin, angle_cos;
static fixed_t viewx_trans, viewy_trans;

fixed_t *yslope = NULL;

// [FG] linear horizontal sky scrolling
boolean linearsky;
static angle_t *xtoskyangle;

// Hexen-style foreground sky rendering
// uses the 0-index for transparency
static byte *skytran;

//
// R_InitPlanes
// Only at game startup.
//
void R_InitPlanes (void)
{
  xtoskyangle = linearsky ? linearskyangle : xtoviewangle;
  skytran = W_CacheLumpName("SKYTRAN", PU_STATIC);
}

void R_InitPlanesRes(void)
{
  floorclip = Z_Calloc(1, video.width * sizeof(*floorclip), PU_RENDERER, NULL);
  ceilingclip = Z_Calloc(1, video.width * sizeof(*ceilingclip), PU_RENDERER, NULL);
  spanstart = Z_Calloc(1, video.height * sizeof(*spanstart), PU_RENDERER, NULL);

  cachedheight = Z_Calloc(1, video.height * sizeof(*cachedheight), PU_RENDERER, NULL);
  cacheddistance = Z_Calloc(1, video.height * sizeof(*cacheddistance), PU_RENDERER, NULL);
  cachedxstep = Z_Calloc(1, video.height * sizeof(*cachedxstep), PU_RENDERER, NULL);
  cachedystep = Z_Calloc(1, video.height * sizeof(*cachedystep), PU_RENDERER, NULL);
  cachedrotation = Z_Calloc(1, video.height * sizeof(*cachedrotation), PU_RENDERER, NULL);

  yslope = Z_Calloc(1, video.height * sizeof(*yslope), PU_RENDERER, NULL);

  maxopenings = video.width * video.height;
  openings = Z_Calloc(1, maxopenings * sizeof(*openings), PU_RENDERER, NULL);

  R_InitPlanes();
}

void R_InitVisplanesRes(void)
{
  int i;

  freetail = NULL;
  freehead = &freetail;

  for (i = 0; i < MAXVISPLANES; i++)
  {
    visplanes[i] = 0;
  }
}

//
// R_MapPlane
//
// Uses global vars:
//  planeheight
//  ds_source
//  viewx
//  viewy
//  xoffs
//  yoffs
//
// BASIC PRIMITIVE
//

static void R_MapPlane(int y, int x1, int x2, lighttable_t *thiscolormap)
{
  fixed_t distance;
  unsigned lookup;
  int lightindex;
  int dx;
  fixed_t dy;

#ifdef RANGECHECK
  if (x2 < x1 || x1<0 || x2>=viewwidth || (unsigned)y>viewheight)
    I_Error ("%i, %i at %i",x1,x2,y);
#endif

  // [FG] calculate flat coordinates relative to screen center
  //
  // SoM: because centery is an actual row of pixels (and it isn't really the
  // center row because there are an even number of rows) some corrections need
  // to be made depending on where the row lies relative to the centery row.
  if (centery == y)
    return;
  else if (y < centery)
    dy = (abs(centery - y) << FRACBITS) - FRACUNIT / 2;
  else
    dy = (abs(centery - y) << FRACBITS) + FRACUNIT / 2;

  // plane math updated for accounting flat rotation, thanks to Odamex
  if (planeheight != cachedheight[y] || rotation != cachedrotation[y])
    {
      cachedheight[y] = planeheight;
      cachedrotation[y] = rotation;
      distance = cacheddistance[y] = FixedMul(planeheight, yslope[y]);
      // [FG] avoid right-shifting in FixedMul() followed by left-shifting in FixedDiv()
      ds_xstep = cachedxstep[y] = (fixed_t)((int64_t)angle_sin * planeheight / dy);
      ds_ystep = cachedystep[y] = (fixed_t)((int64_t)angle_cos * planeheight / dy);
    }
  else
    {
      distance = cacheddistance[y];
      ds_xstep = cachedxstep[y];
      ds_ystep = cachedystep[y];
    }

  dx = x1 - centerx;

  // killough 2/28/98: Add offsets
  ds_xfrac = viewx_trans + FixedMul(angle_cos, distance) + dx * ds_xstep;
  ds_yfrac = viewy_trans - FixedMul(angle_sin, distance) + dx * ds_ystep;

  // ID24 per-sector colormaps
  if (fixedcolormapindex)
  {
    ds_colormap[0] = thiscolormap + fixedcolormapindex * 256;
    ds_colormap[1] = (STRICTMODE(brightmaps) || force_brightmaps)
                      ? thiscolormap
                      : ds_colormap[0];
  }
  else
  {
    lookup = distance >> LIGHTZSHIFT;
    lookup = CLAMP(lookup, 0, MAXLIGHTZ - 1);
    lightindex = zlightindex[planezlightindex * MAXLIGHTZ + lookup];
    ds_colormap[0] = thiscolormap + lightindex * 256;
    ds_colormap[1] = (STRICTMODE(brightmaps) || force_brightmaps)
                      ? thiscolormap
                      : ds_colormap[0];
  }

  ds_y = y;
  ds_x1 = x1;
  ds_x2 = x2;

  R_DrawSpan();
}

//
// R_ClearPlanes
// At begining of frame.
//

void R_ClearPlanes(void)
{
  int i;

  // opening / clipping determination
  for (i=0 ; i<viewwidth ; i++)
    floorclip[i] = viewheight, ceilingclip[i] = -1;

  for (i=0;i<MAXVISPLANES;i++)    // new code -- killough
    for (*freehead = visplanes[i], visplanes[i] = NULL; *freehead; )
      freehead = &(*freehead)->next;

  lastopening = openings;

  // texture calculation
  memset(cachedheight, 0, viewheight * sizeof(*cachedheight));
}

// New function, by Lee Killough

static visplane_t *new_visplane(unsigned hash)
{
  visplane_t *check = freetail;
  if (!check)
  {
    const int size = sizeof(*check) + (video.width * 2) * sizeof(*check->top);
    check = Z_Calloc(1, size, PU_VALLOC, NULL);
    check->bottom = &check->top[video.width + 2];
  }
  else
    if (!(freetail = freetail->next))
      freehead = &freetail;
  check->next = visplanes[hash];
  visplanes[hash] = check;
  return check;
}

// cph 2003/04/18 - create duplicate of existing visplane and set initial range

visplane_t *R_DupPlane(const visplane_t *pl, int start, int stop)
{
    unsigned hash = visplane_hash(pl->picnum, pl->lightlevel, pl->height, pl->tint);
    visplane_t *new_pl = new_visplane(hash);

    new_pl->height = pl->height;
    new_pl->picnum = pl->picnum;
    new_pl->lightlevel = pl->lightlevel;
    new_pl->xoffs = pl->xoffs;           // killough 2/28/98
    new_pl->yoffs = pl->yoffs;
    new_pl->rotation = pl->rotation;
    new_pl->minx = start;
    new_pl->maxx = stop;
    new_pl->tint = pl->tint;
    memset(new_pl->top, UCHAR_MAX, video.width * sizeof(*new_pl->top));

    return new_pl;
}

//
// R_FindPlane
//
// killough 2/28/98: Add offsets

visplane_t *R_FindPlane(fixed_t height, int picnum, int lightlevel,
                        fixed_t xoffs, fixed_t yoffs, angle_t rotation,
                        int tint)
{
  visplane_t *check;
  unsigned hash;                      // killough

  if (picnum == NO_TEXTURE)
  {
    lightlevel = 255;
  }
  else if (picnum == skyflatnum || picnum & PL_SKYFLAT)  // killough 10/98
  {
    lightlevel = 0;   // killough 7/19/98: most skies map together

    // haleyjd 05/06/08: but not all. If height > viewpoint.z, set height to 1
    // instead of 0, to keep ceilings mapping with ceilings, and floors mapping
    // with floors.
    if (height > viewz)
      height = 1;
    else
      height = 0;
  }

  // New visplane algorithm uses hash table -- killough
  hash = visplane_hash(picnum,lightlevel,height,tint);

  for (check=visplanes[hash]; check; check=check->next)  // killough
    if (height == check->height &&
        picnum == check->picnum &&
        lightlevel == check->lightlevel &&
        xoffs == check->xoffs &&      // killough 2/28/98: Add offset checks
        yoffs == check->yoffs &&
        rotation == check->rotation &&
        tint == check->tint)
      return check;

  check = new_visplane(hash);         // killough

  check->height = height;
  check->picnum = picnum;
  check->lightlevel = lightlevel;
  check->minx = viewwidth;            // Was SCREENWIDTH -- killough 11/98
  check->maxx = -1;
  check->xoffs = xoffs;               // killough 2/28/98: Save offsets
  check->yoffs = yoffs;
  check->rotation = rotation;
  check->tint = tint;

  memset(check->top, UCHAR_MAX, video.width * sizeof(*check->top));

  return check;
}

//
// R_CheckPlane
//
visplane_t *R_CheckPlane(visplane_t *pl, int start, int stop)
{
  int intrl, intrh, unionl, unionh, x;

  if (start < pl->minx)
    intrl   = pl->minx, unionl = start;
  else
    unionl  = pl->minx,  intrl = start;

  if (stop  > pl->maxx)
    intrh   = pl->maxx, unionh = stop;
  else
    unionh  = pl->maxx, intrh  = stop;

  for (x=intrl ; x <= intrh && pl->top[x] == USHRT_MAX; x++)
    ;

  if (x > intrh)
    pl->minx = unionl, pl->maxx = unionh;
  else
    pl = R_DupPlane(pl, start, stop);

  return pl;
}

//
// R_MakeSpans
//

// [FG] 32-bit integer math
static void R_MakeSpans(int x, unsigned int t1, unsigned int b1,
                        unsigned int t2, unsigned int b2, lighttable_t *colormap)
{
  for (; t1 < t2 && t1 <= b1; t1++)
    R_MapPlane(t1, spanstart[t1], x-1, colormap);
  for (; b1 > b2 && b1 >= t1; b1--)
    R_MapPlane(b1, spanstart[b1] ,x-1, colormap);
  while (t2 < t1 && t2 <= b2)
    spanstart[t2++] = x;
  while (b2 > b1 && b2 >= t2)
    spanstart[b2--] = x;
}

static void DrawSkyTex(visplane_t *pl, sky_t *sky, skytex_t *skytex)
{
    const side_t * const side = sky->side;
    const int texture = texturetranslation[skytex->texture];
    dc_texheight = textureheight[texture] >> FRACBITS;
    dc_iscale = FixedMul(skyiscale, skytex->scaley);

    fixed_t deltax, deltay;
    if (uncapped && leveltime > oldleveltime)
    {
        deltay = LerpFixed(skytex->prevy, skytex->curry);
        deltax = LerpFixed(skytex->prevx, skytex->currx) << (ANGLETOSKYSHIFT - FRACBITS);
        dc_texturemid = skytex->mid + deltay;

        if (side)
        {
            deltax += LerpFixed(side->oldtextureoffset, side->textureoffset);
            dc_texturemid += side->rowoffset;
        }
    }
    else
    {
        deltay = skytex->curry;
        deltax = skytex->currx << (ANGLETOSKYSHIFT - FRACBITS);
        dc_texturemid = skytex->mid + deltay;

        if (side)
        {
            deltax += side->textureoffset;
            dc_texturemid += side->rowoffset;
        }
    }

    // sidedef-defined skies are stretched here
    if (stretchsky && sky->stretchable && side)
    {
        dc_texturemid = dc_texturemid * dc_texheight / SKYSTRETCH_HEIGHT;
        dc_iscale = dc_iscale * dc_texheight / SKYSTRETCH_HEIGHT;
    }

    angle_t an = viewangle + deltax;

    if (sky->texturemid_tic != leveltime)
    {
        sky->vertically_scrolling = (sky->old_texturemid != dc_texturemid);
        sky->old_texturemid = dc_texturemid;
        sky->texturemid_tic = leveltime;
    }

    if (colfunc != R_DrawTLColumn && !sky->vertically_scrolling && dc_texheight >= 128)
    {
        // Make sure the fade-to-color effect doesn't happen too early
        fixed_t diff = dc_texturemid - SCREENHEIGHT / 2 * FRACUNIT;
        if (diff < 0)
        {
            diff += textureheight[texture];
            diff %= textureheight[texture];
            dc_texturemid = SCREENHEIGHT / 2 * FRACUNIT + diff;
        }
        dc_skycolor = R_GetSkyColor(skytex->texture);
        colfunc = R_DrawSkyColumn;
    }

    for (int x = pl->minx; x <= pl->maxx; x++)
    {
        dc_x = x;
        dc_yl = pl->top[x];
        dc_yh = pl->bottom[x];

        if (dc_yl != USHRT_MAX && dc_yl <= dc_yh)
        {
            int col = (an + xtoskyangle[x]) >> ANGLETOSKYSHIFT;
            col = FixedToInt(col * skytex->scalex);
            dc_source = R_GetColumn(texture, col);
            colfunc();
        }
    }

    colfunc = R_DrawColumn;
}

static void DrawSkyDef(visplane_t *pl, sky_t *sky)
{
    // Sky is always drawn full bright, i.e. colormaps[0] is used.
    // Because of this hack, sky is not affected by INVUL inverse mapping.
    //
    // killough 7/19/98: fix hack to be more realistic:

    if (STRICTMODE_COMP(comp_skymap)
        || !(dc_colormap[0] = dc_colormap[1] = fixedcolormap))
    {
        dc_colormap[0] = dc_colormap[1] = fullcolormap; // killough 3/20/98
    }

    DrawSkyTex(pl, sky, &sky->background);

    if (sky->type == SkyType_WithForeground)
    {
        // Special tranmap to avoid custom render path to render sky
        // transparently. See id24 SKYDEFS spec.
        tranmap = skytran;
        colfunc = R_DrawTLColumn;
        DrawSkyTex(pl, sky, &sky->foreground);
        tranmap = main_tranmap;
        colfunc = R_DrawColumn;
    }
}

// New function, by Lee Killough

static void do_draw_plane(visplane_t *pl)
{
    if (pl->minx > pl->maxx)
    {
        return;
    }

    boolean swirling = false;

    if (pl->picnum != NO_TEXTURE)
    {
        // sky flat

        if (pl->picnum == skyflatnum)
        {
            DrawSkyDef(pl, levelskies);
            return;
        }

        if (pl->picnum & PL_SKYFLAT)
        {
            sky_t *const sky = R_GetLevelsky(pl->picnum & ~PL_SKYFLAT);
            DrawSkyDef(pl, sky);
            return;
        }

        // regular flat

        swirling = (flattranslation[pl->picnum] == -1);

        // [crispy] add support for SMMU swirling flats
        if (swirling)
        {
            ds_source = R_DistortedFlat(firstflat + pl->picnum);
            ds_brightmap = R_BrightmapForFlatNum(pl->picnum);
        }
        else
        {
            ds_source = V_CacheFlatNum(firstflat + flattranslation[pl->picnum],
                                       PU_STATIC);
            ds_brightmap = R_BrightmapForFlatNum(flattranslation[pl->picnum]);
        }
    }
    else
    {
        ds_source = R_MissingFlat();
    }

    xoffs = pl->xoffs; // killough 2/28/98: Add offsets
    yoffs = pl->yoffs;
    rotation = pl->rotation;

    // plane math updated for accounting flat rotation, thanks to Odamex
    angle_sin = finesine[(viewangle + rotation) >> ANGLETOFINESHIFT];
    angle_cos = finecosine[(viewangle + rotation) >> ANGLETOFINESHIFT];

    if (pl->rotation == 0)
    {
        viewx_trans = xoffs + viewx;
        viewy_trans = yoffs - viewy;
    }
    else
    {
        const fixed_t sin = finesine[pl->rotation >> ANGLETOFINESHIFT];
        const fixed_t cos = finecosine[pl->rotation >> ANGLETOFINESHIFT];

        viewx_trans = xoffs + FixedMul(viewx, cos) - FixedMul(viewy, sin);
        viewy_trans = yoffs - (FixedMul(viewx, sin) + FixedMul(viewy, cos));
    }

    int stop, light;
    planeheight = abs(pl->height - viewz);
    light = (pl->lightlevel >> LIGHTSEGSHIFT) + extralight;

    if (light >= LIGHTLEVELS)
    {
        light = LIGHTLEVELS - 1;
    }

    if (light < 0)
    {
        light = 0;
    }

    stop = pl->maxx + 1;
    pl->top[pl->minx - 1] = pl->top[stop] = USHRT_MAX;

    planezlightindex = light;
    planezlightoffset = &zlightoffset[light * MAXLIGHTZ];
    lighttable_t *thiscolormap = pl->tint ? colormaps[pl->tint] : fullcolormap;

    for (int x = pl->minx; x <= stop; x++)
    {
        R_MakeSpans(x, pl->top[x - 1], pl->bottom[x - 1], pl->top[x],
                    pl->bottom[x], thiscolormap);
    }

    if (!swirling)
    {
        Z_ChangeTag(ds_source, PU_CACHE);
    }
}

//
// RDrawPlanes
// At the end of each frame.
//

void R_DrawPlanes (void)
{
  visplane_t *pl;
  int i;
  for (i=0;i<MAXVISPLANES;i++)
    for (pl=visplanes[i]; pl; pl=pl->next)
    {
      do_draw_plane(pl);
      rendered_visplanes++;
    }
}

//----------------------------------------------------------------------------
//
// $Log: r_plane.c,v $
// Revision 1.8  1998/05/03  23:09:53  killough
// Fix #includes at the top
//
// Revision 1.7  1998/04/27  01:48:31  killough
// Program beautification
//
// Revision 1.6  1998/03/23  03:38:26  killough
// Use 'fullcolormap' for fully-bright F_SKY1
//
// Revision 1.5  1998/03/02  11:47:13  killough
// Add support for general flats xy offsets
//
// Revision 1.4  1998/02/09  03:16:03  killough
// Change arrays to use MAX height/width
//
// Revision 1.3  1998/02/02  13:28:40  killough
// performance tuning
//
// Revision 1.2  1998/01/26  19:24:45  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:03:03  rand
// Lee's Jan 19 sources
//
//----------------------------------------------------------------------------
