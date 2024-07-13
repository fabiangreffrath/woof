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
//      Map utility functions
//
//-----------------------------------------------------------------------------

#ifndef __P_MAPUTL__
#define __P_MAPUTL__

#include "doomtype.h"
#include "m_fixed.h"
#include "tables.h"

struct mobj_s;
struct line_s;

// mapblocks are used to check movement against lines and things
#define MAPBLOCKUNITS   128
#define MAPBLOCKSIZE    (MAPBLOCKUNITS*FRACUNIT)
#define MAPBLOCKSHIFT   (FRACBITS+7)
#define MAPBMASK        (MAPBLOCKSIZE-1)
#define MAPBTOFRAC      (MAPBLOCKSHIFT-FRACBITS)

#define PT_ADDLINES     1
#define PT_ADDTHINGS    2
#define PT_EARLYOUT     4

typedef struct {
  fixed_t     x;
  fixed_t     y;
  fixed_t     dx;
  fixed_t     dy;
} divline_t;

typedef struct {
  fixed_t     frac;           // along trace line
  boolean     isaline;
  union {
    struct mobj_s* thing;
    struct line_s* line;
  } d;
} intercept_t;

typedef boolean (*traverser_t)(intercept_t *in);

fixed_t P_AproxDistance(fixed_t dx, fixed_t dy);
int     P_PointOnLineSide(fixed_t x, fixed_t y, struct line_s *line);
int     P_PointOnDivlineSide(fixed_t x, fixed_t y, divline_t *line);
void    P_MakeDivline(struct line_s *li, divline_t *dl);
fixed_t P_InterceptVector(divline_t *v2, divline_t *v1);
int     P_BoxOnLineSide(fixed_t *tmbox, struct line_s *ld);
void    P_LineOpening(struct line_s *linedef);
void    P_UnsetThingPosition(struct mobj_s *thing);
void    P_SetThingPosition(struct mobj_s *thing);
boolean P_BlockLinesIterator (int x, int y, boolean func(struct line_s *));
boolean P_BlockThingsIterator(int x, int y, boolean func(struct mobj_s *));
boolean ThingIsOnLine(struct mobj_s *t, struct line_s *l);  // killough 3/15/98
boolean P_PathTraverse(fixed_t x1, fixed_t y1, fixed_t x2, fixed_t y2,
                       int flags, boolean trav(intercept_t *));

angle_t P_PointToAngle(fixed_t xo, fixed_t yo, fixed_t x, fixed_t y);
struct mobj_s *P_RoughTargetSearch(struct mobj_s *mo, angle_t fov, int distance);

boolean P_SightPathTraverse(fixed_t x1, fixed_t y1, fixed_t x2, fixed_t y2);
boolean PTR_SightTraverse(intercept_t *in);
boolean P_CheckSight_12(struct mobj_s *t1, struct mobj_s *t2);

extern fixed_t opentop;
extern fixed_t openbottom;
extern fixed_t openrange;
extern fixed_t lowfloor;
extern divline_t trace;

extern boolean blockmapfix;

#endif  // __P_MAPUTL__

//----------------------------------------------------------------------------
//
// $Log: p_maputl.h,v $
// Revision 1.1  1998/05/03  22:19:26  killough
// External declarations formerly in p_local.h
//
//
//----------------------------------------------------------------------------
