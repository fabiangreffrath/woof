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
// DESCRIPTION:
//      All the clipping: columns, horizontal spans, sky columns.
//
//-----------------------------------------------------------------------------
//
// 4/25/98, 5/2/98 killough: reformatted, beautified

#include <limits.h>
#include <string.h>

#include "doomdata.h"
#include "doomstat.h"
#include "doomtype.h"
#include "i_system.h"
#include "m_fixed.h"
#include "r_bmaps.h" // [crispy] brightmaps
#include "r_bsp.h"
#include "r_data.h"
#include "r_defs.h"
#include "r_draw.h"
#include "r_main.h"
#include "r_plane.h"
#include "r_state.h"
#include "r_things.h"
#include "tables.h"
#include "v_video.h"
#include "w_wad.h"
#include "z_zone.h"

// OPTIMIZE: closed two sided lines as single sided

// killough 1/6/98: replaced globals with statics where appropriate

// True if any of the segs textures might be visible.
boolean  segtextured;
boolean  markfloor;      // False if the back side is the same plane.
boolean  markceiling;
static boolean  maskedtexture;
static int      toptexture;
static int      bottomtexture;
static int      midtexture;

angle_t         rw_normalangle; // angle to line origin
int             rw_angle1;
fixed_t         rw_distance;

//
// regular wall
//
int      rw_x;
int      rw_stopx;
static angle_t  rw_centerangle;
static fixed_t  rw_offset;
static fixed_t  rw_scale;
static fixed_t  rw_scalestep;
static fixed_t  rw_midtexturemid;
static fixed_t  rw_toptexturemid;
static fixed_t  rw_bottomtexturemid;
static int      worldtop;
static int      worldbottom;
static int      worldhigh;
static int      worldlow;
static int64_t  pixhigh; // [FG] 64-bit integer math
static int64_t  pixlow; // [FG] 64-bit integer math
static fixed_t  pixhighstep;
static fixed_t  pixlowstep;
static int64_t  topfrac; // [FG] 64-bit integer math
static fixed_t  topstep;
static int64_t  bottomfrac; // [FG] 64-bit integer math
static fixed_t  bottomstep;
static int    *maskedtexturecol; // [FG] 32-bit integer math

//
// R_RenderMaskedSegRange
//

