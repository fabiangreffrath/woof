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
//      BSP traversal, handling of LineSegs for rendering.
//
//-----------------------------------------------------------------------------

#include <string.h>

#include "d_player.h"
#include "doomdata.h"
#include "doomstat.h"
#include "doomtype.h"
#include "i_system.h"
#include "i_video.h" // [FG] uncapped
#include "m_bbox.h"
#include "m_fixed.h"
#include "p_mobj.h"
#include "r_defs.h"
#include "r_main.h"
#include "r_plane.h"
#include "r_segs.h"
#include "r_state.h"
#include "r_things.h"
#include "tables.h"
#include "v_video.h"

seg_t     *curline;
side_t    *sidedef;
line_t    *linedef;
sector_t  *frontsector;
sector_t  *backsector;
drawseg_t *ds_p;

// killough 4/7/98: indicates doors closed wrt automap bugfix:
int      doorclosed;

// killough: New code which removes 2s linedef limit
drawseg_t *drawsegs;
unsigned  maxdrawsegs;
// drawseg_t drawsegs[MAXDRAWSEGS];       // old code -- killough

//
// R_ClearDrawSegs
//

void R_ClearDrawSegs(void)
{
  ds_p = drawsegs;
}

//
// ClipWallSegment
// Clips the given range of columns
// and includes it in the new clip list.
//
// 1/11/98 killough: Since a type "short" is sufficient, we
// should use it, since smaller arrays fit better in cache.
//

typedef struct {
  short first, last;      // killough
} cliprange_t;

// 1/11/98: Lee Killough
//
// This fixes many strange venetian blinds crashes, which occurred when a scan
// line had too many "posts" of alternating non-transparent and transparent
// regions. Using a doubly-linked list to represent the posts is one way to
// do it, but it has increased overhead and poor spatial locality, which hurts
// cache performance on modern machines. Since the maximum number of posts
// theoretically possible is a function of screen width, a static limit is
// okay in this case. It used to be 32, which was way too small.
//
// This limit was frequently mistaken for the visplane limit in some Doom
// editing FAQs, where visplanes were said to "double" if a pillar or other
// object split the view's space into two pieces horizontally. That did not
// have anything to do with visplanes, but it had everything to do with these
// clip posts.

// CPhipps -
// R_ClipWallSegment
//
// Replaces the old R_Clip*WallSegment functions. It draws bits of walls in those
// columns which aren't solid, and updates the solidcol[] array appropriately

byte *solidcol = NULL;

static void R_ClipWallSegment(int first, int last, boolean solid)
{
  byte *p;
  while (first < last)
  {
    if (solidcol[first])
    {
      if (!(p = memchr(solidcol+first, 0, last-first)))
        return; // All solid

      first = p - solidcol;
    }
    else
    {
      int to;

      if (!(p = memchr(solidcol+first, 1, last-first)))
        to = last;
      else
        to = p - solidcol;

      R_StoreWallRange(first, to-1);

      if (solid)
        memset(solidcol+first, 1, to-first);

      first = to;
    }
  }
}

//
// R_ClearClipSegs
//

void R_ClearClipSegs (void)
{
  memset(solidcol, 0, video.width);
}

// killough 1/18/98 -- This function is used to fix the automap bug which
// showed lines behind closed doors simply because the door had a dropoff.
//
// It assumes that Doom has already ruled out a door being closed because
// of front-back closure (e.g. front floor is taller than back ceiling).

int R_DoorClosed(void)
{
  return

    // if door is closed because back is shut:
    backsector->interpceilingheight <= backsector->interpfloorheight

    // preserve a kind of transparent door/lift special effect:
    && (backsector->interpceilingheight >= frontsector->interpceilingheight ||
     curline->sidedef->toptexture)

    && (backsector->interpfloorheight <= frontsector->interpfloorheight ||
     curline->sidedef->bottomtexture)

    // properly render skies (consider door "open" if both ceilings are sky):
    && (backsector->ceilingpic !=skyflatnum ||
        frontsector->ceilingpic!=skyflatnum);
}

//
// killough 3/7/98: Hack floor/ceiling heights for deep water etc.
//
// If player's view height is underneath fake floor, lower the
// drawn ceiling to be just under the floor height, and replace
// the drawn floor and ceiling textures, and light level, with
// the control sector's.
//
// Similar for ceiling, only reflected.
//
// killough 4/11/98, 4/13/98: fix bugs, add 'back' parameter
//

