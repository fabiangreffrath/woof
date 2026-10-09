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
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "doomdef.h"
#include "doomstat.h"
#include "doomtype.h"
#include "i_system.h"
#include "i_thread.h"
#include "i_video.h"
#include "m_fixed.h"
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

// [MT] The whole visplane state is thread-local: each render context
// collects planes for its own column slab during BSP traversal and draws
// them right after, so visplanes never cross thread boundaries.
static THREADLOCAL visplane_t *visplanes[MAXVISPLANES];   // killough
static THREADLOCAL visplane_t *freetail;                  // killough
static THREADLOCAL visplane_t **freehead;                 // killough
THREADLOCAL visplane_t *floorplane, *ceilingplane;

// killough -- hash function for visplanes
// Empirically verified to be fairly uniform:

// added sector tinting, adapted from Doom Retro
#define visplane_hash(picnum, lightlevel, height, tint) \
  (((unsigned)(picnum) * 3 + (unsigned)(lightlevel) + (unsigned)(height) * 7 + (unsigned)(tint) * 11) & (MAXVISPLANES - 1))

// killough 8/1/98: set static number of openings to be large enough
// (a static limit is okay in this case and avoids difficulties in r_segs.c)
// [MT] per-thread, malloc-backed (workers must not touch the zone allocator).
THREADLOCAL int maxopenings;
THREADLOCAL int *openings, *lastopening; // [FG] 32-bit integer math

// Clip values are the solid pixel bounding the range.
//  floorclip starts out SCREENHEIGHT
//  ceilingclip starts out -1

THREADLOCAL int *floorclip = NULL, *ceilingclip = NULL; // [FG] 32-bit integer math

//
// texture mapping
//

static THREADLOCAL fixed_t planeheight;

static THREADLOCAL fixed_t xoffs,yoffs;    // killough 2/28/98: flat offsets
static THREADLOCAL angle_t rotation;

static THREADLOCAL fixed_t viewx_trans, viewy_trans;

fixed_t *yslope = NULL;

// [Nugget] Sky projection
skyprojection_t sky_projection;
static angle_t *xtoskyangle;

// Hexen-style foreground sky rendering
// uses the 0-index for transparency
static byte *skytran;

// [MT] per-thread plane buffers, (re)allocated when the video size changes.
static THREADLOCAL int plane_buffers_width = 0;
static THREADLOCAL int plane_buffers_height = 0;
// [MT] visplane top/bottom arrays are allocated for this width.
static THREADLOCAL int visplane_width = 0;

//
// Per-row raster cache. Replaces both the span machinery (spanstart/cached*)
// and R_MapPlane's per-span recomputation: distance and light colormap are
// computed once per output row for a plane, then the per-column rasteriser
// resamples perspective-correctly from them.
//
typedef struct planerow_s
{
  fixed_t distance;
  const lighttable_t *colormap;
} planerow_t;

static THREADLOCAL planerow_t *planeraster = NULL;

// Perspective-correct resampling stride: the exact texture position is
// recomputed every planeleap rows and stepped linearly in between
// (R&R's Span_PolyRaster_Log2_* selection).
static THREADLOCAL int planeleap;
static THREADLOCAL int planeleaplog2;

static void R_FreeVisplanes(void)
{
  for (int i = 0; i < MAXVISPLANES; i++)
  {
    visplane_t *pl = visplanes[i];
    while (pl)
    {
      visplane_t *next = pl->next;
      free(pl);
      pl = next;
    }
    visplanes[i] = NULL;
  }

  visplane_t *pl = freetail;
  while (pl)
  {
    visplane_t *next = pl->next;
    free(pl);
    pl = next;
  }
  freetail = NULL;
  freehead = &freetail;
}

static void R_AllocPlaneBuffers(void)
{
  if (plane_buffers_width == video.width
      && plane_buffers_height == video.height)
  {
    return;
  }

  plane_buffers_width = video.width;
  plane_buffers_height = video.height;

  floorclip = I_Realloc(floorclip, video.width * sizeof(*floorclip));
  ceilingclip = I_Realloc(ceilingclip, video.width * sizeof(*ceilingclip));

  planeraster = I_Realloc(planeraster, video.height * sizeof(*planeraster));

  maxopenings = video.width * video.height;
  openings = I_Realloc(openings, maxopenings * sizeof(*openings));

  memset(openings, 0, maxopenings * sizeof(*openings));

  // R&R spantype selection: clamp(round(log2(height * 0.02)), 2..4),
  // written out with integer thresholds (2^2.5*50 ~= 283, 2^3.5*50 ~= 566).
  if (video.height < 283)
  {
    planeleap = 4;
    planeleaplog2 = 2;
  }
  else if (video.height < 566)
  {
    planeleap = 8;
    planeleaplog2 = 3;
  }
  else
  {
    planeleap = 16;
    planeleaplog2 = 4;
  }
}