void R_RenderMaskedSegRange(drawseg_t *ds, int x1, int x2)
{
  column_t *col;
  int      lightnum;
  int      texnum;
  sector_t tempsec;      // killough 4/13/98

  // Calculate light table.
  // Use different light tables
  //   for horizontal / vertical / diagonal. Diagonal?

  curline = ds->curline;  // OPTIMIZE: get rid of LIGHTSEGSHIFT globally
  lighttable_t *thiscolormap = curline->sidedef->sector->tint
                             ? colormaps[curline->sidedef->sector->tint]
                             : fullcolormap;

  // killough 4/11/98: draw translucent 2s normal textures

  colfunc = R_DrawColumn;
  if (curline->linedef->tranlump >= 0)
    {
      colfunc = R_DrawTLColumn;
      tranmap = main_tranmap;
      if (curline->linedef->tranlump > 0)
        tranmap = W_CacheLumpNum(curline->linedef->tranlump-1, PU_STATIC);
    }
  // killough 4/11/98: end translucent 2s normal code

  frontsector = curline->frontsector;
  backsector = curline->backsector;

  texnum = texturetranslation[curline->sidedef->midtexture];

  // killough 4/13/98: get correct lightlevel for 2s normal textures
  lightnum = (R_FakeFlat(frontsector, &tempsec, NULL, NULL, false)
              ->lightlevel >> LIGHTSEGSHIFT)+extralight;

  // [crispy] smoother fake contrast
  lightnum += curline->fakecontrast;
#if 0
  if (curline->v1->y == curline->v2->y)
    lightnum--;
  else
    if (curline->v1->x == curline->v2->x)
      lightnum++;
#endif

  walllightindex = fixedcolormapindex ? fixedcolormapindex
                                      : CLAMP(lightnum, 0, LIGHTLEVELS - 1);
  walllightoffset = &scalelightoffset[walllightindex * MAXLIGHTSCALE];

  maskedtexturecol = ds->maskedtexturecol;

  rw_scalestep = ds->scalestep;
  spryscale = ds->scale1 + (x1 - ds->x1)*rw_scalestep;
  mfloorclip = ds->sprbottomclip;
  mceilingclip = ds->sprtopclip;

  // find positioning
  if (curline->linedef->flags & ML_DONTPEGBOTTOM)
    {
      dc_texturemid = frontsector->interpfloorheight > backsector->interpfloorheight
        ? frontsector->interpfloorheight : backsector->interpfloorheight;
      dc_texturemid = dc_texturemid + textureheight[texnum] - viewz;
    }
  else
    {
      dc_texturemid =frontsector->interpceilingheight<backsector->interpceilingheight
        ? frontsector->interpceilingheight : backsector->interpceilingheight;
      dc_texturemid = dc_texturemid - viewz;
    }

  dc_texturemid += curline->sidedef->interprowoffset;

  if (fixedcolormap)
    dc_colormap[0] = dc_colormap[1] = thiscolormap + fixedcolormapindex * 256;

  // draw the columns
  for (dc_x = x1 ; dc_x <= x2 ; dc_x++, spryscale += rw_scalestep)
    if (maskedtexturecol[dc_x] != INT_MAX) // [FG] 32-bit integer math
      {
        if (!fixedcolormap)      // calculate lighting
          {                             // killough 11/98:
            const int index = R_GetLightIndex(spryscale);

            // [crispy] brightmaps for two sided mid-textures
            dc_brightmap = texturebrightmap[texnum];
            dc_colormap[0] = thiscolormap + walllightoffset[index];
            dc_colormap[1] = (STRICTMODE(brightmaps) || force_brightmaps)
                              ? thiscolormap
                              : dc_colormap[0];
          }

        // killough 3/2/98:
        //
        // This calculation used to overflow and cause crashes in Doom:
        //
        // sprtopscreen = centeryfrac - FixedMul(dc_texturemid, spryscale);
        //
        // This code fixes it, by using double-precision intermediate
        // arithmetic and by skipping the drawing of 2s normals whose
        // mapping to screen coordinates is totally out of range:

        {
          int64_t t = ((int64_t) centeryfrac << FRACBITS) -
            (int64_t) dc_texturemid * spryscale;
          if (t + (int64_t) textureheight[texnum] * spryscale < 0 ||
              t > (int64_t) video.height << FRACBITS*2)
            continue;        // skip if the texture is out of screen's range
          sprtopscreen = (int64_t)(t >> FRACBITS); // [FG] 64-bit integer math
        }

        dc_iscale = 0xffffffffu / (unsigned) spryscale;

        // killough 1/25/98: here's where Medusa came in, because
        // it implicitly assumed that the column was all one patch.
        // Originally, Doom did not construct complete columns for
        // multipatched textures, so there were no header or trailer
        // bytes in the column referred to below, which explains
        // the Medusa effect. The fix is to construct true columns
        // when forming multipatched textures (see r_data.c).

        // draw the texture
        col = (column_t *)(R_GetColumnMasked(texnum, maskedtexturecol[dc_x]) - 3);
        R_DrawMaskedColumn (col);
        maskedtexturecol[dc_x] = INT_MAX; // [FG] 32-bit integer math
      }

  // [FG] reset column drawing function
  colfunc = R_DrawColumn;

  // Except for main_tranmap, mark others purgable at this point
  if (curline->linedef->tranlump > 0)
    Z_ChangeTag(tranmap, PU_CACHE); // killough 4/11/98
}

//
// R_RenderSegLoop
// Draws zero, one, or two textures (and possibly a masked texture) for walls.
// Can draw or mark the starting pixel of floor and ceiling textures.
// CALLED: CORE LOOPING ROUTINE.
//

#define HEIGHTBITS 12
#define HEIGHTUNIT (1<<HEIGHTBITS)