sector_t *R_FakeFlat(sector_t *sec, sector_t *tempsec,
                     int *floorlightlevel, int *ceilinglightlevel,
                     boolean back)
{
  if (floorlightlevel)
    *floorlightlevel = sec->floorlightsec == -1 ?
      sec->lightlevel : sectors[sec->floorlightsec].lightlevel;

  if (ceilinglightlevel)
    *ceilinglightlevel = sec->ceilinglightsec == -1 ? // killough 4/11/98
      sec->lightlevel : sectors[sec->ceilinglightsec].lightlevel;

  if (sec->heightsec != -1)
    {
      const sector_t *s = &sectors[sec->heightsec];
      int heightsec = viewplayer->mo->subsector->sector->heightsec;
      int underwater = heightsec!=-1 && viewz<=sectors[heightsec].floorheight;

      // Replace sector being drawn, with a copy to be hacked
      *tempsec = *sec;

      // Replace floor and ceiling height with other sector's heights.
      tempsec->floorheight   = s->floorheight;
      tempsec->ceilingheight = s->ceilingheight;
      tempsec->interpfloorheight   = s->interpfloorheight;
      tempsec->interpceilingheight = s->interpceilingheight;

      // killough 11/98: prevent sudden light changes from non-water sectors:
      if (underwater && (tempsec->  floorheight = sec->floorheight,
			 tempsec->interpfloorheight = sec->interpfloorheight,
			 tempsec->interpceilingheight = s->interpfloorheight-1,
			 tempsec->ceilingheight = s->floorheight-1, !back))
        {                   // head-below-floor hack
          tempsec->floorpic    = s->floorpic;
          tempsec->interp_floor_xoffs = s->interp_floor_xoffs;
          tempsec->interp_floor_yoffs = s->interp_floor_yoffs;
          tempsec->floor_rotation = s->floor_rotation;

          if (underwater)
          {
            if (s->ceilingpic == skyflatnum)
              {
                tempsec->floorheight   = tempsec->ceilingheight+1;
                tempsec->interpfloorheight = tempsec->interpceilingheight+1;
                tempsec->ceilingpic    = tempsec->floorpic;
                tempsec->interp_ceiling_xoffs = tempsec->interp_floor_xoffs;
                tempsec->interp_ceiling_yoffs = tempsec->interp_floor_yoffs;
                tempsec->ceiling_rotation = tempsec->ceiling_rotation;
              }
            else
              {
                tempsec->ceilingpic    = s->ceilingpic;
                tempsec->interp_ceiling_xoffs = s->interp_ceiling_xoffs;
                tempsec->interp_ceiling_yoffs = s->interp_ceiling_yoffs;
                tempsec->ceiling_rotation = s->ceiling_rotation;
              }
          }

          tempsec->lightlevel  = s->lightlevel;

          if (floorlightlevel)
            *floorlightlevel = s->floorlightsec == -1 ? s->lightlevel :
            sectors[s->floorlightsec].lightlevel; // killough 3/16/98

          if (ceilinglightlevel)
            *ceilinglightlevel = s->ceilinglightsec == -1 ? s->lightlevel :
            sectors[s->ceilinglightsec].lightlevel; // killough 4/11/98
        }
      else
        if (heightsec != -1 && viewz >= sectors[heightsec].ceilingheight &&
            sec->ceilingheight > s->ceilingheight)
          {   // Above-ceiling hack
            tempsec->ceilingheight = s->ceilingheight;
            tempsec->floorheight   = s->ceilingheight + 1;
            tempsec->interpceilingheight = s->interpceilingheight;
            tempsec->interpfloorheight   = s->interpceilingheight + 1;

            tempsec->floorpic    = tempsec->ceilingpic    = s->ceilingpic;
            tempsec->interp_floor_xoffs = tempsec->interp_ceiling_xoffs = s->interp_ceiling_xoffs;
            tempsec->interp_floor_yoffs = tempsec->interp_ceiling_yoffs = s->interp_ceiling_yoffs;
            tempsec->floor_rotation = tempsec->ceiling_rotation = s->ceiling_rotation;

            if (s->floorpic != skyflatnum)
              {
                tempsec->ceilingheight = sec->ceilingheight;
                tempsec->interpceilingheight = sec->interpceilingheight;
                tempsec->floorpic      = s->floorpic;
                tempsec->interp_floor_xoffs   = s->interp_floor_xoffs;
                tempsec->interp_floor_yoffs   = s->interp_floor_yoffs;
                tempsec->floor_rotation = s->floor_rotation;
              }

            tempsec->lightlevel  = s->lightlevel;

            if (floorlightlevel)
              *floorlightlevel = s->floorlightsec == -1 ? s->lightlevel :
              sectors[s->floorlightsec].lightlevel; // killough 3/16/98

            if (ceilinglightlevel)
              *ceilinglightlevel = s->ceilinglightsec == -1 ? s->lightlevel :
              sectors[s->ceilinglightsec].lightlevel; // killough 4/11/98
          }
      sec = tempsec;               // Use other sector
    }
  return sec;
}

