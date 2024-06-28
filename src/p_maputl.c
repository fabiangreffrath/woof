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
//      Movement/collision utility functions,
//      as used by function in p_map.c.
//      BLOCKMAP Iterator functions,
//      and some PIT_* functions to use for iteration.
//
//-----------------------------------------------------------------------------

#include <limits.h>
#include <stdlib.h>

#include "doomdata.h"
#include "doomstat.h"
#include "i_printf.h"
#include "m_bbox.h"
#include "p_map.h"
#include "p_maputl.h"
#include "p_mobj.h"
#include "p_setup.h"
#include "r_defs.h"
#include "r_main.h"
#include "r_state.h"
#include "z_zone.h"

//
// P_AproxDistance
// Gives an estimation of distance (not exact)
//

fixed_t P_AproxDistance(fixed_t dx, fixed_t dy)
{
  dx = abs(dx);
  dy = abs(dy);
  if (dx < dy)
    return dx+dy-(dx>>1);
  return dx+dy-(dy>>1);
}

//
// P_PointOnLineSide
// Returns 0 or 1
//
// killough 5/3/98: reformatted, cleaned up

int P_PointOnLineSide(fixed_t x, fixed_t y, line_t *line)
{
  return
    !line->dx ? x <= line->v1->x ? line->dy > 0 : line->dy < 0 :
    !line->dy ? y <= line->v1->y ? line->dx < 0 : line->dx > 0 :
    FixedMul(y-line->v1->y, line->dx>>FRACBITS) >=
    FixedMul(line->dy>>FRACBITS, x-line->v1->x);
}

//
// P_BoxOnLineSide
// Considers the line to be infinite
// Returns side 0 or 1, -1 if box crosses the line.
//
// killough 5/3/98: reformatted, cleaned up

int P_BoxOnLineSide(fixed_t *tmbox, line_t *ld)
{
  switch (ld->slopetype)
    {
      int p;
    default: // shut up compiler warnings -- killough
    case ST_HORIZONTAL:
      return
      (tmbox[BOXBOTTOM] > ld->v1->y) == (p = tmbox[BOXTOP] > ld->v1->y) ?
        p ^ (ld->dx < 0) : -1;
    case ST_VERTICAL:
      return
        (tmbox[BOXLEFT] < ld->v1->x) == (p = tmbox[BOXRIGHT] < ld->v1->x) ?
        p ^ (ld->dy < 0) : -1;
    case ST_POSITIVE:
      return
        P_PointOnLineSide(tmbox[BOXRIGHT], tmbox[BOXBOTTOM], ld) ==
        (p = P_PointOnLineSide(tmbox[BOXLEFT], tmbox[BOXTOP], ld)) ? p : -1;
    case ST_NEGATIVE:
      return
        (P_PointOnLineSide(tmbox[BOXLEFT], tmbox[BOXBOTTOM], ld)) ==
        (p = P_PointOnLineSide(tmbox[BOXRIGHT], tmbox[BOXTOP], ld)) ? p : -1;
    }
}

//
// P_PointOnDivlineSide
// Returns 0 or 1.
//
// killough 5/3/98: reformatted, cleaned up

int P_PointOnDivlineSide(fixed_t x, fixed_t y, divline_t *line)
{
  return
    !line->dx ? x <= line->x ? line->dy > 0 : line->dy < 0 :
    !line->dy ? y <= line->y ? line->dx < 0 : line->dx > 0 :
    (line->dy^line->dx^(x -= line->x)^(y -= line->y)) < 0 ? (line->dy^x) < 0 :
    FixedMul(y>>8, line->dx>>8) >= FixedMul(line->dy>>8, x>>8);
}

//
// P_MakeDivline
//

void P_MakeDivline(line_t *li, divline_t *dl)
{
  dl->x = li->v1->x;
  dl->y = li->v1->y;
  dl->dx = li->dx;
  dl->dy = li->dy;
}

//
// P_InterceptVector
// Returns the fractional intercept point
// along the first divline.
// This is only called by the addthings
// and addlines traversers.
//
// killough 5/3/98: reformatted, cleaned up

fixed_t P_InterceptVector(divline_t *v2, divline_t *v1)
{
  if (!mbf21)
  {
  fixed_t den = FixedMul(v1->dy>>8, v2->dx) - FixedMul(v1->dx>>8, v2->dy);
  return den ? FixedDiv((FixedMul((v1->x-v2->x)>>8, v1->dy) +
                         FixedMul((v2->y-v1->y)>>8, v1->dx)), den) : 0;
  }
  else
  {
    // cph - no precision/overflow problems
    int64_t den = (int64_t)v1->dy * v2->dx - (int64_t)v1->dx * v2->dy;
    den >>= 16;
    if (!den)
      return 0;
    return (fixed_t)(((int64_t)(v1->x - v2->x) * v1->dy - (int64_t)(v1->y - v2->y) * v1->dx) / den);
  }
}

//
// P_LineOpening
// Sets opentop and openbottom to the window
// through a two sided line.
// OPTIMIZE: keep this precalculated
//

fixed_t opentop;
fixed_t openbottom;
fixed_t openrange;
fixed_t lowfloor;

// moved front and back outside P-LineOpening and changed    // phares 3/7/98
// them to these so we can pick up the new friction value
// in PIT_CheckLine()
sector_t *openfrontsector; // made global                    // phares
sector_t *openbacksector;  // made global