// WiggleFix: add this code block near the top of r_segs.c
//
// R_FixWiggle()
// Dynamic wall/texture rescaler, AKA "WiggleHack II"
//  by Kurt "kb1" Baumgardner ("kb") and Andrey "Entryway" Budko
//
//  [kb] When the rendered view is positioned, such that the viewer is
//   looking almost parallel down a wall, the result of the scale
//   calculation in R_ScaleFromGlobalAngle becomes very large. And, the
//   taller the wall, the larger that value becomes. If these large
//   values were used as-is, subsequent calculations would overflow,
//   causing full-screen HOM, and possible program crashes.
//
//  Therefore, vanilla Doom clamps this scale calculation, preventing it
//   from becoming larger than 0x400000 (64*FRACUNIT). This number was
//   chosen carefully, to allow reasonably-tight angles, with reasonably
//   tall sectors to be rendered, within the limits of the fixed-point
//   math system being used. When the scale gets clamped, Doom cannot
//   properly render the wall, causing an undesirable wall-bending
//   effect that I call "floor wiggle". Not a crash, but still ugly.
//
//  Modern source ports offer higher video resolutions, which worsens
//   the issue. And, Doom is simply not adjusted for the taller walls
//   found in many PWADs.
//
//  This code attempts to correct these issues, by dynamically
//   adjusting the fixed-point math, and the maximum scale clamp,
//   on a wall-by-wall basis. This has 2 effects:
//
//  1. Floor wiggle is greatly reduced and/or eliminated.
//  2. Overflow is no longer possible, even in levels with maximum
//     height sectors (65535 is the theoretical height, though Doom
//     cannot handle sectors > 32767 units in height.
//
//  The code is not perfect across all situations. Some floor wiggle can
//   still be seen, and some texture strips may be slightly misaligned in
//   extreme cases. These effects cannot be corrected further, without
//   increasing the precision of various renderer variables, and,
//   possibly, creating a noticable performance penalty.
//

static int max_rwscale = 64 * FRACUNIT;
static int heightbits  = HEIGHTBITS;
static int heightunit  = HEIGHTUNIT;
static int invhgtbits  = 4;

static const struct
{
    int clamp;
    int heightbits;
} scale_values[8] = {
    {2048 * FRACUNIT, 12},
    {1024 * FRACUNIT, 12},
    {1024 * FRACUNIT, 11},
    { 512 * FRACUNIT, 11},
    { 512 * FRACUNIT, 10},
    { 256 * FRACUNIT, 10},
    { 256 * FRACUNIT,  9},
    { 128 * FRACUNIT,  9}
};

void R_FixWiggle (sector_t *sector)
{
    static int lastheight = 0;
    int height = (sector->interpceilingheight - sector->interpfloorheight) >> FRACBITS;

    // disallow negative heights. using 1 forces cache initialization
    if (height < 1)
        height = 1;

    // early out?
    if (height != lastheight)
    {
        lastheight = height;

        // initialize, or handle moving sector
        if (height != sector->cachedheight)
        {
            sector->cachedheight = height;
            sector->scaleindex = 0;
            height >>= 7;

            // calculate adjustment
            while (height >>= 1)
                sector->scaleindex++;
        }

        // fine-tune renderer for this wall
        max_rwscale = scale_values[sector->scaleindex].clamp;
        heightbits  = scale_values[sector->scaleindex].heightbits;
        heightunit  = (1 << heightbits);
        invhgtbits  = FRACBITS - heightbits;
    }
}

static boolean didsolidcol; // True if at least one column was marked solid

