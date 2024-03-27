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
//      Map functions
//
//-----------------------------------------------------------------------------

#ifndef __P_MAP__
#define __P_MAP__

#include "doomtype.h"
#include "m_fixed.h"
#include "tables.h"

struct mobj_s;
struct msecnode_s;
struct player_s;
struct sector_s;

#define USERANGE        (64*FRACUNIT)
#define MELEERANGE      (64*FRACUNIT)
#define MISSILERANGE    (32*64*FRACUNIT)

// mbf21: a couple of explicit constants for non-melee things that used to use MELEERANGE
#define WAKEUPRANGE     (64*FRACUNIT)
#define SNEAKRANGE      (128*FRACUNIT)

// MAXRADIUS is for precalculated sector block boxes the spider demon
// is larger, but we do not have any moving sectors nearby
#define MAXRADIUS       (32*FRACUNIT)

#define CROSSHAIR_AIM 1

// killough 3/15/98: add fourth argument to P_TryMove
boolean P_TryMove(struct mobj_s *thing, fixed_t x, fixed_t y, boolean dropoff);

// killough 8/9/98: extra argument for telefragging
boolean P_TeleportMove(struct mobj_s *thing, fixed_t x, fixed_t y, boolean boss);
void    P_SlideMove(struct mobj_s *mo);
extern boolean (*P_CheckSight)(struct mobj_s *t1, struct mobj_s *t2);
boolean P_CheckFov(struct mobj_s *t1, struct mobj_s *t2, angle_t fov);
void    P_UseLines(struct player_s *player);

// killough 8/2/98: add 'mask' argument to prevent friends autoaiming at others
fixed_t P_AimLineAttack(struct mobj_s *t1, angle_t angle, fixed_t distance, int mask);
void    P_LineAttack(struct mobj_s *t1, angle_t angle, fixed_t distance,
                     fixed_t slope, int damage );
void    P_RadiusAttack(struct mobj_s *spot, struct mobj_s *source,
                       int damage, int distance);
boolean P_CheckPosition(struct mobj_s *thing, fixed_t x, fixed_t y);

//jff 3/19/98 P_CheckSector(): new routine to replace P_ChangeSector()
boolean P_CheckSector(struct sector_s *sector, boolean crunch);
void    P_DelSeclist(struct msecnode_s *);                          // phares 3/16/98
void    P_CreateSecNodeList(struct mobj_s *, fixed_t, fixed_t);       // phares 3/14/98
boolean Check_Sides(struct mobj_s *, int, int);                    // phares

int     P_GetMoveFactor(const struct mobj_s *mo, int *friction);   // killough 8/28/98
int     P_GetFriction(const struct mobj_s *mo, int *factor);       // killough 8/28/98
void    P_ApplyTorque(struct mobj_s *mo);                          // killough 9/12/98

void    P_MapStart(void);
void    P_MapEnd(void);

// If "floatok" true, move would be ok if within "tmfloorz - tmceilingz".
extern boolean floatok;
extern boolean felldown;   // killough 11/98: indicates object pushed off ledge
extern fixed_t tmfloorz;
extern fixed_t tmceilingz;
extern struct line_s *ceilingline;
extern struct line_s *floorline;      // killough 8/23/98
extern struct mobj_s *linetarget;     // who got hit (or NULL)
extern struct msecnode_s *sector_list;                             // phares 3/16/98
extern fixed_t tmbbox[4];         // phares 3/20/98
extern struct line_s *blockline;   // killough 8/11/98

#endif // __P_MAP__

//----------------------------------------------------------------------------
//
// $Log: p_map.h,v $
// Revision 1.2  1998/05/07  00:53:07  killough
// Add more external declarations
//
// Revision 1.1  1998/05/03  22:19:23  killough
// External declarations formerly in p_local.h
//
//
//----------------------------------------------------------------------------