void P_LineOpening(line_t *linedef)
{
  if (linedef->sidenum[1] == NO_INDEX)      // single sided line
    {
      openrange = 0;
      return;
    }

  openfrontsector = linedef->frontsector;
  openbacksector = linedef->backsector;

  if (openfrontsector->ceilingheight < openbacksector->ceilingheight)
    opentop = openfrontsector->ceilingheight;
  else
    opentop = openbacksector->ceilingheight;

  if (openfrontsector->floorheight > openbacksector->floorheight)
    {
      openbottom = openfrontsector->floorheight;
      lowfloor = openbacksector->floorheight;
    }
  else
    {
      openbottom = openbacksector->floorheight;
      lowfloor = openfrontsector->floorheight;
    }
  openrange = opentop - openbottom;
}

//
// THING POSITION SETTING
//

//
// P_UnsetThingPosition
// Unlinks a thing from block map and sectors.
// On each position change, BLOCKMAP and other
// lookups maintaining lists ot things inside
// these structures need to be updated.
//

void P_UnsetThingPosition (mobj_t *thing)
{
  if (!(thing->flags & MF_NOSECTOR))
    {
      // invisible things don't need to be in sector list
      // unlink from subsector

      // killough 8/11/98: simpler scheme using pointers-to-pointers for prev
      // pointers, allows head node pointers to be treated like everything else
      mobj_t **sprev = thing->sprev;
      mobj_t  *snext = thing->snext;
      if ((*sprev = snext))  // unlink from sector list
	snext->sprev = sprev;

      // phares 3/14/98
      //
      // Save the sector list pointed to by touching_sectorlist.
      // In P_SetThingPosition, we'll keep any nodes that represent
      // sectors the Thing still touches. We'll add new ones then, and
      // delete any nodes for sectors the Thing has vacated. Then we'll
      // put it back into touching_sectorlist. It's done this way to
      // avoid a lot of deleting/creating for nodes, when most of the
      // time you just get back what you deleted anyway.
      //
      // If this Thing is being removed entirely, then the calling
      // routine will clear out the nodes in sector_list.
      
      sector_list = thing->touching_sectorlist;
      thing->touching_sectorlist = NULL; //to be restored by P_SetThingPosition
    }

  if (!(thing->flags & MF_NOBLOCKMAP))
    {
      // inert things don't need to be in blockmap
      
      // killough 8/11/98: simpler scheme using pointers-to-pointers for prev
      // pointers, allows head node pointers to be treated like everything else
      //
      // Also more robust, since it doesn't depend on current position for
      // unlinking. Old method required computing head node based on position
      // at time of unlinking, assuming it was the same position as during
      // linking.

      mobj_t *bnext, **bprev = thing->bprev;
      if (bprev && (*bprev = bnext = thing->bnext))  // unlink from block map
	bnext->bprev = bprev;
    }
}

//
// P_SetThingPosition
// Links a thing into both a block and a subsector
// based on it's x y.
// Sets thing->subsector properly
//
// killough 5/3/98: reformatted, cleaned up

void P_SetThingPosition(mobj_t *thing)
{                                                      // link into subsector
  subsector_t *ss = thing->subsector = R_PointInSubsector(thing->x, thing->y);

  if (!(thing->flags & MF_NOSECTOR))
    {
      // invisible things don't go into the sector links

      // killough 8/11/98: simpler scheme using pointer-to-pointer prev
      // pointers, allows head nodes to be treated like everything else

      mobj_t **link = &ss->sector->thinglist;
      mobj_t *snext = *link;
      if ((thing->snext = snext))
	snext->sprev = &thing->snext;
      thing->sprev = link;
      *link = thing;

      // phares 3/16/98
      //
      // If sector_list isn't NULL, it has a collection of sector
      // nodes that were just removed from this Thing.

      // Collect the sectors the object will live in by looking at
      // the existing sector_list and adding new nodes and deleting
      // obsolete ones.

      // When a node is deleted, its sector links (the links starting
      // at sector_t->touching_thinglist) are broken. When a node is
      // added, new sector links are created.

      P_CreateSecNodeList(thing, thing->x, thing->y);
      thing->touching_sectorlist = sector_list; // Attach to Thing's mobj_t
      sector_list = NULL; // clear for next time
    }

  // link into blockmap
  if (!(thing->flags & MF_NOBLOCKMAP))
    {
      // inert things don't need to be in blockmap
      int blockx = (thing->x - bmaporgx)>>MAPBLOCKSHIFT;
      int blocky = (thing->y - bmaporgy)>>MAPBLOCKSHIFT;

      if (blockx>=0 && blockx < bmapwidth && blocky>=0 && blocky < bmapheight)
        {
	  // killough 8/11/98: simpler scheme using pointer-to-pointer prev
	  // pointers, allows head nodes to be treated like everything else

          mobj_t **link = &blocklinks[blocky*bmapwidth+blockx];
	  mobj_t *bnext = *link;
          if ((thing->bnext = bnext))
	    bnext->bprev = &thing->bnext;
	  thing->bprev = link;
          *link = thing;
        }
      else        // thing is off the map
        thing->bnext = NULL, thing->bprev = NULL;
    }
}

// killough 3/15/98:
//
// A fast function for testing intersections between things and linedefs.