static void R_RenderSegLoop(lighttable_t * thiscolormap)
{
  fixed_t  texturecolumn = 0;   // shut up compiler warning

  rendered_segs++;

  for ( ; rw_x < rw_stopx ; rw_x++)
    {
      // mark floor / ceiling areas
      int yh, yl = (int)((topfrac+heightunit-1)>>heightbits); // [FG] 64-bit integer math

      // no space above wall?
      int bottom, top = ceilingclip[rw_x]+1;

      if (yl < top)
        yl = top;

      if (markceiling)
        {
          bottom = yl-1;

          if (bottom >= floorclip[rw_x])
            bottom = floorclip[rw_x]-1;

          if (top <= bottom)
            {
              ceilingplane->top[rw_x] = top;
              ceilingplane->bottom[rw_x] = bottom;
            }
        }

      yh = (int)(bottomfrac>>heightbits); // [FG] 64-bit integer math

      bottom = floorclip[rw_x]-1;
      if (yh > bottom)
        yh = bottom;

      if (markfloor)
        {
          top  = yh < ceilingclip[rw_x] ? ceilingclip[rw_x] : yh;
          if (++top <= bottom)
            {
              floorplane->top[rw_x] = top;
              floorplane->bottom[rw_x] = bottom;
            }
        }

      // texturecolumn and lighting are independent of wall tiers
      if (segtextured)
        {
          const int index = R_GetLightIndex(rw_scale);

          // calculate texture offset
          angle_t angle =(rw_centerangle+xtoviewangle[rw_x])>>ANGLETOFINESHIFT;
          angle &= 0xFFF; // Prevent finetangent overflow.
          texturecolumn = rw_offset-FixedMul(finetangent[angle],rw_distance);
          texturecolumn >>= FRACBITS;

          // calculate lighting
          int colormapindex = fixedcolormapindex;

          if (!fixedcolormapindex)
          {
            colormapindex = walllightindex < NUMCOLORMAPS
                          ? scalelightindex[walllightindex * MAXLIGHTSCALE + index]
                          : walllightindex;
          }
          dc_colormap[0] = thiscolormap + colormapindex * 256;
          dc_colormap[1] = (!fixedcolormap &&
                            (STRICTMODE(brightmaps) || force_brightmaps))
                            ? thiscolormap
                            : dc_colormap[0];
          dc_x = rw_x;
          dc_iscale = 0xffffffffu / (unsigned)rw_scale;
        }

      // draw the wall tiers
      if (midtexture)
        {
          dc_yl = yl;     // single sided line
          dc_yh = yh;
          dc_texturemid = rw_midtexturemid;
          dc_source = R_GetColumn(midtexture, texturecolumn);
          dc_texheight = textureheight[midtexture]>>FRACBITS; // killough
          dc_brightmap = texturebrightmap[midtexture];
          colfunc ();
          ceilingclip[rw_x] = viewheight;
          floorclip[rw_x] = -1;
        }
      else
        {
          // two sided line
          if (toptexture)
            {
              // top wall
              int mid = (int)(pixhigh>>heightbits); // [FG] 64-bit integer math
              pixhigh += pixhighstep;

              if (mid >= floorclip[rw_x])
                mid = floorclip[rw_x]-1;

              if (mid >= yl)
                {
                  dc_yl = yl;
                  dc_yh = mid;
                  dc_texturemid = rw_toptexturemid;
                  dc_source = R_GetColumn(toptexture,texturecolumn);
                  dc_texheight = textureheight[toptexture]>>FRACBITS;//killough
                  dc_brightmap = texturebrightmap[toptexture];
                  colfunc ();
                  ceilingclip[rw_x] = mid;
                }
              else
                ceilingclip[rw_x] = yl-1;
            }
          else  // no top wall
            if (markceiling)
              ceilingclip[rw_x] = yl-1;

          if (bottomtexture)          // bottom wall
            {
              int mid = (int)((pixlow+heightunit-1)>>heightbits); // [FG] 64-bit integer math
              pixlow += pixlowstep;

              // no space above wall?
              if (mid <= ceilingclip[rw_x])
                mid = ceilingclip[rw_x]+1;

              if (mid <= yh)
                {
                  dc_yl = mid;
                  dc_yh = yh;
                  dc_texturemid = rw_bottomtexturemid;
                  dc_source = R_GetColumn(bottomtexture,
                                          texturecolumn);
                  dc_texheight = textureheight[bottomtexture]>>FRACBITS; // killough
                  dc_brightmap = texturebrightmap[bottomtexture];
                  colfunc ();
                  floorclip[rw_x] = mid;
                }
              else
                floorclip[rw_x] = yh+1;
            }
          else        // no bottom wall
            if (markfloor)
              floorclip[rw_x] = yh+1;

          // cph - if we completely blocked further sight through this column,
          // add this info to the solid columns array for r_bsp.c
          if ((markceiling || markfloor) &&
              (floorclip[rw_x] <= ceilingclip[rw_x] + 1))
          {
            solidcol[rw_x] = 1;
            didsolidcol = true;
          }

          // save texturecol for backdrawing of masked mid texture
          if (maskedtexture)
            maskedtexturecol[rw_x] = texturecolumn;
        }

      rw_scale += rw_scalestep;
      topfrac += topstep;
      bottomfrac += bottomstep;
    }
}

// below function is ripped from Crispy
// WiggleFix: move R_ScaleFromGlobalAngle function to r_segs.c,
// above R_StoreWallRange
static fixed_t R_ScaleFromGlobalAngle (angle_t visangle)
{
    angle_t anglea = ANG90 + (visangle - viewangle);
    angle_t angleb = ANG90 + (visangle - rw_normalangle);
    int     den = FixedMul(rw_distance, finesine[anglea >> ANGLETOFINESHIFT]);
    fixed_t num = FixedMul(projection,  finesine[angleb >> ANGLETOFINESHIFT]);
    fixed_t scale;

    if (den > (num >> 16))
    {
        scale = FixedDiv(num, den);

        // [kb] When this evaluates True, the scale is clamped,
        //  and there will be some wiggling.
        if (scale > max_rwscale)
            scale = max_rwscale;
        else if (scale < 256)
            scale = 256;
    }
    else
        scale = max_rwscale;

    return scale;
}