//
// R_InitPlanes
// Only at game startup.
//
void R_InitPlanes (void)
{
  // [Nugget] Sky projection
  xtoskyangle = (sky_projection == SKYPROJ_LINEAR) ? linearskyangle : xtoviewangle;
  skytran = W_CacheLumpName("SKYTRAN", PU_STATIC);
}

void R_InitPlanesRes(void)
{
  // [MT] floorclip/ceilingclip/spanstart/cached*/openings moved to per-thread
  // buffers, lazily allocated in R_AllocPlaneBuffers(). Only yslope stays
  // shared: it is rebuilt by R_SetupFreelook() on the main thread before the
  // render contexts are dispatched, then only read.
  yslope = Z_Calloc(video.height, sizeof(*yslope), PU_RENDERER, NULL);

  R_InitPlanes();
}

void R_InitVisplanesRes(void)
{
  int i;

  // [MT] resets the calling (main) thread's visplane pool. Worker threads
  // keep their own pools; they are recycled every frame in R_ClearPlanes()
  // and rebuilt if the video width changes.
  R_FreeVisplanes();

  for (i = 0; i < MAXVISPLANES; i++)
  {
    visplanes[i] = 0;
  }
}

//
// R_PreparePlaneRows
//
// Once per plane: cache the distance and light colormap for every output
// row. Replaces the cachedheight/cacheddistance/cachedxstep/cachedystep
// row caches of the old R_MapPlane(). Same light selection as R_MapPlane
// used (zlight table, fixed colormap override).
//

static void R_PreparePlaneRows(const lighttable_t *const thiscolormap)
{
    for (int y = 0; y < viewheight; y++)
    {
        planerow_t *row = &planeraster[y];
        row->distance = FixedMul(planeheight, yslope[y]);

        if (fixedcolormapoffset)
        {
            row->colormap = thiscolormap + fixedcolormapoffset;
        }
        else
        {
            int index = row->distance >> LIGHTZSHIFT;
            index = MIN(index, MAXLIGHTZ - 1);
            row->colormap = thiscolormap + planezlightoffset[index];
        }
    }
}

//
// R_RasterPlaneColumn
//
// Perspective-correct flat rendering for one screen column. Port of Rum and
// Raisin Doom's R_RasteriseColumnImpl (r_raster.cpp) to 16.16 fixed point and
// Doom's row-major flats. The exact world position of a row is recovered from
// the cached row distance every `planeleap` rows, and stepped linearly between
// recomputations, so the interpolation error is bounded by the leap and
// self-corrects. This replaces the whole span pipeline (R_MakeSpans +
// R_MapPlane + R_DrawSpan): with the transposed framebuffer the visplane
// columns ARE the scanlines.
//

// flat spot: y*64 + x from 6 integer bits of each frac - the same trick
// R_DrawSpan used (SoM).
#define PLANESPOT(xf, yf) ((((yf) >> 10) & 0xFC0) | (((xf) >> 16) & 0x3F))

static void R_RasterPlaneColumn(const byte *source, int x, int top, int bottom)
{
    int count = bottom - top;
    pixel_t *dest = xlookup[x] + rowofs[top]; // rows are contiguous (transposed)
    int row = top;

    const fixed_t scaledist = distscale[x];
    const angle_t angle = viewangle + rotation + xtoviewangle[x];
    const fixed_t anglecos = finecosine[angle >> ANGLETOFINESHIFT];
    const fixed_t anglesin = finesine[angle >> ANGLETOFINESHIFT];

    fixed_t distance = planeraster[row].distance;
    fixed_t length = FixedMul(distance, scaledist);
    fixed_t xfrac = viewx_trans + FixedMul(anglecos, length);
    fixed_t yfrac = viewy_trans - FixedMul(anglesin, length);

    while (count >= planeleap)
    {
        const int nextrow = row + planeleap;
        distance = planeraster[nextrow].distance;
        length = FixedMul(distance, scaledist);
        const fixed_t nextxfrac = viewx_trans + FixedMul(anglecos, length);
        const fixed_t nextyfrac = viewy_trans - FixedMul(anglesin, length);

        const fixed_t xstep = (nextxfrac - xfrac) >> planeleaplog2;
        const fixed_t ystep = (nextyfrac - yfrac) >> planeleaplog2;

        for (int i = 0; i < planeleap; i++)
        {
            *dest++ =
                planeraster[row].colormap[source[PLANESPOT(xfrac, yfrac)]];
            row++;
            xfrac += xstep;
            yfrac += ystep;
        }

        // snap to the exact endpoint: interpolation error does not accumulate
        xfrac = nextxfrac;
        yfrac = nextyfrac;

        count -= planeleap;
    }

    if (count >= 0)
    {
        const int nextrow = row + count;
        distance = planeraster[nextrow].distance;
        length = FixedMul(distance, scaledist);
        const fixed_t nextxfrac = viewx_trans + FixedMul(anglecos, length);
        const fixed_t nextyfrac = viewy_trans - FixedMul(anglesin, length);

        count++;

        const fixed_t xstep = (nextxfrac - xfrac) / count;
        const fixed_t ystep = (nextyfrac - yfrac) / count;

        do
        {
            *dest++ =
                planeraster[row].colormap[source[PLANESPOT(xfrac, yfrac)]];
            row++;
            xfrac += xstep;
            yfrac += ystep;
        } while (row <= nextrow);
    }
}