// [AM] Interpolate the passed sector, if prudent.
static void R_MaybeInterpolateSector(sector_t* sector)
{
    if (uncapped)
    {
        // Interpolate between current and last floor/ceiling position.
        if (sector->floordata && sector->floorheight != sector->oldfloorheight &&
            // Only if we moved the sector last tic.
            sector->oldfloorgametic == gametic - 1)
        {
            sector->interpfloorheight = LerpFixed(sector->oldfloorheight, sector->floorheight);
        }
        else
        {
            sector->interpfloorheight = sector->floorheight;
        }

        if (sector->ceilingdata && sector->ceilingheight != sector->oldceilingheight &&
            // Only if we moved the sector last tic.
            sector->oldceilgametic == gametic - 1)
        {
            sector->interpceilingheight = LerpFixed(sector->oldceilingheight, sector->ceilingheight);
        }
        else
        {
            sector->interpceilingheight = sector->ceilingheight;
        }

        if (sector->old_floor_offs_gametic == gametic - 1)
        {
            sector->interp_floor_xoffs = LerpFixed(sector->old_floor_xoffs, sector->floor_xoffs);
            sector->interp_floor_yoffs = LerpFixed(sector->old_floor_yoffs, sector->floor_yoffs);
        }
        else
        {
            sector->interp_floor_xoffs = sector->floor_xoffs;
            sector->interp_floor_yoffs = sector->floor_yoffs;
        }

        if (sector->old_ceil_offs_gametic == gametic - 1)
        {
            sector->interp_ceiling_xoffs = LerpFixed(sector->old_ceiling_xoffs, sector->ceiling_xoffs);
            sector->interp_ceiling_yoffs = LerpFixed(sector->old_ceiling_yoffs, sector->ceiling_yoffs);
        }
        else
        {
            sector->interp_ceiling_xoffs = sector->ceiling_xoffs;
            sector->interp_ceiling_yoffs = sector->ceiling_yoffs;
        }
    }
    else
    {
        sector->interpfloorheight = sector->floorheight;
        sector->interpceilingheight = sector->ceilingheight;
        sector->interp_floor_xoffs = sector->floor_xoffs;
        sector->interp_floor_yoffs = sector->floor_yoffs;
        sector->interp_ceiling_xoffs = sector->ceiling_xoffs;
        sector->interp_ceiling_yoffs = sector->ceiling_yoffs;
    }
}

static void R_MaybeInterpolateTextureOffsets(side_t *side)
{
    if (uncapped && side->oldgametic == gametic - 1)
    {
        side->interptextureoffset = LerpFixed(side->oldtextureoffset, side->textureoffset);
        side->interprowoffset = LerpFixed(side->oldrowoffset, side->rowoffset);
    }
    else
    {
        side->interptextureoffset = side->textureoffset;
        side->interprowoffset = side->rowoffset;
    }
}

//
// R_AddLine
// Clips the given segment
// and adds any visible pieces to the line list.
//