//
// R_StoreWallRange
// A wall segment will be drawn
//  between start and stop pixels (inclusive).
//
void R_StoreWallRange(const int start, const int stop)
{
  // [FG] fix long wall wobble
  int64_t dx, dy, dx1, dy1, dist;
  const uint32_t len = curline->r_length; // [FG] use re-calculated seg lengths

  sector_t *sec = curline->sidedef->sector;
  lighttable_t *thiscolormap = sec->tint ? colormaps[sec->tint] : fullcolormap;

  if (!drawsegs || ds_p == drawsegs+maxdrawsegs) // killough 1/98 -- fix 2s line HOM
    {
      unsigned newmax = maxdrawsegs ? maxdrawsegs*2 : 128; // killough
      drawsegs = Z_Realloc(drawsegs,newmax*sizeof(*drawsegs),PU_STATIC,0);
      ds_p = drawsegs+maxdrawsegs;
      maxdrawsegs = newmax;
    }

#ifdef RANGECHECK
  if (start >=viewwidth || start > stop)
    I_Error ("Bad range: %i to %i", start , stop);
#endif

  sidedef = curline->sidedef;
  linedef = curline->linedef;

  if (!linedef)
    return;

  // mark the segment as visible for auto map
  linedef->flags |= ML_MAPPED;

  // [FG] update automap while playing
  if (automap_on)
    return;

  // calculate rw_distance for scale calculation
  rw_normalangle = curline->r_angle + ANG90; // [FG] use re-calculated seg angles

  // [FG] fix long wall wobble
  // shift right to avoid possibility of int64 overflow in rw_distance calculation
  dx = ((int64_t)curline->v2->r_x - curline->v1->r_x) >> 1;
  dy = ((int64_t)curline->v2->r_y - curline->v1->r_y) >> 1;
  dx1 = ((int64_t)viewx - curline->v1->r_x) >> 1;
  dy1 = ((int64_t)viewy - curline->v1->r_y) >> 1;
  dist = ((dy * dx1 - dx * dy1) / len) << 1;
  rw_distance = (fixed_t)(dist < INT_MIN ? INT_MIN : dist > INT_MAX ? INT_MAX : dist);

  ds_p->x1 = rw_x = start;
  ds_p->x2 = stop;
  ds_p->curline = curline;
  rw_stopx = stop+1;

  ptrdiff_t pos = lastopening - openings;
  size_t need = (rw_stopx - start) * sizeof(*lastopening) + pos;
  if (need > maxopenings)
  {
      return;
  }

  // WiggleFix: add this line, in r_segs.c:R_StoreWallRange,
  // right before calls to R_ScaleFromGlobalAngle
  R_FixWiggle(frontsector);

  // killough 1/6/98, 2/1/98: remove limit on openings
  // killough 8/1/98: Replaced code with a static limit 
  // guaranteed to be big enough

  // calculate scale at both ends and step
  ds_p->scale1 = rw_scale =
    R_ScaleFromGlobalAngle (viewangle + xtoviewangle[start]);

  if (stop > start)
    {
      ds_p->scale2 = R_ScaleFromGlobalAngle (viewangle + xtoviewangle[stop]);
      ds_p->scalestep = rw_scalestep = (ds_p->scale2-rw_scale) / (stop-start);
    }
  else
    ds_p->scale2 = ds_p->scale1;

  // calculate texture boundaries
  //  and decide if floor / ceiling marks are needed
  worldtop = frontsector->interpceilingheight - viewz;
  worldbottom = frontsector->interpfloorheight - viewz;

  midtexture = toptexture = bottomtexture = maskedtexture = 0;
  ds_p->maskedtexturecol = NULL;

  if (!backsector)
    {
      // single sided line
      midtexture = texturetranslation[sidedef->midtexture];

      // a single sided line is terminal, so it must mark ends
      markfloor = markceiling = true;

      if (linedef->flags & ML_DONTPEGBOTTOM)
        {         // bottom of texture at bottom
          fixed_t vtop = frontsector->interpfloorheight +
            textureheight[sidedef->midtexture];
          rw_midtexturemid = vtop - viewz;
        }
      else        // top of texture at top
        rw_midtexturemid = worldtop;

      rw_midtexturemid += sidedef->interprowoffset;

      {      // killough 3/27/98: reduce offset
        fixed_t h = textureheight[sidedef->midtexture];
        if (h & (h-FRACUNIT))
          rw_midtexturemid %= h;
      }

      ds_p->silhouette = SIL_BOTH;
      ds_p->sprtopclip = screenheightarray;
      ds_p->sprbottomclip = negonearray;
      ds_p->bsilheight = INT_MAX;
      ds_p->tsilheight = INT_MIN;
    }
  else      // two sided line
    {
      ds_p->sprtopclip = ds_p->sprbottomclip = NULL;
      ds_p->silhouette = 0;

      if (frontsector->interpfloorheight > backsector->interpfloorheight)
        {
          ds_p->silhouette = SIL_BOTTOM;
          ds_p->bsilheight = frontsector->interpfloorheight;
        }
      else
        if (backsector->interpfloorheight > viewz)
          {
            ds_p->silhouette = SIL_BOTTOM;
            ds_p->bsilheight = INT_MAX;
          }

      if (frontsector->interpceilingheight < backsector->interpceilingheight)
        {
          ds_p->silhouette |= SIL_TOP;
          ds_p->tsilheight = frontsector->interpceilingheight;
        }
      else
        if (backsector->interpceilingheight < viewz)
          {
            ds_p->silhouette |= SIL_TOP;
            ds_p->tsilheight = INT_MIN;
          }

      // killough 1/17/98: this test is required if the fix
      // for the automap bug (r_bsp.c) is used, or else some
      // sprites will be displayed behind closed doors. That
      // fix prevents lines behind closed doors with dropoffs
      // from being displayed on the automap.
      //
      // killough 4/7/98: make doorclosed external variable

      {
        if (doorclosed || backsector->interpceilingheight<=frontsector->interpfloorheight)
          {
            ds_p->sprbottomclip = negonearray;
            ds_p->bsilheight = INT_MAX;
            ds_p->silhouette |= SIL_BOTTOM;
          }
        if (doorclosed || backsector->interpfloorheight>=frontsector->interpceilingheight)
          {                   // killough 1/17/98, 2/8/98
            ds_p->sprtopclip = screenheightarray;
            ds_p->tsilheight = INT_MIN;
            ds_p->silhouette |= SIL_TOP;
          }
      }

      worldhigh = backsector->interpceilingheight - viewz;
      worldlow = backsector->interpfloorheight - viewz;

      // hack to allow height changes in outdoor areas
      if (frontsector->ceilingpic == skyflatnum
          && backsector->ceilingpic == skyflatnum)
        worldtop = worldhigh;

      markfloor = worldlow != worldbottom
        || backsector->floorpic != frontsector->floorpic
        || backsector->lightlevel != frontsector->lightlevel

        // killough 3/7/98: Add checks for (x,y) offsets
        || backsector->interp_floor_xoffs != frontsector->interp_floor_xoffs
        || backsector->interp_floor_yoffs != frontsector->interp_floor_yoffs
        || backsector->floor_rotation != frontsector->floor_rotation

        // killough 4/15/98: prevent 2s normals
        // from bleeding through deep water
        || frontsector->heightsec != -1

        // killough 4/17/98: draw floors if different light levels
        || backsector->floorlightsec != frontsector->floorlightsec

        // hexen flowing water
        || backsector->special != frontsector->special
        || backsector->tint != frontsector->tint
        ;

      markceiling = worldhigh != worldtop
        || backsector->ceilingpic != frontsector->ceilingpic
        || backsector->lightlevel != frontsector->lightlevel

        // killough 3/7/98: Add checks for (x,y) offsets
        || backsector->interp_ceiling_xoffs != frontsector->interp_ceiling_xoffs
        || backsector->interp_ceiling_yoffs != frontsector->interp_ceiling_yoffs
        || backsector->ceiling_rotation != frontsector->ceiling_rotation

        // killough 4/15/98: prevent 2s normals
        // from bleeding through fake ceilings
        || (frontsector->heightsec != -1 &&
            frontsector->ceilingpic!=skyflatnum)

        // killough 4/17/98: draw ceilings if different light levels
        || backsector->ceilinglightsec != frontsector->ceilinglightsec
        || backsector->tint != frontsector->tint
        ;

      if (backsector->interpceilingheight <= frontsector->interpfloorheight
          || backsector->interpfloorheight >= frontsector->interpceilingheight)
        markceiling = markfloor = true;   // closed door

      if (worldhigh < worldtop)   // top texture
        {
          toptexture = texturetranslation[sidedef->toptexture];
          rw_toptexturemid = linedef->flags & ML_DONTPEGTOP ? worldtop :
            backsector->interpceilingheight+textureheight[sidedef->toptexture]-viewz;
        }

      if (worldlow > worldbottom) // bottom texture
        {
          bottomtexture = texturetranslation[sidedef->bottomtexture];
          rw_bottomtexturemid = linedef->flags & ML_DONTPEGBOTTOM ? worldtop :
            worldlow;
        }
      rw_toptexturemid += sidedef->interprowoffset;

      // killough 3/27/98: reduce offset
      {
        fixed_t h = textureheight[sidedef->toptexture];
        if (h & (h-FRACUNIT))
          rw_toptexturemid %= h;
      }

      rw_bottomtexturemid += sidedef->interprowoffset;

      // killough 3/27/98: reduce offset
      {
        fixed_t h;
        h = textureheight[sidedef->bottomtexture];
        if (h & (h-FRACUNIT))
          rw_bottomtexturemid %= h;
      }

      // allocate space for masked texture tables
      if (sidedef->midtexture)    // masked midtexture
        {
          maskedtexture = true;
          ds_p->maskedtexturecol = maskedtexturecol = lastopening - rw_x;
          lastopening += rw_stopx - rw_x;
        }
    }

  // calculate rw_offset (only needed for textured lines)
  segtextured = midtexture | toptexture | bottomtexture | maskedtexture;

  if (segtextured)
    {
      // [FG] fix long wall wobble
      rw_offset = (fixed_t)(((dx * dx1 + dy * dy1) / len) << 1);
      rw_offset += sidedef->interptextureoffset + curline->offset;

      rw_centerangle = ANG90 + viewangle - rw_normalangle;

      // calculate light table
      //  use different light tables
      //  for horizontal / vertical / diagonal
      // OPTIMIZE: get rid of LIGHTSEGSHIFT globally
      if (!fixedcolormap)
        {
          int lightnum = (frontsector->lightlevel >> LIGHTSEGSHIFT)+extralight;

          // [crispy] smoother fake contrast
          lightnum += curline->fakecontrast;
#if 0
          if (curline->v1->y == curline->v2->y)
            lightnum--;
          else if (curline->v1->x == curline->v2->x)
            lightnum++;
#endif
          if (fixedcolormapindex)
            walllightindex = fixedcolormapindex;
          else
            walllightindex = CLAMP(lightnum, 0, LIGHTLEVELS - 1);

          walllightoffset = &scalelightoffset[walllightindex * MAXLIGHTSCALE];
        }
    }

  // if a floor / ceiling plane is on the wrong side of the view
  // plane, it is definitely invisible and doesn't need to be marked.

  // killough 3/7/98: add deep water check
  if (frontsector->heightsec == -1)
    {
      if (frontsector->interpfloorheight >= viewz)       // above view plane
        markfloor = false;
      if (frontsector->interpceilingheight <= viewz &&
          frontsector->ceilingpic != skyflatnum)   // below view plane
        markceiling = false;
    }

  // calculate incremental stepping values for texture edges
  worldtop >>= invhgtbits;
  worldbottom >>= invhgtbits;

  topstep = -FixedMul (rw_scalestep, worldtop);
  topfrac = ((int64_t)centeryfrac>>invhgtbits) - (((int64_t)worldtop*rw_scale)>>FRACBITS); // [FG] 64-bit integer math

  bottomstep = -FixedMul (rw_scalestep,worldbottom);
  bottomfrac = ((int64_t)centeryfrac>>invhgtbits) - (((int64_t)worldbottom*rw_scale)>>FRACBITS); // [FG] 64-bit integer math

  if (backsector)
    {
      worldhigh >>= invhgtbits;
      worldlow >>= invhgtbits;

      if (worldhigh < worldtop)
        {
          pixhigh = ((int64_t)centeryfrac>>invhgtbits) - (((int64_t)worldhigh*rw_scale)>>FRACBITS); // [FG] 64-bit integer math
          pixhighstep = -FixedMul (rw_scalestep,worldhigh);
        }
      if (worldlow > worldbottom)
        {
          pixlow = ((int64_t)centeryfrac>>invhgtbits) - (((int64_t)worldlow*rw_scale)>>FRACBITS); // [FG] 64-bit integer math
          pixlowstep = -FixedMul (rw_scalestep,worldlow);
        }
    }

  // render it
  if (markceiling)
  {
    if (ceilingplane)   // killough 4/11/98: add NULL ptr checks
      ceilingplane = R_CheckPlane (ceilingplane, rw_x, rw_stopx-1);
    else
      markceiling = 0;
  }

  if (markfloor)
  {
    if (floorplane)     // killough 4/11/98: add NULL ptr checks
      // cph 2003/04/18  - ceilingplane and floorplane might be the same visplane (e.g. if both skies)
      if (markceiling && ceilingplane == floorplane)
      floorplane = R_DupPlane (floorplane, rw_x, rw_stopx-1);
      else
      floorplane = R_CheckPlane (floorplane, rw_x, rw_stopx-1);
    else
      markfloor = 0;
  }

  didsolidcol = false;
  R_RenderSegLoop(thiscolormap);

  // cph - if a column was made solid by this wall, we _must_ save full clipping
  // info
  if (backsector && didsolidcol)
  {
    if (!(ds_p->silhouette & SIL_BOTTOM))
    {
      ds_p->silhouette |= SIL_BOTTOM;
      ds_p->bsilheight = backsector->floorheight;
    }
    if (!(ds_p->silhouette & SIL_TOP))
    {
      ds_p->silhouette |= SIL_TOP;
      ds_p->tsilheight = backsector->ceilingheight;
    }
  }

  // save sprite clipping info
  if ((ds_p->silhouette & SIL_TOP || maskedtexture) && !ds_p->sprtopclip)
    {
      memcpy (lastopening, ceilingclip+start, sizeof(*lastopening)*(rw_stopx-start)); // [FG] 32-bit integer math
      ds_p->sprtopclip = lastopening - start;
      lastopening += rw_stopx - start;
    }
  if ((ds_p->silhouette & SIL_BOTTOM || maskedtexture) && !ds_p->sprbottomclip)
    {
      memcpy (lastopening, floorclip+start, sizeof(*lastopening)*(rw_stopx-start)); // [FG] 32-bit integer math
      ds_p->sprbottomclip = lastopening - start;
      lastopening += rw_stopx - start;
    }
  if (maskedtexture && !(ds_p->silhouette & SIL_TOP))
    {
      ds_p->silhouette |= SIL_TOP;
      ds_p->tsilheight = INT_MIN;
    }
  if (maskedtexture && !(ds_p->silhouette & SIL_BOTTOM))
    {
      ds_p->silhouette |= SIL_BOTTOM;
      ds_p->bsilheight = INT_MAX;
    }
  ds_p++;
}