#undef PLANESPOT

//
// R_ClearPlanes
// At begining of frame.
//

void R_ClearPlanes(void)
{
  int i;

  // [MT] per-thread buffers, sized to the current video mode.
  R_AllocPlaneBuffers();

  if (visplane_width != video.width)
  {
    // visplane top/bottom arrays are sized by video.width; recycle the pool
    // when the resolution changes.
    R_FreeVisplanes();
    visplane_width = video.width;
  }

  if (!freehead)
  {
    freehead = &freetail;
  }

  // opening / clipping determination
  // [MT] only this context's columns; outside of them no spans are drawn.
  for (i = r_context->startcol; i < r_context->endcol; i++)
    floorclip[i] = viewheight, ceilingclip[i] = -1;

  for (i=0;i<MAXVISPLANES;i++)    // new code -- killough
    for (*freehead = visplanes[i], visplanes[i] = NULL; *freehead; )
      freehead = &(*freehead)->next;

  lastopening = openings;

  // texture calculation happens per row in R_PreparePlaneRows() now.
}

// New function, by Lee Killough

static visplane_t *new_visplane(unsigned hash)
{
  visplane_t *check = freetail;
  if (!check)
  {
    const int size = sizeof(*check) + (video.width * 2) * sizeof(*check->top);
    // [MT] plain calloc: worker threads must not use the zone allocator.
    check = calloc(1, size);
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
    if (side && !sky->vertically_scrolling)
    {
        // [MT] the sky_t fields written in this block are shared between
        // render contexts, but every context computes and stores the same
        // values (deterministic on the frame's inputs), so the concurrent
        // stores are benign - same approach as Rum and Raisin Doom.
        // If the sky is scrolled vertically for at least one tic,
        // we mark it as vertically-scrolling permanently
        if (sky->texturemid_tic != leveltime)
        {
            if (sky->old_texturemid != dc_texturemid)
            {
                sky->vertically_scrolling = true;
                sky->stretchable = false;
            }
            else
            {
                sky->texturemid_tic = leveltime;
                sky->old_texturemid = dc_texturemid;
            }
        }

        if (stretchsky && sky->stretchable)
        {
            dc_texturemid = dc_texturemid * dc_texheight / SKYSTRETCH_HEIGHT;
            dc_iscale = dc_iscale * dc_texheight / SKYSTRETCH_HEIGHT;
        }
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

    const angle_t an = viewangle + deltax;

    // [Nugget] Sky projection
    const fixed_t base_iscale = dc_iscale;

    for (int x = pl->minx; x <= pl->maxx; x++)
    {
        dc_x = x;
        dc_yl = pl->top[x];
        dc_yh = pl->bottom[x];

        // [Nugget] Sky projection
        if (sky_projection == SKYPROJ_CYLINDRICAL)
        {
            dc_iscale = FixedMul(base_iscale, finecosine[xtoviewangle[x] >> ANGLETOFINESHIFT]);
        }

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
        || !(dc_colormap = fixedcolormap))
    {
        dc_colormap = fullcolormap; // killough 3/20/98
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
    const byte *source = NULL;

    if (pl->minx > pl->maxx)
    {
        return;
    }

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

        // [crispy] add support for SMMU swirling flats
        if (flattranslation[pl->picnum] == -1)
        {
            source = R_DistortedFlat(firstflat + pl->picnum);
        }
        else
        {
            source = V_CacheFlatNum(firstflat + flattranslation[pl->picnum],
                                    PU_STATIC);
        }
    }
    else
    {
        source = R_MissingFlat();
    }

    xoffs = pl->xoffs; // killough 2/28/98: Add offsets
    yoffs = pl->yoffs;
    rotation = pl->rotation;

    // plane math updated for accounting flat rotation, thanks to Odamex
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

    planeheight = abs(pl->height - viewz);

    int light = (pl->lightlevel >> LIGHTSEGSHIFT) + extralight;
    light = CLAMP(light, 0, LIGHTLEVELS - 1);

    planezlightoffset = zlightoffset[light];

    const lighttable_t * const thiscolormap = (pl->tint >= 0)
                                            ? colormaps[pl->tint]
                                            : fullcolormap;

    // [R&R] once per plane: per-row distance and light cache.
    R_PreparePlaneRows(thiscolormap);

    // [R&R] per-column perspective-correct rasterisation - the span
    // generation pass (R_MakeSpans over pl->top/pl->bottom) is gone.
    for (int x = pl->minx; x <= pl->maxx; x++)
    {
        if (pl->top[x] != USHRT_MAX && pl->top[x] <= pl->bottom[x])
        {
            R_RasterPlaneColumn(source, x, pl->top[x], pl->bottom[x]);
        }
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
      r_context->rendered_visplanes++; // [MT] per-context stat
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