static void R_AddLine (seg_t *line)
{
  int      x1;
  int      x2;
  angle_t  angle1;
  angle_t  angle2;
  angle_t  span;
  angle_t  tspan;
  static sector_t tempsec;     // killough 3/8/98: ceiling/water hack

  curline = line;

  angle1 = R_PointToAngleCrispy (line->v1->r_x, line->v1->r_y);
  angle2 = R_PointToAngleCrispy (line->v2->r_x, line->v2->r_y);

  // Clip to view edges.
  span = angle1 - angle2;

  // Back side, i.e. backface culling
  if (span >= ANG180)
    return;

  // Global angle needed by segcalc.
  rw_angle1 = angle1;
  angle1 -= viewangle;
  angle2 -= viewangle;

  tspan = angle1 + clipangle;
  if (tspan > 2*clipangle)
    {
      tspan -= 2*clipangle;

      // Totally off the left edge?
      if (tspan >= span)
        return;

      angle1 = clipangle;
    }

  tspan = clipangle - angle2;
  if (tspan > 2*clipangle)
    {
      tspan -= 2*clipangle;

      // Totally off the left edge?
      if (tspan >= span)
        return;
      angle2 = 0 - clipangle;
    }

  // The seg is in the view range,
  // but not necessarily visible.

  angle1 = (angle1+ANG90)>>ANGLETOFINESHIFT;
  angle2 = (angle2+ANG90)>>ANGLETOFINESHIFT;

  // killough 1/31/98: Here is where "slime trails" can SOMETIMES occur:
  x1 = viewangletox[angle1];
  x2 = viewangletox[angle2];

  // Does not cross a pixel?
  if (x1 >= x2)       // killough 1/31/98 -- change == to >= for robustness
    return;

  R_MaybeInterpolateTextureOffsets(line->sidedef);

  backsector = line->backsector;

  // Single sided line?
  if (!backsector)
    goto clipsolid;

  // [AM] Interpolate sector movement before
  //      running clipping tests.  Frontsector
  //      should already be interpolated.
  R_MaybeInterpolateSector(backsector);

  if (backsector->heightsec != -1)
  {
    R_MaybeInterpolateSector(&sectors[backsector->heightsec]);
  }

  // killough 3/8/98, 4/4/98: hack for invisible ceilings / deep water
  backsector = R_FakeFlat(backsector, &tempsec, NULL, NULL, true);

  doorclosed = 0;       // killough 4/16/98

  // Closed door.

  if (backsector->interpceilingheight <= frontsector->interpfloorheight
      || backsector->interpfloorheight >= frontsector->interpceilingheight)
    goto clipsolid;

  // This fixes the automap floor height bug -- killough 1/18/98:
  // killough 4/7/98: optimize: save result in doorclosed for use in r_segs.c
  if ((doorclosed = R_DoorClosed()))
    goto clipsolid;

  // Window.
  if (backsector->interpceilingheight != frontsector->interpceilingheight
      || backsector->interpfloorheight != frontsector->interpfloorheight)
    goto clippass;

  // Reject empty lines used for triggers
  //  and special events.
  // Identical floor and ceiling on both sides,
  // identical light levels on both sides,
  // and no middle texture.
  if (backsector->ceilingpic == frontsector->ceilingpic
      && backsector->floorpic == frontsector->floorpic
      && backsector->lightlevel == frontsector->lightlevel
      && curline->sidedef->midtexture == 0

      // killough 3/7/98: Take flats offsets into account:
      && backsector->interp_floor_xoffs == frontsector->interp_floor_xoffs
      && backsector->interp_floor_yoffs == frontsector->interp_floor_yoffs
      && backsector->interp_ceiling_xoffs == frontsector->interp_ceiling_xoffs
      && backsector->interp_ceiling_yoffs == frontsector->interp_ceiling_yoffs
      && backsector->floor_rotation == frontsector->floor_rotation
      && backsector->ceiling_rotation == frontsector->ceiling_rotation

      // killough 4/16/98: consider altered lighting
      && backsector->floorlightsec == frontsector->floorlightsec
      && backsector->ceilinglightsec == frontsector->ceilinglightsec
      && backsector->tint == frontsector->tint
      )
    return;

clippass:
  R_ClipWallSegment(x1, x2, false);
  return;

clipsolid:
  R_ClipWallSegment(x1, x2, true);
}

//
// R_CheckBBox
// Checks BSP node/subtree bounding box.
// Returns true
//  if some part of the bbox might be visible.
//

static const int checkcoord[12][4] = // killough -- static const
{
  {3,0,2,1},
  {3,0,2,0},
  {3,1,2,0},
  {0},
  {2,0,2,1},
  {0,0,0,0},
  {3,1,3,0},
  {0},
  {2,0,3,1},
  {2,1,3,1},
  {2,1,3,0}
};