boolean ThingIsOnLine(mobj_t *t, line_t *l)
{
  int dx = l->dx >> FRACBITS;                             // Linedef vector
  int dy = l->dy >> FRACBITS;
  int a = (l->v1->x >> FRACBITS) - (t->x >> FRACBITS);    // Thing-->v1 vector
  int b = (l->v1->y >> FRACBITS) - (t->y >> FRACBITS);
  int r = t->radius >> FRACBITS;                          // Thing radius

  // First make sure bounding boxes of linedef and thing intersect.
  // Leads to quick rejection using only shifts and adds/subs/compares.

  if (abs(a*2+dx)-abs(dx) > r*2 || abs(b*2+dy)-abs(dy) > r*2)
    return 0;

  // Next, make sure that at least one thing crosshair intersects linedef's
  // extension. Requires only 3-4 multiplications, the rest adds/subs/
  // shifts/xors (writing the steps out this way leads to better codegen).

  a *= dy;
  b *= dx;
  a -= b;
  b = dx + dy;
  b *= r;
  if (((a-b)^(a+b)) < 0)
    return 1;
  dy -= dx;
  dy *= r;
  b = a+dy;
  a -= dy;
  return (a^b) < 0;
}

//
// BLOCK MAP ITERATORS
// For each line/thing in the given mapblock,
// call the passed PIT_* function.
// If the function returns false,
// exit with false without checking anything else.
//

//
// P_BlockLinesIterator
// The validcount flags are used to avoid checking lines
// that are marked in multiple mapblocks,
// so increment validcount before the first call
// to P_BlockLinesIterator, then make one or more calls
// to it.
//
// killough 5/3/98: reformatted, cleaned up

boolean P_BlockLinesIterator(int x, int y, boolean func(line_t*))
{
  int        offset;
  const long *list;   // killough 3/1/98: for removal of blockmap limit

  if (x<0 || y<0 || x>=bmapwidth || y>=bmapheight)
    return true;
  offset = y*bmapwidth+x;
  offset = *(blockmap+offset);
  list = blockmaplump+offset;     // original was reading         // phares
                                  // delmiting 0 as linedef 0     // phares

  // killough 1/31/98: for compatibility we need to use the old method.
  // Most demos go out of sync, and maybe other problems happen, if we
  // don't consider linedef 0. For safety this should be qualified.

  // killough 2/22/98: demo_compatibility check
  // mbf21: Fix blockmap issue seen in btsx e2 Map 20
  if ((!demo_compatibility && !mbf21) || (mbf21 && skipblstart))
    list++;     // skip 0 starting delimiter                      // phares
  for ( ; *list != -1 ; list++)                                   // phares
    {
      line_t *ld = &lines[*list];
      if (ld->validcount == validcount)
        continue;       // line has already been checked
      ld->validcount = validcount;
      if (!func(ld))
        return false;
    }
  return true;  // everything was checked
}

//
// P_BlockThingsIterator
//
// killough 5/3/98: reformatted, cleaned up

boolean blockmapfix;

boolean P_BlockThingsIterator(int x, int y, boolean func(mobj_t*))
{
  mobj_t *mobj;

  if (x < 0 || y < 0 || x >= bmapwidth || y >= bmapheight)
    return true;

  for (mobj = blocklinks[y*bmapwidth+x]; mobj; mobj = mobj->bnext)
    if (!func(mobj))
      return false;

  // Blockmap bug fix by Terry Hearst
  // https://github.com/fabiangreffrath/crispy-doom/pull/723
  // Add other mobjs from surrounding blocks that overlap this one
  if (CRITICAL(blockmapfix))
  {
    if (demo_compatibility && overflow[emu_intercepts].enabled)
      return true;

    // Don't do for explosions and crashers
    if (func == PIT_RadiusAttack || func == PIT_ChangeSector)
      return true;

    // Unwrapped for least number of bounding box checks
    // (-1, -1)
    if (x > 0 && y > 0)
    {
      for (mobj = blocklinks[(y-1)*bmapwidth+(x-1)]; mobj; mobj = mobj->bnext)
      {
        const int xx = (mobj->x + mobj->radius - bmaporgx)>>MAPBLOCKSHIFT;
        const int yy = (mobj->y + mobj->radius - bmaporgy)>>MAPBLOCKSHIFT;
        if (xx == x && yy == y)
          if (!func(mobj))
            return false;
      }
    }
    // (0, -1)
    if (y > 0)
    {
      for (mobj = blocklinks[(y-1)*bmapwidth+x]; mobj; mobj = mobj->bnext)
      {
        const int yy = (mobj->y + mobj->radius - bmaporgy)>>MAPBLOCKSHIFT;
        if (yy == y)
          if (!func(mobj))
            return false;
      }
    }
    // (1, -1)
    if (x < (bmapwidth-1) && y > 0)
    {
      for (mobj = blocklinks[(y-1)*bmapwidth+(x+1)]; mobj; mobj = mobj->bnext)
      {
        const int xx = (mobj->x - mobj->radius - bmaporgx)>>MAPBLOCKSHIFT;
        const int yy = (mobj->y + mobj->radius - bmaporgy)>>MAPBLOCKSHIFT;
        if (xx == x && yy == y)
          if (!func(mobj))
            return false;
      }
    }
    // (1, 0)
    if (x < (bmapwidth-1))
    {
      for (mobj = blocklinks[y*bmapwidth+(x+1)]; mobj; mobj = mobj->bnext)
      {
        const int xx = (mobj->x - mobj->radius - bmaporgx)>>MAPBLOCKSHIFT;
        if (xx == x)
          if (!func(mobj))
            return false;
      }
    }
    // (1, 1)
    if (x < (bmapwidth-1) && y < (bmapheight-1))
    {
      for (mobj = blocklinks[(y+1)*bmapwidth+(x+1)]; mobj; mobj = mobj->bnext)
      {
        const int xx = (mobj->x - mobj->radius - bmaporgx)>>MAPBLOCKSHIFT;
        const int yy = (mobj->y - mobj->radius - bmaporgy)>>MAPBLOCKSHIFT;
        if (xx == x && yy == y)
            if (!func( mobj ) )
                return false;
      }
    }
    // (0, 1)
    if (y < (bmapheight-1))
    {
      for (mobj = blocklinks[(y+1)*bmapwidth+x]; mobj; mobj = mobj->bnext)
      {
        const int yy = (mobj->y - mobj->radius - bmaporgy)>>MAPBLOCKSHIFT;
        if (yy == y)
          if (!func(mobj))
            return false;
      }
    }
    // (-1, 1)
    if (x > 0 && y < (bmapheight-1))
    {
      for (mobj = blocklinks[(y+1)*bmapwidth+(x-1)]; mobj; mobj = mobj->bnext)
      {
        const int xx = (mobj->x + mobj->radius - bmaporgx)>>MAPBLOCKSHIFT;
        const int yy = (mobj->y - mobj->radius - bmaporgy)>>MAPBLOCKSHIFT;
        if (xx == x && yy == y)
          if (!func(mobj))
            return false;
      }
    }
    // (-1, 0)
    if (x > 0)
    {
      for (mobj = blocklinks[y*bmapwidth+(x-1)]; mobj; mobj = mobj->bnext)
      {
        const int xx = (mobj->x + mobj->radius - bmaporgx)>>MAPBLOCKSHIFT;
        if (xx == x)
          if (!func(mobj))
            return false;
      }
    }
  }

  return true;
}