//----------------------------------------------------------------------------
//
// $Log: r_segs.c,v $
// Revision 1.16  1998/05/03  23:02:01  killough
// Move R_PointToDist from r_main.c, fix #includes
//
// Revision 1.15  1998/04/27  01:48:37  killough
// Program beautification
//
// Revision 1.14  1998/04/17  10:40:31  killough
// Fix 213, 261 (floor/ceiling lighting)
//
// Revision 1.13  1998/04/16  06:24:20  killough
// Prevent 2s sectors from bleeding across deep water or fake floors
//
// Revision 1.12  1998/04/14  08:17:16  killough
// Fix light levels on 2s textures
//
// Revision 1.11  1998/04/12  02:01:41  killough
// Add translucent walls, add insurance against SIGSEGV
//
// Revision 1.10  1998/04/07  06:43:05  killough
// Optimize: use external doorclosed variable
//
// Revision 1.9  1998/03/28  18:04:31  killough
// Reduce texture offsets vertically
//
// Revision 1.8  1998/03/16  12:41:09  killough
// Fix underwater / dual ceiling support
//
// Revision 1.7  1998/03/09  07:30:25  killough
// Add primitive underwater support, fix scrolling flats
//
// Revision 1.6  1998/03/02  11:52:58  killough
// Fix texturemapping overflow, add scrolling walls
//
// Revision 1.5  1998/02/09  03:17:13  killough
// Make closed door clipping more consistent
//
// Revision 1.4  1998/02/02  13:27:02  killough
// fix openings bug
//
// Revision 1.3  1998/01/26  19:24:47  phares
// First rev with no ^Ms
//
// Revision 1.2  1998/01/26  06:10:42  killough
// Discard old Medusa hack -- fixed in r_data.c now
//
// Revision 1.1.1.1  1998/01/19  14:03:03  rand
// Lee's Jan 19 sources
//
//----------------------------------------------------------------------------