static boolean R_CheckBBox(fixed_t *bspcoord) // killough 1/28/98: static
{
  int     boxpos, boxx, boxy;
  fixed_t x1, x2, y1, y2;
  angle_t angle1, angle2, span, tspan;
  int     sx1, sx2;

  // Find the corners of the box
  // that define the edges from current viewpoint.
  boxx = viewx <= bspcoord[BOXLEFT] ? 0 : viewx < bspcoord[BOXRIGHT ] ? 1 : 2;
  boxy = viewy >= bspcoord[BOXTOP ] ? 0 : viewy > bspcoord[BOXBOTTOM] ? 1 : 2;

  boxpos = (boxy<<2)+boxx;
  if (boxpos == 5)
    return true;

  x1 = bspcoord[checkcoord[boxpos][0]];
  y1 = bspcoord[checkcoord[boxpos][1]];
  x2 = bspcoord[checkcoord[boxpos][2]];
  y2 = bspcoord[checkcoord[boxpos][3]];

    // check clip list for an open space
  angle1 = R_PointToAngleCrispy (x1, y1) - viewangle;
  angle2 = R_PointToAngleCrispy (x2, y2) - viewangle;

  span = angle1 - angle2;

  // Sitting on a line?
  if (span >= ANG180)
    return true;

  tspan = angle1 + clipangle;
  if (tspan > 2*clipangle)
    {
      tspan -= 2*clipangle;

      // Totally off the left edge?
      if (tspan >= span)
        return false;

      angle1 = clipangle;
    }

  tspan = clipangle - angle2;
  if (tspan > 2*clipangle)
    {
      tspan -= 2*clipangle;

      // Totally off the left edge?
      if (tspan >= span)
        return false;

      angle2 = 0 - clipangle;
    }

  // Find the first clippost
  //  that touches the source post
  //  (adjacent pixels are touching).
  angle1 = (angle1+ANG90)>>ANGLETOFINESHIFT;
  angle2 = (angle2+ANG90)>>ANGLETOFINESHIFT;
  sx1 = viewangletox[angle1];
  sx2 = viewangletox[angle2];

    // Does not cross a pixel.
  if (sx1 == sx2)
    return false;

  if (!memchr(solidcol+sx1, 0, sx2-sx1))
    return false; // All columns it covers are already solidly covered

  return true;
}

//
// R_Subsector
// Determine floor/ceiling planes.
// Add sprites of things in sector.
// Draw one or more line segments.
//
// killough 1/31/98 -- made static, polished

static void R_Subsector(int num)
{
  int         count;
  seg_t       *line;
  subsector_t *sub;
  sector_t    tempsec;              // killough 3/7/98: deep water hack
  int         floorlightlevel;      // killough 3/16/98: set floor lightlevel
  int         ceilinglightlevel;    // killough 4/11/98
  int         floor_tint = 0;
  int         ceiling_tint = 0;

#ifdef RANGECHECK
  if (num>=numsubsectors)
    I_Error ("ss %i with numss = %i", num, numsubsectors);
#endif

  sub = &subsectors[num];
  frontsector = sub->sector;
  count = sub->numlines;
  line = &segs[sub->firstline];

  // [AM] Interpolate sector movement.  Usually only needed
  //      when you're standing inside the sector.
  R_MaybeInterpolateSector(frontsector);

  if (frontsector->heightsec != -1)
  {
    R_MaybeInterpolateSector(&sectors[frontsector->heightsec]);
  }

  // killough 3/8/98, 4/4/98: Deep water / fake ceiling effect
  frontsector = R_FakeFlat(frontsector, &tempsec, &floorlightlevel,
                           &ceilinglightlevel, false);   // killough 4/11/98

  if (frontsector->floorlightsec >= 0)
  {
    floor_tint = sectors[frontsector->floorlightsec].tint;
  }
  else
  {
    floor_tint = frontsector->tint;
  }

  if (frontsector->ceilinglightsec >= 0)
  {
    ceiling_tint = sectors[frontsector->ceilinglightsec].tint;
  }
  else
  {
    ceiling_tint = frontsector->tint;
  }

  // killough 3/7/98: Add (x,y) offsets to flats, add deep water check
  // killough 3/16/98: add floorlightlevel
  // killough 10/98: add support for skies transferred from sidedefs

  floorplane = frontsector->interpfloorheight < viewz || // killough 3/7/98
    (frontsector->heightsec != -1 &&
     sectors[frontsector->heightsec].ceilingpic == skyflatnum) ?
    R_FindPlane(frontsector->interpfloorheight,
		frontsector->floorpic == skyflatnum &&  // kilough 10/98
		frontsector->floorsky & PL_SKYFLAT ? frontsector->floorsky :
                frontsector->floorpic,
                floorlightlevel,                // killough 3/16/98
                frontsector->interp_floor_xoffs,       // killough 3/7/98
                frontsector->interp_floor_yoffs,
                frontsector->floor_rotation,
                floor_tint
                ) : NULL;

  ceilingplane = frontsector->interpceilingheight > viewz ||
    frontsector->ceilingpic == skyflatnum ||
    (frontsector->heightsec != -1 &&
     sectors[frontsector->heightsec].floorpic == skyflatnum) ?
    R_FindPlane(frontsector->interpceilingheight,     // killough 3/8/98
		frontsector->ceilingpic == skyflatnum &&  // kilough 10/98
		frontsector->ceilingsky & PL_SKYFLAT ? frontsector->ceilingsky :
                frontsector->ceilingpic,
                ceilinglightlevel,              // killough 4/11/98
                frontsector->interp_ceiling_xoffs,     // killough 3/7/98
                frontsector->interp_ceiling_yoffs,
                frontsector->ceiling_rotation,
                ceiling_tint
                ) : NULL;

  // killough 9/18/98: Fix underwater slowdown, by passing real sector 
  // instead of fake one. Improve sprite lighting by basing sprite
  // lightlevels on floor & ceiling lightlevels in the surrounding area.
  //
  // 10/98 killough:
  //
  // NOTE: TeamTNT fixed this bug incorrectly, messing up sprite lighting!!!
  // That is part of the 242 effect!!!  If you simply pass sub->sector to
  // the old code you will not get correct lighting for underwater sprites!!!
  // Either you must pass the fake sector and handle validcount here, on the
  // real sector, or you must account for the lighting in some other way, 
  // like passing it as an argument.

  R_AddSprites(sub->sector, (floorlightlevel+ceilinglightlevel)/2);

  while (count--)
  {
    if (line->linedef)
      R_AddLine(line);
    line++;
  }
}