//
// INTERCEPT ROUTINES
//

#define MAXINTERCEPTS_ORIGINAL 128

// 1/11/98 killough: Intercept limit removed
static intercept_t *intercepts, *intercept_p;

// Check for limit and double size if necessary -- killough
static void check_intercept(void)
{
  static size_t num_intercepts;
  size_t offset = intercept_p - intercepts;
  if (offset >= num_intercepts)
    {
      num_intercepts = num_intercepts ? num_intercepts*2 : MAXINTERCEPTS_ORIGINAL;
      intercepts = Z_Realloc(intercepts, sizeof(*intercepts)*num_intercepts, PU_STATIC, 0);
      intercept_p = intercepts + offset;
    }
}

divline_t trace;

static void InterceptsOverrun(int num_intercepts, intercept_t *intercept);

// PIT_AddLineIntercepts.
// Looks for lines in the given block
// that intercept the given trace
// to add to the intercepts list.
//
// A line is crossed if its endpoints
// are on opposite sides of the trace.
//
// killough 5/3/98: reformatted, cleaned up

boolean PIT_AddLineIntercepts(line_t *ld)
{
  int       s1;
  int       s2;
  fixed_t   frac;
  divline_t dl;

  // avoid precision problems with two routines
  if (trace.dx >  FRACUNIT*16 || trace.dy >  FRACUNIT*16 ||
      trace.dx < -FRACUNIT*16 || trace.dy < -FRACUNIT*16)
    {
      s1 = P_PointOnDivlineSide (ld->v1->x, ld->v1->y, &trace);
      s2 = P_PointOnDivlineSide (ld->v2->x, ld->v2->y, &trace);
    }
  else
    {
      s1 = P_PointOnLineSide (trace.x, trace.y, ld);
      s2 = P_PointOnLineSide (trace.x+trace.dx, trace.y+trace.dy, ld);
    }

  if (s1 == s2)
    return true;        // line isn't crossed

  // hit the line
  P_MakeDivline(ld, &dl);
  frac = P_InterceptVector(&trace, &dl);

  if (frac < 0)
    return true;        // behind source

  check_intercept();    // killough

  intercept_p->frac = frac;
  intercept_p->isaline = true;
  intercept_p->d.line = ld;
  InterceptsOverrun(intercept_p - intercepts, intercept_p);
  intercept_p++;

  return true;  // continue
}

//
// PIT_AddThingIntercepts
//
// killough 5/3/98: reformatted, cleaned up

boolean PIT_AddThingIntercepts(mobj_t *thing)
{
  fixed_t   x1, y1;
  fixed_t   x2, y2;
  int       s1, s2;
  divline_t dl;
  fixed_t   frac;

  // check a corner to corner crossection for hit
  if ((trace.dx ^ trace.dy) > 0)
    {
      x1 = thing->x - thing->radius;
      y1 = thing->y + thing->radius;
      x2 = thing->x + thing->radius;
      y2 = thing->y - thing->radius;
    }
  else
    {
      x1 = thing->x - thing->radius;
      y1 = thing->y - thing->radius;
      x2 = thing->x + thing->radius;
      y2 = thing->y + thing->radius;
    }

  s1 = P_PointOnDivlineSide (x1, y1, &trace);
  s2 = P_PointOnDivlineSide (x2, y2, &trace);

  if (s1 == s2)
    return true;                // line isn't crossed

  dl.x = x1;
  dl.y = y1;
  dl.dx = x2-x1;
  dl.dy = y2-y1;

  frac = P_InterceptVector (&trace, &dl);

  if (frac < 0)
    return true;                // behind source

  check_intercept();            // killough

  intercept_p->frac = frac;
  intercept_p->isaline = false;
  intercept_p->d.thing = thing;
  InterceptsOverrun(intercept_p - intercepts, intercept_p);
  intercept_p++;

  return true;          // keep going
}

