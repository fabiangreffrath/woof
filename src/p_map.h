// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id: p_map.h,v 1.2 1998/05/07 00:53:07 killough Exp $
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
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 
//  02111-1307, USA.
//
// DESCRIPTION:
//      Map functions
//
//-----------------------------------------------------------------------------

#ifndef __P_MAP__
#define __P_MAP__

#include "r_defs.h"
#include "d_player.h"

#define USERANGE        (64*FRACUNIT)
#define MELEERANGE      (64*FRACUNIT)
#define MISSILERANGE    (32*64*FRACUNIT)

// MAXRADIUS is for precalculated sector block boxes the spider demon
// is larger, but we do not have any moving sectors nearby
#define MAXRADIUS       (32*FRACUNIT)

// killough 3/15/98: add fourth argument to P_TryMove
boolean P_TryMove(mobj_t *thing, fixed_t x, fixed_t y, boolean dropoff);

// killough 8/9/98: extra argument for telefragging
boolean P_TeleportMove(mobj_t *thing, fixed_t x, fixed_t y,boolean boss);
void    P_SlideMove(mobj_t *mo);
boolean P_CheckSight(mobj_t *t1, mobj_t *t2);
void    P_UseLines(player_t *player);

// killough 8/2/98: add 'mask' argument to prevent friends autoaiming at others
fixed_t P_AimLineAttack(mobj_t *t1, angle_t angle, fixed_t distance,int mask);
void    P_LineAttack(mobj_t *t1, angle_t angle, fixed_t distance,
                     fixed_t slope, int damage );
void    P_RadiusAttack(mobj_t *spot, mobj_t *source, int damage);
boolean P_CheckPosition(mobj_t *thing, fixed_t x, fixed_t y);

//jff 3/19/98 P_CheckSector(): new routine to replace P_ChangeSector()
boolean P_CheckSector(sector_t *sector, boolean crunch);
void    P_DelSeclist(msecnode_t*);                          // phares 3/16/98
void    P_CreateSecNodeList(mobj_t*,fixed_t,fixed_t);       // phares 3/14/98
boolean Check_Sides(mobj_t *, int, int);                    // phares

int     P_GetMoveFactor(const mobj_t *mo, int *friction);   // killough 8/28/98
int     P_GetFriction(const mobj_t *mo, int *factor);       // killough 8/28/98
void    P_ApplyTorque(mobj_t *mo);                          // killough 9/12/98

// If "floatok" true, move would be ok if within "tmfloorz - tmceilingz".
extern boolean floatok;
extern boolean felldown;   // killough 11/98: indicates object pushed off ledge
extern fixed_t tmfloorz;
extern fixed_t tmceilingz;
extern line_t *ceilingline;
extern line_t *floorline;      // killough 8/23/98
extern mobj_t *linetarget;     // who got hit (or NULL)
extern msecnode_t *sector_list;                             // phares 3/16/98
extern fixed_t tmbbox[4];         // phares 3/20/98
extern line_t *blockline;   // killough 8/11/98

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