//
// RenderBSPNode
// Renders all subsectors below a given node,
//  traversing subtree recursively.
// Just call with BSP root.
//
// killough 5/2/98: reformatted, removed tail recursion

void R_RenderBSPNode(int bspnum)
{
  while (!(bspnum & NF_SUBSECTOR))  // Found a subsector?
    {
      node_t *bsp = &nodes[bspnum];

      // Decide which side the view point is on.
      int side = R_PointOnSide(viewx, viewy, bsp);

      // Recursively divide front space.
      R_RenderBSPNode(bsp->children[side]);

      // Possibly divide back space.

      if (!R_CheckBBox(bsp->bbox[side^=1]))
        return;

      bspnum = bsp->children[side];
    }
  R_Subsector(bspnum == -1 ? 0 : bspnum & ~NF_SUBSECTOR);
}

//----------------------------------------------------------------------------
//
// $Log: r_bsp.c,v $
// Revision 1.17  1998/05/03  22:47:33  killough
// beautification
//
// Revision 1.16  1998/04/23  12:19:50  killough
// Testing untabify feature
//
// Revision 1.15  1998/04/17  10:22:22  killough
// Fix 213, 261 (floor/ceiling lighting)
//
// Revision 1.14  1998/04/14  08:15:55  killough
// Fix light levels on 2s textures
//
// Revision 1.13  1998/04/13  09:44:40  killough
// Fix head-over ceiling effects
//
// Revision 1.12  1998/04/12  01:57:18  killough
// Fix deep water effects
//
// Revision 1.11  1998/04/07  06:41:14  killough
// Fix disappearing things, AASHITTY sky wall HOM, remove obsolete HOM detector
//
// Revision 1.10  1998/04/06  04:37:48  killough
// Make deep water / fake ceiling handling more consistent
//
// Revision 1.9  1998/03/28  18:14:27  killough
// Improve underwater support
//
// Revision 1.8  1998/03/16  12:40:11  killough
// Fix underwater effects, floor light levels from other sectors
//
// Revision 1.7  1998/03/09  07:22:41  killough
// Add primitive underwater support
//
// Revision 1.6  1998/03/02  11:50:53  killough
// Add support for scrolling flats
//
// Revision 1.5  1998/02/17  06:21:57  killough
// Change commented-out code to #if'ed out code
//
// Revision 1.4  1998/02/09  03:14:55  killough
// Make HOM detector under control of TNTHOM cheat
//
// Revision 1.3  1998/02/02  13:31:23  killough
// Performance tuning, add HOM detector
//
// Revision 1.2  1998/01/26  19:24:36  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:03:02  rand
// Lee's Jan 19 sources
//
//----------------------------------------------------------------------------