//
// P_TraverseIntercepts
// Returns true if the traverser function returns true
// for all lines.
//
// killough 5/3/98: reformatted, cleaned up

boolean P_TraverseIntercepts(traverser_t func, fixed_t maxfrac)
{
  intercept_t *in = NULL;
  int count = intercept_p - intercepts;
  while (count--)
    {
      fixed_t dist = INT_MAX;
      intercept_t *scan;
      for (scan = intercepts; scan < intercept_p; scan++)
        if (scan->frac < dist)
          dist = (in=scan)->frac;
      if (dist > maxfrac)
        return true;    // checked everything in range
      if (!func(in))
        return false;           // don't bother going farther
      in->frac = INT_MAX;
    }
  return true;                  // everything was traversed
}

// Intercepts Overrun emulation, from PrBoom-plus.
// Thanks to Andrey Budko (entryway) for researching this and his 
// implementation of Intercepts Overrun emulation in PrBoom-plus
// which this is based on.

typedef struct
{
    int len;
    void *addr;
    boolean int16_array;
} intercepts_overrun_t;

// Intercepts memory table.  This is where various variables are located
// in memory in Vanilla Doom.  When the intercepts table overflows, we
// need to write to them.
//
// Almost all of the values to overwrite are 32-bit integers, except for
// playerstarts, which is effectively an array of 16-bit integers and
// must be treated differently.

static intercepts_overrun_t intercepts_overrun[] =
{
    {4,   NULL,                          false},
    {4,   NULL, /* &earlyout, */         false},
    {4,   NULL, /* &intercept_p, */      false},
    {4,   &lowfloor,                     false},
    {4,   &openbottom,                   false},
    {4,   &opentop,                      false},
    {4,   &openrange,                    false},
    {4,   NULL,                          false},
    {120, NULL, /* &activeplats, */      false},
    {8,   NULL,                          false},
    {4,   &bulletslope,                  false},
    {4,   NULL, /* &swingx, */           false},
    {4,   NULL, /* &swingy, */           false},
    {4,   NULL,                          false},
    {40,  &playerstarts,                 true},
    {4,   NULL, /* &blocklinks, */       false},
    {4,   &bmapwidth,                    false},
    {4,   NULL, /* &blockmap, */         false},
    {4,   &bmaporgx,                     false},
    {4,   &bmaporgy,                     false},
    {4,   NULL, /* &blockmaplump, */     false},
    {4,   &bmapheight,                   false},
    {0,   NULL,                          false},
};

// Overwrite a specific memory location with a value.

static void InterceptsMemoryOverrun(int location, int value)
{
    int i, offset;
    int index;
    void *addr;

    i = 0;
    offset = 0;

    // Search down the array until we find the right entry

    while (intercepts_overrun[i].len != 0)
    {
        if (offset + intercepts_overrun[i].len > location)
        {
            addr = intercepts_overrun[i].addr;

            // Write the value to the memory location.
            // 16-bit and 32-bit values are written differently.

            if (addr != NULL)
            {
                if (intercepts_overrun[i].int16_array)
                {
                    index = (location - offset) / 2;
                    ((short *) addr)[index] = value & FRACMASK;
                    ((short *) addr)[index + 1] = (value >> 16) & FRACMASK;
                }
                else
                {
                    index = (location - offset) / 4;
                    ((int *) addr)[index] = value;
                }
            }

            break;
        }

        offset += intercepts_overrun[i].len;
        ++i;
    }
}

// Emulate overruns of the intercepts[] array.

static void InterceptsOverrun(int num_intercepts, intercept_t *intercept)
{
  if (num_intercepts > MAXINTERCEPTS_ORIGINAL && demo_compatibility
      && overflow[emu_intercepts].enabled)
  {
    int location;

    if (num_intercepts == MAXINTERCEPTS_ORIGINAL + 1)
    {
        overflow[emu_intercepts].triggered = true;
        // [crispy] print a warning
        I_Printf(VB_WARNING, "Triggered INTERCEPTS overflow!");
    }

    location = (num_intercepts - MAXINTERCEPTS_ORIGINAL - 1) * 12;

    // Overwrite memory that is overwritten in Vanilla Doom, using
    // the values from the intercept structure.
    //
    // Note: the ->d.{thing,line} member should really have its
    // address translated into the correct address value for 
    // Vanilla Doom.

    InterceptsMemoryOverrun(location, intercept->frac);
    InterceptsMemoryOverrun(location + 4, intercept->isaline);
    InterceptsMemoryOverrun(location + 8, (intptr_t) intercept->d.thing);
  }
}

//
// P_PathTraverse
// Traces a line from x1,y1 to x2,y2,
// calling the traverser function for each.
// Returns true if the traverser function returns true
// for all lines.
//
// killough 5/3/98: reformatted, cleaned up

boolean P_PathTraverse(fixed_t x1, fixed_t y1, fixed_t x2, fixed_t y2,
                       int flags, boolean trav(intercept_t *))
{
  fixed_t xt1, yt1;
  fixed_t xt2, yt2;
  fixed_t xstep, ystep;
  fixed_t partial;
  fixed_t xintercept, yintercept;
  int     mapx, mapy;
  int     mapxstep, mapystep;
  int     count;

  validcount++;
  intercept_p = intercepts;

  if (!((x1-bmaporgx)&(MAPBLOCKSIZE-1)))
    x1 += FRACUNIT;     // don't side exactly on a line

  if (!((y1-bmaporgy)&(MAPBLOCKSIZE-1)))
    y1 += FRACUNIT;     // don't side exactly on a line

  trace.x = x1;
  trace.y = y1;
  trace.dx = x2 - x1;
  trace.dy = y2 - y1;

  x1 -= bmaporgx;
  y1 -= bmaporgy;
  xt1 = x1>>MAPBLOCKSHIFT;
  yt1 = y1>>MAPBLOCKSHIFT;

  x2 -= bmaporgx;
  y2 -= bmaporgy;
  xt2 = x2>>MAPBLOCKSHIFT;
  yt2 = y2>>MAPBLOCKSHIFT;

  if (xt2 > xt1)
    {
      mapxstep = 1;
      partial = FRACUNIT - ((x1>>MAPBTOFRAC)&FRACMASK);
      ystep = FixedDiv (y2-y1,abs(x2-x1));
    }
  else
    if (xt2 < xt1)
      {
        mapxstep = -1;
        partial = (x1>>MAPBTOFRAC)&FRACMASK;
        ystep = FixedDiv (y2-y1,abs(x2-x1));
      }
    else
      {
        mapxstep = 0;
        partial = FRACUNIT;
        ystep = 256*FRACUNIT;
      }

  yintercept = (y1>>MAPBTOFRAC) + FixedMul(partial, ystep);

  if (yt2 > yt1)
    {
      mapystep = 1;
      partial = FRACUNIT - ((y1>>MAPBTOFRAC)&FRACMASK);
      xstep = FixedDiv (x2-x1,abs(y2-y1));
    }
  else
    if (yt2 < yt1)
      {
        mapystep = -1;
        partial = (y1>>MAPBTOFRAC)&FRACMASK;
        xstep = FixedDiv (x2-x1,abs(y2-y1));
      }
    else
      {
        mapystep = 0;
        partial = FRACUNIT;
        xstep = 256*FRACUNIT;
      }

  xintercept = (x1>>MAPBTOFRAC) + FixedMul (partial, xstep);

  // Step through map blocks.
  // Count is present to prevent a round off error
  // from skipping the break.

  mapx = xt1;
  mapy = yt1;

  for (count = 0; count < 64; count++)
    {
      if (flags & PT_ADDLINES)
        if (!P_BlockLinesIterator(mapx, mapy,PIT_AddLineIntercepts))
          return false; // early out

      if (flags & PT_ADDTHINGS)
        if (!P_BlockThingsIterator(mapx, mapy,PIT_AddThingIntercepts))
          return false; // early out

      if (mapx == xt2 && mapy == yt2)
        break;

      if ((yintercept >> FRACBITS) == mapy)
        {
          yintercept += ystep;
          mapx += mapxstep;
        }
      else
        if ((xintercept >> FRACBITS) == mapx)
          {
            xintercept += xstep;
            mapy += mapystep;
          }
    }

  // go through the sorted list
  return P_TraverseIntercepts(trav, FRACUNIT);
}

//
// mbf21: RoughBlockCheck
// [XA] adapted from Hexen -- used by P_RoughTargetSearch
//

static mobj_t *RoughBlockCheck(mobj_t *mo, int index, angle_t fov)
{
  mobj_t *link;

  link = blocklinks[index];
  while (link)
  {
    // skip non-shootable actors
    if (!(link->flags & MF_SHOOTABLE))
    {
      link = link->bnext;
      continue;
    }

    // skip the projectile's owner
    if (link == mo->target)
    {
      link = link->bnext;
      continue;
    }

    // skip actors on the same "team", unless infighting or deathmatching
    if (mo->target &&
      !((link->flags ^ mo->target->flags) & MF_FRIEND) &&
      mo->target->target != link &&
      !(deathmatch && link->player && mo->target->player))
    {
      link = link->bnext;
      continue;
    }

    // skip actors outside of specified FOV
    if (fov > 0 && !P_CheckFov(mo, link, fov))
    {
      link = link->bnext;
      continue;
    }

    // skip actors not in line of sight
    if (!P_CheckSight(mo, link))
    {
      link = link->bnext;
      continue;
    }

    // all good! return it.
    return link;
  }

  // couldn't find a valid target
  return NULL;
}

//
// mbf21: P_RoughTargetSearch
// Searches though the surrounding mapblocks for monsters/players
// based on Hexen's P_RoughMonsterSearch
//
// distance is in MAPBLOCKUNITS

mobj_t *P_RoughTargetSearch(mobj_t *mo, angle_t fov, int distance)
{
  int blockX;
  int blockY;
  int startX, startY;
  int blockIndex;
  int firstStop;
  int secondStop;
  int thirdStop;
  int finalStop;
  int count;
  mobj_t *target;

  startX = (mo->x - bmaporgx) >> MAPBLOCKSHIFT;
  startY = (mo->y - bmaporgy) >> MAPBLOCKSHIFT;

  if (startX >= 0 && startX < bmapwidth && startY >= 0 && startY < bmapheight)
  {
    if ((target = RoughBlockCheck(mo, startY*bmapwidth + startX, fov)))
    { // found a target right away
      return target;
    }
  }
  for (count = 1; count <= distance; count++)
  {
    blockX = startX - count;
    blockY = startY - count;

    if (blockY < 0)
    {
      blockY = 0;
    }
    else if (blockY >= bmapheight)
    {
      blockY = bmapheight - 1;
    }
    if (blockX < 0)
    {
      blockX = 0;
    }
    else if (blockX >= bmapwidth)
    {
      blockX = bmapwidth - 1;
    }
    blockIndex = blockY * bmapwidth + blockX;
    firstStop = startX + count;
    if (firstStop < 0)
    {
      continue;
    }
    if (firstStop >= bmapwidth)
    {
      firstStop = bmapwidth - 1;
    }
    secondStop = startY + count;
    if (secondStop < 0)
    {
      continue;
    }
    if (secondStop >= bmapheight)
    {
      secondStop = bmapheight - 1;
    }
    thirdStop = secondStop * bmapwidth + blockX;
    secondStop = secondStop * bmapwidth + firstStop;
    firstStop += blockY * bmapwidth;
    finalStop = blockIndex;

    // Trace the first block section (along the top)
    for (; blockIndex <= firstStop; blockIndex++)
    {
      if ((target = RoughBlockCheck(mo, blockIndex, fov)))
      {
        return target;
      }
    }
    // Trace the second block section (right edge)
    for (blockIndex--; blockIndex <= secondStop; blockIndex += bmapwidth)
    {
      if ((target = RoughBlockCheck(mo, blockIndex, fov)))
      {
        return target;
      }
    }
    // Trace the third block section (bottom edge)
    for (blockIndex -= bmapwidth; blockIndex >= thirdStop; blockIndex--)
    {
      if ((target = RoughBlockCheck(mo, blockIndex, fov)))
      {
        return target;
      }
    }
    // Trace the final block section (left edge)
    for (blockIndex++; blockIndex > finalStop; blockIndex -= bmapwidth)
    {
      if ((target = RoughBlockCheck(mo, blockIndex, fov)))
      {
        return target;
      }
    }
  }
  return NULL;
}

/*
==================
=
= P_SightBlockLinesIterator
=
===================
*/

static boolean P_SightBlockLinesIterator(int x, int y)
{
  int offset;
  long *list;
  line_t *ld;
  int s1, s2;
  divline_t dl;

  if (x < 0 || y < 0 || x >= bmapwidth || y >= bmapheight)
    return true;

  offset = y*bmapwidth+x;

  offset = *(blockmap+offset);

  for (list = blockmaplump+offset; *list != -1; list++)
  {
    ld = &lines[*list];
    if (ld->validcount == validcount)
      continue;    // line has already been checked
    ld->validcount = validcount;

    s1 = P_PointOnDivlineSide(ld->v1->x, ld->v1->y, &trace);
    s2 = P_PointOnDivlineSide(ld->v2->x, ld->v2->y, &trace);
    if (s1 == s2)
      continue;    // line isn't crossed
    P_MakeDivline (ld, &dl);
    s1 = P_PointOnDivlineSide(trace.x, trace.y, &dl);
    s2 = P_PointOnDivlineSide(trace.x+trace.dx, trace.y+trace.dy, &dl);
    if (s1 == s2)
      continue; // line isn't crossed

    // try to early out the check
    if (!ld->backsector)
      return false; // stop checking

    check_intercept();    // killough

    // store the line for later intersection testing
    intercept_p->d.line = ld;
    intercept_p++;

  }

  return true; // everything was checked
}

/*
====================
=
= P_SightTraverseIntercepts
=
= Returns true if the traverser function returns true for all lines
====================
*/

static boolean P_SightTraverseIntercepts(void)
{
  int count;
  fixed_t dist;
  intercept_t *scan, *in;
  divline_t dl;

  count = intercept_p - intercepts;
  //
  // calculate intercept distance
  //
  for (scan = intercepts; scan<intercept_p; scan++)
  {
    P_MakeDivline(scan->d.line, &dl);
    scan->frac = P_InterceptVector(&trace, &dl);
  }

  //
  // go through in order
  //
  in = 0; // shut up compiler warning

  while (count--)
  {
    dist = INT_MAX;
    for (scan = intercepts ; scan<intercept_p ; scan++)
      if (scan->frac < dist)
      {
        dist = scan->frac;
        in = scan;
      }

    if (!PTR_SightTraverse(in))
        return false;      // don't bother going farther
      in->frac = INT_MAX;
  }

  return true;    // everything was traversed
}

/*
==================
=
= P_SightPathTraverse
=
= Traces a line from x1,y1 to x2,y2, calling the traverser function for each
= Returns true if the traverser function returns true for all lines
==================
*/

boolean P_SightPathTraverse(fixed_t x1, fixed_t y1, fixed_t x2, fixed_t y2)
{
  fixed_t xt1,yt1,xt2,yt2;
  fixed_t xstep,ystep;
  fixed_t partial;
  fixed_t xintercept, yintercept;
  int mapx, mapy, mapxstep, mapystep;
  int count;

  validcount++;
  intercept_p = intercepts;

  if (((x1-bmaporgx)&(MAPBLOCKSIZE-1)) == 0)
    x1 += FRACUNIT;        // don't side exactly on a line
  if (((y1-bmaporgy)&(MAPBLOCKSIZE-1)) == 0)
    y1 += FRACUNIT;        // don't side exactly on a line
  trace.x = x1;
  trace.y = y1;
  trace.dx = x2 - x1;
  trace.dy = y2 - y1;

  x1 -= bmaporgx;
  y1 -= bmaporgy;
  xt1 = x1 >> MAPBLOCKSHIFT;
  yt1 = y1 >> MAPBLOCKSHIFT;

  x2 -= bmaporgx;
  y2 -= bmaporgy;
  xt2 = x2 >> MAPBLOCKSHIFT;
  yt2 = y2 >> MAPBLOCKSHIFT;

  // points should never be out of bounds, but check once instead of
  // each block
  if (xt1<0 || yt1<0 || xt1>=bmapwidth || yt1>=bmapheight
    || xt2<0 || yt2<0 || xt2>=bmapwidth || yt2>=bmapheight)
    return false;

  if (xt2 > xt1)
  {
    mapxstep = 1;
    partial = FRACUNIT - ((x1>>MAPBTOFRAC)&FRACMASK);
    ystep = FixedDiv (y2-y1,abs(x2-x1));
  }
  else if (xt2 < xt1)
  {
    mapxstep = -1;
    partial = (x1>>MAPBTOFRAC)&FRACMASK;
    ystep = FixedDiv (y2-y1,abs(x2-x1));
  }
  else
  {
    mapxstep = 0;
    partial = FRACUNIT;
    ystep = 256*FRACUNIT;
  }
  yintercept = (y1>>MAPBTOFRAC) + FixedMul (partial, ystep);


  if (yt2 > yt1)
  {
    mapystep = 1;
    partial = FRACUNIT - ((y1>>MAPBTOFRAC)&FRACMASK);
    xstep = FixedDiv (x2-x1,abs(y2-y1));
  }
  else if (yt2 < yt1)
  {
    mapystep = -1;
    partial = (y1>>MAPBTOFRAC)&FRACMASK;
    xstep = FixedDiv (x2-x1,abs(y2-y1));
  }
  else
  {
    mapystep = 0;
    partial = FRACUNIT;
    xstep = 256*FRACUNIT;
  }
  xintercept = (x1>>MAPBTOFRAC) + FixedMul (partial, xstep);


  //
  // step through map blocks
  // Count is present to prevent a round off error from skipping the break
  mapx = xt1;
  mapy = yt1;

  for (count = 0; count < 64; count++)
  {
    if (!P_SightBlockLinesIterator(mapx, mapy))
    {
      return false;  // early out
    }

    if (mapx == xt2 && mapy == yt2)
      break;

    // [RH] Handle corner cases properly instead of pretending they don't
    // exist.
    // neither xintercept nor yintercept match!
    if ((xintercept >> FRACBITS) != mapx && (yintercept >> FRACBITS) != mapy)
    {
      count = 64;  // Stop traversing, because somebody screwed up.
    }
    // xintercept and yintercept both match
    else if ((xintercept >> FRACBITS) == mapx && (yintercept >> FRACBITS) == mapy)
    {
      // The trace is exiting a block through its corner. Not only does the
      // block being entered need to be checked (which will happen when this
      // loop continues), but the other two blocks adjacent to the corner
      // also need to be checked.

      if (!P_SightBlockLinesIterator(mapx + mapxstep, mapy) ||
          !P_SightBlockLinesIterator(mapx, mapy + mapystep))
      {
        return false;
      }
      xintercept += xstep;
      yintercept += ystep;
      mapx += mapxstep;
      mapy += mapystep;
    }
    else if ((yintercept >> FRACBITS) == mapy)
    {
      yintercept += ystep;
      mapx += mapxstep;
    }
    else if ((xintercept >> FRACBITS) == mapx)
    {
      xintercept += xstep;
      mapy += mapystep;
    }

  }

  return P_SightTraverseIntercepts();
}

//----------------------------------------------------------------------------
//
// $Log: p_maputl.c,v $
// Revision 1.13  1998/05/03  22:16:48  killough
// beautification
//
// Revision 1.12  1998/03/20  00:30:03  phares
// Changed friction to linedef control
//
// Revision 1.11  1998/03/19  14:37:12  killough
// Fix ThingIsOnLine()
//
// Revision 1.10  1998/03/19  00:40:52  killough
// Change ThingIsOnLine() comments
//
// Revision 1.9  1998/03/16  12:34:45  killough
// Add ThingIsOnLine() function
//
// Revision 1.8  1998/03/09  18:27:10  phares
// Fixed bug in neighboring variable friction sectors
//
// Revision 1.7  1998/03/09  07:19:26  killough
// Remove use of FP for point/line queries
//
// Revision 1.6  1998/03/02  12:03:43  killough
// Change blockmap offsets to 32-bit
//
// Revision 1.5  1998/02/23  04:45:24  killough
// Relax blockmap iterator to demo_compatibility
//
// Revision 1.4  1998/02/02  13:41:38  killough
// Fix demo sync programs caused by last change
//
// Revision 1.3  1998/01/30  23:13:10  phares
// Fixed delimiting 0 bug in P_BlockLinesIterator
//
// Revision 1.2  1998/01/26  19:24:11  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:03:00  rand
// Lee's Jan 19 sources
//
//----------------------------------------------------------------------------
