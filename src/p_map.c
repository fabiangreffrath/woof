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
//  Movement, collision handling.
//  Shooting and aiming.
//
//-----------------------------------------------------------------------------

#include <stdlib.h>

#include "d_player.h"
#include "deh_misc.h"
#include "doomdata.h"
#include "doomdef.h"
#include "doomstat.h"
#include "hu_obituary.h"
#include "i_printf.h"
#include "i_system.h"
#include "info.h"
#include "m_arena.h"
#include "m_argv.h"
#include "m_bbox.h"
#include "m_misc.h"
#include "m_random.h"
#include "p_inter.h"
#include "p_map.h"
#include "p_maputl.h"
#include "p_mobj.h"
#include "p_setup.h"
#include "p_spec.h"
#include "p_user.h"
#include "r_defs.h"
#include "r_main.h"
#include "r_state.h"
#include "s_sound.h"
#include "sounds.h"
#include "v_video.h"
#include "z_zone.h"

static mobj_t    *tmthing;
static int       tmflags;
static fixed_t   tmx;
static fixed_t   tmy;
static int pe_x; // Pain Elemental position for Lost Soul checks // phares
static int pe_y; // Pain Elemental position for Lost Soul checks // phares
static int ls_x; // Lost Soul position for Lost Soul checks      // phares
static int ls_y; // Lost Soul position for Lost Soul checks      // phares

// If "floatok" true, move would be ok
// if within "tmfloorz - tmceilingz".

boolean   floatok;

// killough 11/98: if "felldown" true, object was pushed down ledge
boolean   felldown;

// The tm* items are used to hold information globally, usually for
// line or object intersection checking

fixed_t   tmbbox[4];  // bounding box for line intersection checks
fixed_t   tmfloorz;   // floor you'd hit if free to fall
fixed_t   tmceilingz; // ceiling of sector you're in
fixed_t   tmdropoffz; // dropoff on other side of line you're crossing

// keep track of the line that lowers the ceiling,
// so missiles don't explode against sky hack walls

line_t        *ceilingline;
line_t        *blockline;    // killough 8/11/98: blocking linedef
line_t        *floorline;    // killough 8/1/98: Highest touched floor
static int    tmunstuck;     // killough 8/1/98: whether to allow unsticking

// keep track of special lines as they are hit,
// but don't process them until the move is proven valid

// 1/11/98 killough: removed limit on special lines crossed
line_t **spechit;                // new code -- killough
static int spechit_max;          // killough

int numspechit;

// Temporary holder for thing_sectorlist threads
msecnode_t *sector_list = NULL;                             // phares 3/16/98

arena_t *msecnodes_arena;

//
// TELEPORT MOVE
//

//
// PIT_StompThing
//

static boolean telefrag;   // killough 8/9/98: whether to telefrag at exit

static boolean PIT_StompThing (mobj_t *thing)
{
  fixed_t blockdist;

  if (!(thing->flags & MF_SHOOTABLE)) // Can't shoot it? Can't stomp it!
    return true;

  blockdist = thing->radius + tmthing->radius;

  if (abs(thing->x - tmx) >= blockdist || abs(thing->y - tmy) >= blockdist)
    return true; // didn't hit it

  // don't clip against self
  if (thing == tmthing)
    return true;

  // monsters don't stomp things except on boss level
  if (!telefrag)  // killough 8/9/98: make consistent across all levels
    return false;

  P_DamageMobjBy (thing, tmthing, tmthing, 10000, MOD_Telefrag); // Stomp!

  return true;
}

//
// killough 8/28/98:
//
// P_GetFriction()
//
// Returns the friction associated with a particular mobj.

int P_GetFriction(const mobj_t *mo, int *frictionfactor)
{
  int friction = ORIG_FRICTION;
  int movefactor = ORIG_FRICTION_FACTOR;
  const msecnode_t *m;
  const sector_t *sec;

  // Assign the friction value to objects on the floor, non-floating,
  // and clipped. Normally the object's friction value is kept at
  // ORIG_FRICTION and this thinker changes it for icy or muddy floors.
  //
  // When the object is straddling sectors with the same
  // floorheight that have different frictions, use the lowest
  // friction value (muddy has precedence over icy).

  if (!(mo->flags & (MF_NOCLIP|MF_NOGRAVITY)) 
      && (demo_version >= DV_MBF || (mo->player && !compatibility)) &&
      variable_friction)
    for (m = mo->touching_sectorlist; m; m = m->m_tnext)
      if ((sec = m->m_sector)->special & FRICTION_MASK &&
	  (sec->friction < friction || friction == ORIG_FRICTION) &&
	  (mo->z <= sec->floorheight ||
	   (sec->heightsec != -1 &&
	    mo->z <= sectors[sec->heightsec].floorheight &&
	    demo_version >= DV_MBF)))
	friction = sec->friction, movefactor = sec->movefactor;
  
  if (frictionfactor)
    *frictionfactor = movefactor;

  return friction;
}

// phares 3/19/98
// P_GetMoveFactor() returns the value by which the x,y
// movements are multiplied to add to player movement.
//
// killough 8/28/98: rewritten

int P_GetMoveFactor(const mobj_t *mo, int *frictionp)
{
  int movefactor, friction;

  // Restore original Boom friction code for
  // demo compatibility
  if (demo_version < DV_MBF)
  {
    int momentum;

    movefactor = ORIG_FRICTION_FACTOR;

    if (!compatibility && variable_friction &&
      !(mo->flags & (MF_NOGRAVITY | MF_NOCLIP)))
    {
      friction = mo->friction;
      if (friction == ORIG_FRICTION)            // normal floor
        ;
      else if (friction > ORIG_FRICTION)        // ice
      {
        movefactor = mo->movefactor;
        ((mobj_t*)mo)->movefactor = ORIG_FRICTION_FACTOR;  // reset
      }
      else                                      // sludge
      {

        // phares 3/11/98: you start off slowly, then increase as
        // you get better footing

        momentum = (P_AproxDistance(mo->momx,mo->momy));
        movefactor = mo->movefactor;
        if (momentum > MORE_FRICTION_MOMENTUM<<2)
          movefactor <<= 3;

        else if (momentum > MORE_FRICTION_MOMENTUM<<1)
          movefactor <<= 2;

        else if (momentum > MORE_FRICTION_MOMENTUM)
          movefactor <<= 1;

        ((mobj_t*)mo)->movefactor = ORIG_FRICTION_FACTOR;  // reset
      }
    }                                                       //     ^

    return(movefactor);                                       //     |
  }

  // If the floor is icy or muddy, it's harder to get moving. This is where
  // the different friction factors are applied to 'trying to move'. In
  // p_mobj.c, the friction factors are applied as you coast and slow down.

  if ((friction = P_GetFriction(mo, &movefactor)) < ORIG_FRICTION)
    {
      // phares 3/11/98: you start off slowly, then increase as
      // you get better footing

     int momentum = P_AproxDistance(mo->momx,mo->momy);

     if (momentum > MORE_FRICTION_MOMENTUM<<2)
       movefactor <<= 3;
     else if (momentum > MORE_FRICTION_MOMENTUM<<1)
       movefactor <<= 2;
     else if (momentum > MORE_FRICTION_MOMENTUM)
       movefactor <<= 1;
    }

  if (frictionp)
    *frictionp = friction;

  return movefactor;
}

//
// P_TeleportMove
//

// killough 8/9/98
boolean P_TeleportMove(mobj_t *thing, fixed_t x, fixed_t y, boolean boss)
{
  int xl, xh, yl, yh, bx, by;
  subsector_t *newsubsec;

  // killough 8/9/98: make telefragging more consistent, preserve compatibility
  telefrag = thing->player || 
    (comp[comp_telefrag] || demo_version < DV_MBF ? gamemap==30 : boss);

  // kill anything occupying the position

  tmthing = thing;
  tmflags = thing->flags;

  tmx = x;
  tmy = y;

  tmbbox[BOXTOP] = y + tmthing->radius;
  tmbbox[BOXBOTTOM] = y - tmthing->radius;
  tmbbox[BOXRIGHT] = x + tmthing->radius;
  tmbbox[BOXLEFT] = x - tmthing->radius;

  newsubsec = R_PointInSubsector (x,y);
  ceilingline = NULL;

  // The base floor/ceiling is from the subsector
  // that contains the point.
  // Any contacted lines the step closer together
  // will adjust them.

  tmfloorz = tmdropoffz = newsubsec->sector->floorheight;
  tmceilingz = newsubsec->sector->ceilingheight;

  validcount++;
  numspechit = 0;

  // stomp on any things contacted

  xl = (tmbbox[BOXLEFT] - bmaporgx - MAXRADIUS)>>MAPBLOCKSHIFT;
  xh = (tmbbox[BOXRIGHT] - bmaporgx + MAXRADIUS)>>MAPBLOCKSHIFT;
  yl = (tmbbox[BOXBOTTOM] - bmaporgy - MAXRADIUS)>>MAPBLOCKSHIFT;
  yh = (tmbbox[BOXTOP] - bmaporgy + MAXRADIUS)>>MAPBLOCKSHIFT;

  for (bx=xl ; bx<=xh ; bx++)
    for (by=yl ; by<=yh ; by++)
      if (!P_BlockThingsIterator(bx, by, PIT_StompThing, true))
        return false;

  // the move is ok,
  // so unlink from the old position & link into the new position

  P_UnsetThingPosition(thing);

  thing->floorz = tmfloorz;
  thing->ceilingz = tmceilingz;
  thing->dropoffz = tmdropoffz;        // killough 11/98

  thing->x = x;
  thing->y = y;

  // [AM] Don't interpolate mobjs that pass
  //      through teleporters
  thing->interp = false;

  P_SetThingPosition(thing);

  return true;
}

//
// MOVEMENT ITERATOR FUNCTIONS
//

//                                                                  // phares
// PIT_CrossLine                                                    //   |
// Checks to see if a PE->LS trajectory line crosses a blocking     //   V
// line. Returns false if it does.
//
// tmbbox holds the bounding box of the trajectory. If that box
// does not touch the bounding box of the line in question,
// then the trajectory is not blocked. If the PE is on one side
// of the line and the LS is on the other side, then the
// trajectory is blocked.
//
// Currently this assumes an infinite line, which is not quite
// correct. A more correct solution would be to check for an
// intersection of the trajectory and the line, but that takes
// longer and probably really isn't worth the effort.
//
// killough 11/98: reformatted

static boolean PIT_CrossLine(line_t *ld)
{
  return 
    !((ld->flags ^ ML_TWOSIDED) & (ML_TWOSIDED|ML_BLOCKING|ML_BLOCKMONSTERS))
    || tmbbox[BOXLEFT]   > ld->bbox[BOXRIGHT]
    || tmbbox[BOXRIGHT]  < ld->bbox[BOXLEFT]   
    || tmbbox[BOXTOP]    < ld->bbox[BOXBOTTOM]
    || tmbbox[BOXBOTTOM] > ld->bbox[BOXTOP]
    || P_PointOnLineSide(pe_x,pe_y,ld) == P_PointOnLineSide(ls_x,ls_y,ld);
}

// killough 8/1/98: used to test intersection between thing and line
// assuming NO movement occurs -- used to avoid sticky situations.

static int untouched(line_t *ld)
{
  fixed_t x, y, tmbbox[4];
  return 
    (tmbbox[BOXRIGHT] = (x=tmthing->x)+tmthing->radius) <= ld->bbox[BOXLEFT] ||
    (tmbbox[BOXLEFT] = x-tmthing->radius) >= ld->bbox[BOXRIGHT] ||
    (tmbbox[BOXTOP] = (y=tmthing->y)+tmthing->radius) <= ld->bbox[BOXBOTTOM] ||
    (tmbbox[BOXBOTTOM] = y-tmthing->radius) >= ld->bbox[BOXTOP] ||
    P_BoxOnLineSide(tmbbox, ld) != -1;
}

//
// PIT_CheckLine
// Adjusts tmfloorz and tmceilingz as lines are contacted
//

// [FG] SPECHITS overflow emulation from Chocolate Doom / PrBoom+

#define MAXSPECIALCROSS_ORIGINAL 8
#define DEFAULT_SPECHIT_MAGIC 0x01C09C98
static void SpechitOverrun(line_t *ld);

static boolean PIT_CheckLine(line_t *ld) // killough 3/26/98: make static
{
  if (tmbbox[BOXRIGHT] <= ld->bbox[BOXLEFT]
      || tmbbox[BOXLEFT] >= ld->bbox[BOXRIGHT]
      || tmbbox[BOXTOP] <= ld->bbox[BOXBOTTOM]
      || tmbbox[BOXBOTTOM] >= ld->bbox[BOXTOP] )
    return true; // didn't hit it

  if (P_BoxOnLineSide(tmbbox, ld) != -1)
    return true; // didn't hit it

  // A line has been hit

  // The moving thing's destination position will cross the given line.
  // If this should not be allowed, return false.
  // If the line is special, keep track of it
  // to process later if the move is proven ok.
  // NOTE: specials are NOT sorted by order,
  // so two special lines that are only 8 pixels apart
  // could be crossed in either order.

  // killough 7/24/98: allow player to move out of 1s wall, to prevent sticking
  if (!ld->backsector) // one sided line
    {
      blockline = ld;
      return tmunstuck && !untouched(ld) &&
	FixedMul(tmx-tmthing->x,ld->dy) > FixedMul(tmy-tmthing->y,ld->dx);
    }

  // killough 8/10/98: allow bouncing objects to pass through as missiles
  if (!(tmthing->flags & (MF_MISSILE | MF_BOUNCES)))
    {
      // explicitly blocking everything
      // or blocking player
      if (ld->flags & ML_BLOCKING || (mbf21 && tmthing->player && ld->flags & ML_BLOCKPLAYERS))
	return tmunstuck && !untouched(ld);  // killough 8/1/98: allow escape

      // killough 8/9/98: monster-blockers don't affect friends
      if (!(tmthing->flags & MF_FRIEND || tmthing->player)
	  &&
	  (
	    ld->flags & ML_BLOCKMONSTERS ||
	    (mbf21 && ld->flags & ML_BLOCKLANDMONSTERS && !(tmthing->flags & MF_FLOAT))
	  )
	 )
	return false; // block monsters only
    }

  // set openrange, opentop, openbottom
  // these define a 'window' from one sector to another across this line

  P_LineOpening (ld);

  // adjust floor & ceiling heights

  if (opentop < tmceilingz)
    {
      tmceilingz = opentop;
      ceilingline = ld;
      blockline = ld;
    }

  if (openbottom > tmfloorz)
    {
      tmfloorz = openbottom;
      floorline = ld;          // killough 8/1/98: remember floor linedef
      blockline = ld;
    }

  if (lowfloor < tmdropoffz)
    tmdropoffz = lowfloor;

  // if contacted a special line, add it to the list

  if (ld->special)
    {
      // 1/11/98 killough: remove limit on lines hit, by array doubling
      if (numspechit >= spechit_max)
	{
	  spechit_max = spechit_max ? spechit_max*2 : 8;
	  spechit = Z_Realloc(spechit,sizeof *spechit*spechit_max,PU_STATIC,0); // killough
	}
      spechit[numspechit++] = ld;

      // [FG] SPECHITS overflow emulation from Chocolate Doom / PrBoom+
      if (numspechit > MAXSPECIALCROSS_ORIGINAL && demo_compatibility
          && overflow[emu_spechits].enabled)
	{
	  if (numspechit == MAXSPECIALCROSS_ORIGINAL + 1)
	  {
	    overflow[emu_spechits].triggered = true;
	    I_Printf(VB_WARNING, "PIT_CheckLine: Triggered SPECHITS overflow!");
	  }
	  SpechitOverrun(ld);
	}
    }

  return true;
}

//
// PIT_CheckThing
//

boolean hangsolid;

// mbf21: dehacked projectile groups
static boolean P_ProjectileImmune(mobj_t *target, mobj_t *source)
{
  return
    ( // PG_GROUPLESS means no immunity, even to own species
      mobjinfo[target->type].projectile_group != PG_GROUPLESS ||
      target == source
    ) &&
    (
      ( // target type has default behaviour, and things are the same type
        mobjinfo[target->type].projectile_group == PG_DEFAULT &&
        source->type == target->type
      ) ||
      ( // target type has special behaviour, and things have the same group
        mobjinfo[target->type].projectile_group != PG_DEFAULT &&
        mobjinfo[target->type].projectile_group == mobjinfo[source->type].projectile_group
      )
    );
}

// [FG] mobj or actual sprite height
static const inline fixed_t thingheight (const mobj_t *const thing, const mobj_t *const cond)
{
  return (direct_vertical_aiming && cond && cond->player && thing->actualheight > thing->height) ?
        thing->actualheight : thing->height;
}

static boolean PIT_CheckThing(mobj_t *thing) // killough 3/26/98: make static
{
  fixed_t blockdist;
  int damage;

  // killough 11/98: add touchy things
  if (!(thing->flags & (MF_SOLID|MF_SPECIAL|MF_SHOOTABLE|MF_TOUCHY)))
    return true;

  blockdist = thing->radius + tmthing->radius;

  if (abs(thing->x - tmx) >= blockdist || abs(thing->y - tmy) >= blockdist)
    return true; // didn't hit it

  // killough 11/98:
  //
  // This test has less information content (it's almost always false), so it
  // should not be moved up to first, as it adds more overhead than it removes.

  // don't clip against self

  if (thing == tmthing)
    return true;

  // killough 11/98:
  //
  // TOUCHY flag, for mines or other objects which die on contact with solids.
  // If a solid object of a different type comes in contact with a touchy
  // thing, and the touchy thing is not the sole one moving relative to fixed
  // surroundings such as walls, then the touchy thing dies immediately.

  if (thing->flags & MF_TOUCHY &&                  // touchy object
      tmthing->flags & MF_SOLID &&                 // solid object touches it
      thing->health > 0 &&                         // touchy object is alive
      (thing->intflags & MIF_ARMED ||              // Thing is an armed mine
       sentient(thing)) &&                         // ... or a sentient thing
      (thing->type != tmthing->type ||             // only different species
       thing->type == MT_PLAYER) &&                // ... or different players
      thing->z + thing->height >= tmthing->z &&    // touches vertically
      tmthing->z + tmthing->height >= thing->z &&
      (thing->type ^ MT_PAIN) |                    // PEs and lost souls
      (tmthing->type ^ MT_SKULL) &&                // are considered same
      (thing->type ^ MT_SKULL) |                   // (but Barons & Knights
      (tmthing->type ^ MT_PAIN))                   // are intentionally not)
    {
      P_DamageMobj(thing, NULL, NULL, thing->health);  // kill object
      return true;
    }

  // check for skulls slamming into things

  if (tmthing->flags & MF_SKULLFLY)
    {
      // A flying skull is smacking something.
      // Determine damage amount, and the skull comes to a dead stop.

      int damage = ((P_Random(pr_skullfly)%8)+1)*tmthing->info->damage;

      P_DamageMobj (thing, tmthing, tmthing, damage);

      tmthing->flags &= ~MF_SKULLFLY;
      tmthing->momx = tmthing->momy = tmthing->momz = 0;

      P_SetMobjState (tmthing, tmthing->info->spawnstate);

      return false;   // stop moving
    }

  // missiles can hit other things
  // killough 8/10/98: bouncing non-solid things can hit other things too

  if (tmthing->flags & MF_MISSILE || (tmthing->flags & MF_BOUNCES &&
				      !(tmthing->flags & MF_SOLID)))
    {
      // see if it went over / under

      if (tmthing->z > thing->z + thingheight(thing, tmthing->target))
	return true;    // overhead

      if (tmthing->z+tmthing->height < thing->z)
	return true;    // underneath

      if (tmthing->target && P_ProjectileImmune(thing, tmthing->target))
      {
	if (thing == tmthing->target)
	  return true;                // Don't hit same species as originator.
	else
	  // Dehacked support - monsters infight
	  if (thing->type != MT_PLAYER && !deh_species_infighting) // Explode, but do no damage.
	    return false;	        // Let players missile other players.
      }
      
      // killough 8/10/98: if moving thing is not a missile, no damage
      // is inflicted, and momentum is reduced if object hit is solid.

      if (!(tmthing->flags & MF_MISSILE))
      {
	if (!(thing->flags & MF_SOLID))
	  return true;
	else
	  {
	    tmthing->momx = -tmthing->momx;
	    tmthing->momy = -tmthing->momy;
	    if (!(tmthing->flags & MF_NOGRAVITY))
	      {
		tmthing->momx >>= 2;
		tmthing->momy >>= 2;
	      }
	    return false;
	  }
      }

      if (!(thing->flags & MF_SHOOTABLE))
	return !(thing->flags & MF_SOLID); // didn't do any damage

      // mbf21: ripper projectile
      if (tmthing->flags2 & MF2_RIP)
      {
        damage = ((P_Random(pr_mbf21) & 3) + 2) * tmthing->info->damage;
        if (!(thing->flags & MF_NOBLOOD))
          P_SpawnBlood(tmthing->x, tmthing->y, tmthing->z, damage, thing);
        if (tmthing->info->ripsound)
          S_StartSound(tmthing, tmthing->info->ripsound);

        P_DamageMobj(thing, tmthing, tmthing->target, damage);

        numspechit = 0;
        return true;
      }

      // damage / explode

      damage = ((P_Random(pr_damage)%8)+1)*tmthing->info->damage;
      P_DamageMobj (thing, tmthing, tmthing->target, damage);

      // don't traverse any more
      return false;
    }

  // check for special pickup

  if (thing->flags & MF_SPECIAL)
    {
      int solid = thing->flags & MF_SOLID;
      if (tmflags & MF_PICKUP)
	P_TouchSpecialThing(thing, tmthing); // can remove thing
      return !solid;
    }

  // RjY
  // comperr_hangsolid, an attempt to handle blocking hanging bodies
  // A solid hanging body will allow sufficiently small things underneath it.
  if (CRITICAL(hangsolid) &&
      !((~thing->flags) & (MF_SOLID | MF_SPAWNCEILING)) // solid and hanging
      // invert everything, then both bits should be clear
      && tmthing->z + tmthing->height <= thing->z) // head height <= base
      // top of thing trying to move under the body <= bottom of body
  {
    tmceilingz = thing->z; // pretend ceiling height is at body's base
    return true;
  }

  // killough 3/16/98: Allow non-solid moving objects to move through solid
  // ones, by allowing the moving thing (tmthing) to move if it's non-solid,
  // despite another solid thing being in the way.
  // killough 4/11/98: Treat no-clipping things as not blocking

  return !((thing->flags & MF_SOLID && !(thing->flags & MF_NOCLIP))
           && (tmthing->flags & MF_SOLID || demo_compatibility));

  // return !(thing->flags & MF_SOLID);   // old code -- killough
}


// This routine checks for Lost Souls trying to be spawned      // phares
// across 1-sided lines, impassible lines, or "monsters can't   //   |
// cross" lines. Draw an imaginary line between the PE          //   V
// and the new Lost Soul spawn spot. If that line crosses
// a 'blocking' line, then disallow the spawn. Only search
// lines in the blocks of the blockmap where the bounding box
// of the trajectory line resides. Then check bounding box
// of the trajectory vs. the bounding box of each blocking
// line to see if the trajectory and the blocking line cross.
// Then check the PE and LS to see if they're on different
// sides of the blocking line. If so, return true, otherwise
// false.

boolean Check_Sides(mobj_t *actor, int x, int y)
{
  int bx,by,xl,xh,yl,yh;

  pe_x = actor->x;
  pe_y = actor->y;
  ls_x = x;
  ls_y = y;

  // Here is the bounding box of the trajectory

  tmbbox[BOXLEFT]   = pe_x < x ? pe_x : x;
  tmbbox[BOXRIGHT]  = pe_x > x ? pe_x : x;
  tmbbox[BOXTOP]    = pe_y > y ? pe_y : y;
  tmbbox[BOXBOTTOM] = pe_y < y ? pe_y : y;

  // Determine which blocks to look in for blocking lines

  xl = (tmbbox[BOXLEFT]   - bmaporgx)>>MAPBLOCKSHIFT;
  xh = (tmbbox[BOXRIGHT]  - bmaporgx)>>MAPBLOCKSHIFT;
  yl = (tmbbox[BOXBOTTOM] - bmaporgy)>>MAPBLOCKSHIFT;
  yh = (tmbbox[BOXTOP]    - bmaporgy)>>MAPBLOCKSHIFT;

  // xl->xh, yl->yh determine the mapblock set to search

  validcount++; // prevents checking same line twice
  for (bx = xl ; bx <= xh ; bx++)
    for (by = yl ; by <= yh ; by++)
      if (!P_BlockLinesIterator(bx,by,PIT_CrossLine))
        return true;                                                //   ^
  return(false);                                                    //   |
}                                                                 // phares

//
// MOVEMENT CLIPPING
//

//
// P_CheckPosition
// This is purely informative, nothing is modified
// (except things picked up).
//
// in:
//  a mobj_t (can be valid or invalid)
//  a position to be checked
//   (doesn't need to be related to the mobj_t->x,y)
//
// during:
//  special things are touched if MF_PICKUP
//  early out on solid lines?
//
// out:
//  newsubsec
//  floorz
//  ceilingz
//  tmdropoffz
//   the lowest point contacted
//   (monsters won't move to a dropoff)
//  speciallines[]
//  numspeciallines
//

boolean P_CheckPosition(mobj_t *thing, fixed_t x, fixed_t y) 
{
  int xl, xh, yl, yh, bx, by;
  subsector_t *newsubsec;

  tmthing = thing;
  tmflags = thing->flags;

  tmx = x;
  tmy = y;

  tmbbox[BOXTOP] = y + tmthing->radius;
  tmbbox[BOXBOTTOM] = y - tmthing->radius;
  tmbbox[BOXRIGHT] = x + tmthing->radius;
  tmbbox[BOXLEFT] = x - tmthing->radius;

  newsubsec = R_PointInSubsector(x,y);
  floorline = blockline = ceilingline = NULL; // killough 8/1/98

  // Whether object can get out of a sticky situation:
  tmunstuck = thing->player &&          // only players
    thing->player->mo == thing &&       // not voodoo dolls
    demo_version >= DV_MBF;             // not under old demos

  // The base floor / ceiling is from the subsector
  // that contains the point.
  // Any contacted lines the step closer together
  // will adjust them.

  tmfloorz = tmdropoffz = newsubsec->sector->floorheight;
  tmceilingz = newsubsec->sector->ceilingheight;
  validcount++;
  numspechit = 0;

  if (tmflags & MF_NOCLIP)
    return true;

  // Check things first, possibly picking things up.
  // The bounding box is extended by MAXRADIUS
  // because mobj_ts are grouped into mapblocks
  // based on their origin point, and can overlap
  // into adjacent blocks by up to MAXRADIUS units.

  xl = (tmbbox[BOXLEFT] - bmaporgx - MAXRADIUS)>>MAPBLOCKSHIFT;
  xh = (tmbbox[BOXRIGHT] - bmaporgx + MAXRADIUS)>>MAPBLOCKSHIFT;
  yl = (tmbbox[BOXBOTTOM] - bmaporgy - MAXRADIUS)>>MAPBLOCKSHIFT;
  yh = (tmbbox[BOXTOP] - bmaporgy + MAXRADIUS)>>MAPBLOCKSHIFT;

  for (bx=xl ; bx<=xh ; bx++)
    for (by=yl ; by<=yh ; by++)
      if (!P_BlockThingsIterator(bx, by, PIT_CheckThing, !(tmthing->flags2 & MF2_RIP)))
        return false;

  // check lines

  xl = (tmbbox[BOXLEFT] - bmaporgx)>>MAPBLOCKSHIFT;
  xh = (tmbbox[BOXRIGHT] - bmaporgx)>>MAPBLOCKSHIFT;
  yl = (tmbbox[BOXBOTTOM] - bmaporgy)>>MAPBLOCKSHIFT;
  yh = (tmbbox[BOXTOP] - bmaporgy)>>MAPBLOCKSHIFT;

  // mbf21: Basically, in vanilla doom, this variable is incremented in the wrong position 
  // in P_CheckPosition. The explanation for why this is a problem is complicated, but
  // ripper projectiles (and possibly other cases) will expose this bug and cause desyncs.
  // I recommend adding an extra validcount increment in P_CheckPosition before running 
  // the P_BlockLinesIterator.
  if (mbf21)
  {
    validcount++;
  }
  for (bx=xl ; bx<=xh ; bx++)
    for (by=yl ; by<=yh ; by++)
      if (!P_BlockLinesIterator(bx,by,PIT_CheckLine))
        return false; // doesn't fit

  return true;
}

//
// P_TryMove
// Attempt to move to a new position,
// crossing special lines unless MF_TELEPORT is set.
//
// killough 3/15/98: allow dropoff as option

boolean P_TryMove(mobj_t *thing, fixed_t x, fixed_t y, int dropoff)
{
  fixed_t oldx, oldy;

  felldown = floatok = false;               // killough 11/98

  if (!P_CheckPosition(thing, x, y))
    return false;   // solid wall or thing

  if (!(thing->flags & MF_NOCLIP))
    {
      // killough 7/26/98: reformatted slightly
      // killough 8/1/98: Possibly allow escape if otherwise stuck

      if (tmceilingz - tmfloorz < thing->height ||     // doesn't fit
	  // mobj must lower to fit
	  (floatok = true, !(thing->flags & MF_TELEPORT) &&
	   tmceilingz - thing->z < thing->height) ||
	  // too big a step up
	  (!(thing->flags & MF_TELEPORT) && 
	   tmfloorz - thing->z > 24*FRACUNIT))
	return tmunstuck 
	  && !(ceilingline && untouched(ceilingline))
	  && !(  floorline && untouched(  floorline));
      
      // killough 3/15/98: Allow certain objects to drop off
      // killough 7/24/98, 8/1/98: 
      // Prevent monsters from getting stuck hanging off ledges
      // killough 10/98: Allow dropoffs in controlled circumstances
      // killough 11/98: Improve symmetry of clipping on stairs

      if (!(thing->flags & (MF_DROPOFF|MF_FLOAT)))
      {
        boolean ledgeblock = comp[comp_ledgeblock] &&
                            !(mbf21 && thing->intflags & MIF_SCROLLING);

	if (comp[comp_dropoff] || ledgeblock)
	  {
	    if ((ledgeblock || !dropoff) &&
		tmfloorz - tmdropoffz > 24*FRACUNIT)
	      return false;                      // don't stand over a dropoff
	  }
	else
	  if (!dropoff || (dropoff==2 &&  // large jump down (e.g. dogs)
			   (tmfloorz-tmdropoffz > 128*FRACUNIT || 
			    !thing->target || thing->target->z >tmdropoffz)))
	    {
	      if (!monkeys || demo_version < DV_MBF ?
		  tmfloorz - tmdropoffz > 24*FRACUNIT :
		  thing->floorz  - tmfloorz > 24*FRACUNIT ||
		  thing->dropoffz - tmdropoffz > 24*FRACUNIT)
		return false;
	    }
	  else  // dropoff allowed -- check for whether it fell more than 24
	    felldown = !(thing->flags & MF_NOGRAVITY) &&
	      thing->z - tmfloorz > 24*FRACUNIT;
      }

      if (thing->flags & MF_BOUNCES &&    // killough 8/13/98
	  !(thing->flags & (MF_MISSILE|MF_NOGRAVITY)) &&
	  !sentient(thing) && tmfloorz - thing->z > 16*FRACUNIT)
	return false; // too big a step up for bouncers under gravity

      // killough 11/98: prevent falling objects from going up too many steps
      if (thing->intflags & MIF_FALLING && tmfloorz - thing->z >
	  FixedMul(thing->momx,thing->momx)+FixedMul(thing->momy,thing->momy))
	return false;
    }

  // the move is ok,
  // so unlink from the old position and link into the new position

  P_UnsetThingPosition (thing);

  oldx = thing->x;
  oldy = thing->y;
  thing->floorz = tmfloorz;
  thing->ceilingz = tmceilingz;
  thing->dropoffz = tmdropoffz;      // killough 11/98: keep track of dropoffs
  thing->x = x;
  thing->y = y;

  P_SetThingPosition(thing);

  // if any special lines were hit, do the effect
  // killough 11/98: simplified

  if (!(thing->flags & (MF_TELEPORT | MF_NOCLIP)))
  {
    while (numspechit--)
      if (spechit[numspechit]->special)  // see if the line was crossed
	{
	  int oldside;
	  if ((oldside = P_PointOnLineSide(oldx, oldy, spechit[numspechit])) !=
	      P_PointOnLineSide(thing->x, thing->y, spechit[numspechit]))
	    P_CrossSpecialLine(spechit[numspechit], oldside, thing, false);
	}
    // There are checks elsewhere for numspechit == 0, so we don't want to
    // leave numspechit == -1.
    numspechit = 0;
  }

  return true;
}

//
// killough 9/12/98:
//
// Apply "torque" to objects hanging off of ledges, so that they
// fall off. It's not really torque, since Doom has no concept of
// rotation, but it's a convincing effect which avoids anomalies
// such as lifeless objects hanging more than halfway off of ledges,
// and allows objects to roll off of the edges of moving lifts, or
// to slide up and then back down stairs, or to fall into a ditch.
// If more than one linedef is contacted, the effects are cumulative,
// so balancing is possible.
//

static boolean PIT_ApplyTorque(line_t *ld)
{
  if (ld->backsector &&       // If thing touches two-sided pivot linedef
      (ld->dx || ld->dy) && // Torque is undefined if the line has no length
      tmbbox[BOXRIGHT]  > ld->bbox[BOXLEFT]  &&
      tmbbox[BOXLEFT]   < ld->bbox[BOXRIGHT] &&
      tmbbox[BOXTOP]    > ld->bbox[BOXBOTTOM] &&
      tmbbox[BOXBOTTOM] < ld->bbox[BOXTOP] &&
      P_BoxOnLineSide(tmbbox, ld) == -1)
    {
      mobj_t *mo = tmthing;

      fixed_t dist =                               // lever arm
	+ (ld->dx >> FRACBITS) * (mo->y >> FRACBITS)
	- (ld->dy >> FRACBITS) * (mo->x >> FRACBITS) 
	- (ld->dx >> FRACBITS) * (ld->v1->y >> FRACBITS)
	+ (ld->dy >> FRACBITS) * (ld->v1->x >> FRACBITS);

      if (dist < 0 ?                               // dropoff direction
	  ld->frontsector->floorheight < mo->z &&
	  ld->backsector->floorheight >= mo->z :
          ld->backsector->floorheight < mo->z &&
          ld->frontsector->floorheight >= mo->z)
	{
	  // At this point, we know that the object straddles a two-sided
	  // linedef, and that the object's center of mass is above-ground.

	  fixed_t x = abs(ld->dx), y = abs(ld->dy);

	  if (y > x)
	    {
	      fixed_t t = x;
	      x = y;
	      y = t;
	    }

	  y = finesine[(tantoangle[FixedDiv(y,x)>>DBITS] +
			ANG90) >> ANGLETOFINESHIFT];

	  // Momentum is proportional to distance between the
	  // object's center of mass and the pivot linedef.
	  //
	  // It is scaled by 2^(OVERDRIVE - gear). When gear is
	  // increased, the momentum gradually decreases to 0 for
	  // the same amount of pseudotorque, so that oscillations
	  // are prevented, yet it has a chance to reach equilibrium.

	  dist = FixedDiv(FixedMul(dist, (mo->gear < OVERDRIVE) ?
				   y << -(mo->gear - OVERDRIVE) :
				   y >> +(mo->gear - OVERDRIVE)), x);

	  // Apply momentum away from the pivot linedef.
	  
	  x = FixedMul(ld->dy, dist);
	  y = FixedMul(ld->dx, dist);

	  // Avoid moving too fast all of a sudden (step into "overdrive")

	  dist = FixedMul(x,x) + FixedMul(y,y);

	  while (dist > FRACUNIT*4 && mo->gear < MAXGEAR)
	    ++mo->gear, x >>= 1, y >>= 1, dist >>= 1;
	  
	  mo->momx -= x;
	  mo->momy += y;
	}
    }
  return true;
}

//
// killough 9/12/98
//
// Applies "torque" to objects, based on all contacted linedefs
//

void P_ApplyTorque(mobj_t *mo)
{
  int xl = ((tmbbox[BOXLEFT] = 
	     mo->x - mo->radius) - bmaporgx) >> MAPBLOCKSHIFT;
  int xh = ((tmbbox[BOXRIGHT] = 
	     mo->x + mo->radius) - bmaporgx) >> MAPBLOCKSHIFT;
  int yl = ((tmbbox[BOXBOTTOM] =
	     mo->y - mo->radius) - bmaporgy) >> MAPBLOCKSHIFT;
  int yh = ((tmbbox[BOXTOP] = 
	     mo->y + mo->radius) - bmaporgy) >> MAPBLOCKSHIFT;
  int bx,by,flags = mo->intflags; //Remember the current state, for gear-change

  tmthing = mo;
  validcount++; // prevents checking same line twice
      
  for (bx = xl ; bx <= xh ; bx++)
    for (by = yl ; by <= yh ; by++)
      P_BlockLinesIterator(bx, by, PIT_ApplyTorque);
      
  // If any momentum, mark object as 'falling' using engine-internal flags
  if (mo->momx | mo->momy)
    mo->intflags |= MIF_FALLING;
  else  // Clear the engine-internal flag indicating falling object.
    mo->intflags &= ~MIF_FALLING;

  // If the object has been moving, step up the gear.
  // This helps reach equilibrium and avoid oscillations.
  //
  // Doom has no concept of potential energy, much less
  // of rotation, so we have to creatively simulate these 
  // systems somehow :)

  if (!((mo->intflags | flags) & MIF_FALLING))   // If not falling for a while,
    mo->gear = 0;                                // Reset it to full strength
  else
    if (mo->gear < MAXGEAR)                      // Else if not at max gear,
      mo->gear++;                                // move up a gear
}

//
// P_ThingHeightClip
// Takes a valid thing and adjusts the thing->floorz,
// thing->ceilingz, and possibly thing->z.
// This is called for all nearby monsters
// whenever a sector changes height.
// If the thing doesn't fit,
// the z will be set to the lowest value
// and false will be returned.
//

static boolean P_ThingHeightClip(mobj_t *thing)
{
  boolean onfloor = thing->z == thing->floorz;

  P_CheckPosition(thing, thing->x, thing->y);

  // what about stranding a monster partially off an edge?
  // killough 11/98: Answer: see below (upset balance if hanging off ledge)

  thing->floorz = tmfloorz;
  thing->ceilingz = tmceilingz;
  thing->dropoffz = tmdropoffz;         // killough 11/98: remember dropoffs

  if (onfloor)  // walking monsters rise and fall with the floor
    {
      thing->z = thing->floorz;

      // killough 11/98: Possibly upset balance of objects hanging off ledges
      if (thing->intflags & MIF_FALLING && thing->gear >= MAXGEAR)
	thing->gear = 0;
    }
  else          // don't adjust a floating monster unless forced to
    if (thing->z + thing->height > thing->ceilingz)
      thing->z = thing->ceilingz - thing->height;

  return thing->ceilingz - thing->floorz >= thing->height;
}

//
// SLIDE MOVE
// Allows the player to slide along any angled walls.
//

// killough 8/2/98: make variables static
static fixed_t   bestslidefrac;
static fixed_t   secondslidefrac;
static line_t    *bestslideline;
static line_t    *secondslideline;
static mobj_t    *slidemo;
static fixed_t   tmxmove;
static fixed_t   tmymove;

//
// P_HitSlideLine
// Adjusts the xmove / ymove
// so that the next move will slide along the wall.
//
// If the floor is icy, then you can bounce off a wall.             // phares
//

static void P_HitSlideLine(line_t *ld)
{
  int     side;
  angle_t lineangle;
  angle_t moveangle;
  angle_t deltaangle;
  fixed_t movelen;
  fixed_t newlen;
  boolean icyfloor;  // is floor icy?

  // phares:
  // Under icy conditions, if the angle of approach to the wall
  // is more than 45 degrees, then you'll bounce and lose half
  // your momentum. If less than 45 degrees, you'll slide along
  // the wall. 45 is arbitrary and is believable.
  //
  // Check for the special cases of horz or vert walls.

  // killough 10/98: only bounce if hit hard (prevents wobbling)

  if (demo_version >= DV_MBF)
  {
  icyfloor = 
     P_AproxDistance(tmxmove, tmymove) > 4*FRACUNIT &&
    variable_friction &&  // killough 8/28/98: calc friction on demand
    slidemo->z <= slidemo->floorz &&
    P_GetFriction(slidemo, NULL) > ORIG_FRICTION;
  }
  else
  {
    icyfloor = !compatibility &&
    variable_friction &&
    slidemo->player &&
    onground &&
    slidemo->friction > ORIG_FRICTION;
  }

  if (ld->slopetype == ST_HORIZONTAL)
    {
      if (icyfloor && abs(tmymove) > abs(tmxmove))
	{
	  S_StartSoundPreset(slidemo, sfx_oof, PITCH_FULL); // oooff!
	  tmxmove /= 2; // absorb half the momentum
	  tmymove = -tmymove/2;
	}
      else
	tmymove = 0; // no more movement in the Y direction
      return;
    }

  if (ld->slopetype == ST_VERTICAL)
    {
      if (icyfloor && abs(tmxmove) > abs(tmymove))
	{
	  S_StartSoundPreset(slidemo, sfx_oof, PITCH_FULL); // oooff!
	  tmxmove = -tmxmove/2; // absorb half the momentum
	  tmymove /= 2;
	}
      else                                                        // phares
	tmxmove = 0; // no more movement in the X direction
      return;
    }

  // The wall is angled. Bounce if the angle of approach is  // phares
  // less than 45 degrees.

  side = P_PointOnLineSide (slidemo->x, slidemo->y, ld);
  
  lineangle = R_PointToAngle2 (0,0, ld->dx, ld->dy);
  if (side == 1)
    lineangle += ANG180;
  moveangle = R_PointToAngle2 (0,0, tmxmove, tmymove);

  // killough 3/2/98:
  // The moveangle+=10 breaks v1.9 demo compatibility in
  // some demos, so it needs demo_compatibility switch.

  if (!demo_compatibility)
    moveangle += 10;
  // ^ prevents sudden path reversal due to rounding error // phares

  deltaangle = moveangle-lineangle;
  movelen = P_AproxDistance (tmxmove, tmymove);

  if (icyfloor && deltaangle > ANG45 && deltaangle < ANG90+ANG45)
    {
      S_StartSoundPreset(slidemo, sfx_oof, PITCH_FULL); // oooff!
      moveangle = lineangle - deltaangle;
      movelen /= 2; // absorb
      moveangle >>= ANGLETOFINESHIFT;
      tmxmove = FixedMul (movelen, finecosine[moveangle]);
      tmymove = FixedMul (movelen, finesine[moveangle]);
    }
  else
    {
      if (deltaangle > ANG180)
	deltaangle += ANG180;

      //  I_Error ("ang>ANG180");

      lineangle >>= ANGLETOFINESHIFT;
      deltaangle >>= ANGLETOFINESHIFT;
      newlen = FixedMul (movelen, finecosine[deltaangle]);
      tmxmove = FixedMul (newlen, finecosine[lineangle]);
      tmymove = FixedMul (newlen, finesine[lineangle]);
    }
}


//
// PTR_SlideTraverse
//

static boolean PTR_SlideTraverse(intercept_t *in)
{
  line_t *li;

#ifdef RANGECHECK
  if (!in->isaline)
    I_Error ("not a line?");
#endif

  li = in->d.line;

  if (!(li->flags & ML_TWOSIDED))
    {
      if (P_PointOnLineSide (slidemo->x, slidemo->y, li))
	return true; // don't hit the back side
      goto isblocking;
    }

  // set openrange, opentop, openbottom.
  // These define a 'window' from one sector to another across a line

  P_LineOpening(li);

  if (openrange < slidemo->height)
    goto isblocking;  // doesn't fit

  if (opentop - slidemo->z < slidemo->height)
    goto isblocking;  // mobj is too high

  if (openbottom - slidemo->z > 24*FRACUNIT )
    goto isblocking;  // too big a step up

  // this line doesn't block movement

  return true;

  // the line does block movement,
  // see if it is closer than best so far

isblocking:

  if (in->frac < bestslidefrac)
    {
      secondslidefrac = bestslidefrac;
      secondslideline = bestslideline;
      bestslidefrac = in->frac;
      bestslideline = li;
    }

  return false; // stop
}

//
// P_SlideMove
// The momx / momy move is bad, so try to slide
// along a wall.
// Find the first line hit, move flush to it,
// and slide along it
//
// This is a kludgy mess.
//
// killough 11/98: reformatted

void P_SlideMove(mobj_t *mo)
{
  int hitcount = 3;

  slidemo = mo; // the object that's sliding

  do 
    {
      fixed_t leadx, leady, trailx, traily;

      if (!--hitcount)
	goto stairstep;   // don't loop forever

      // trace along the three leading corners

      if (mo->momx > 0)
	leadx = mo->x + mo->radius, trailx = mo->x - mo->radius;
      else
	leadx = mo->x - mo->radius, trailx = mo->x + mo->radius;

      if (mo->momy > 0)
	leady = mo->y + mo->radius, traily = mo->y - mo->radius;
      else
	leady = mo->y - mo->radius, traily = mo->y + mo->radius;

      bestslidefrac = FRACUNIT+1;

      P_PathTraverse(leadx, leady, leadx+mo->momx, leady+mo->momy,
		     PT_ADDLINES, PTR_SlideTraverse);
      P_PathTraverse(trailx, leady, trailx+mo->momx, leady+mo->momy,
		     PT_ADDLINES, PTR_SlideTraverse);
      P_PathTraverse(leadx, traily, leadx+mo->momx, traily+mo->momy,
		     PT_ADDLINES, PTR_SlideTraverse);

      // move up to the wall

      if (bestslidefrac == FRACUNIT+1)
	{
	  // the move must have hit the middle, so stairstep

	stairstep:

	  // killough 3/15/98: Allow objects to drop off ledges
	  //
	  // phares 5/4/98: kill momentum if you can't move at all
	  // This eliminates player bobbing if pressed against a wall
	  // while on ice.
	  //
	  // killough 10/98: keep buggy code around for old Boom demos

	  if (!P_TryMove(mo, mo->x, mo->y + mo->momy, true))
	    if (!P_TryMove(mo, mo->x + mo->momx, mo->y, true))
	      // [FG] Compatibility bug in P_SlideMove
	      // http://prboom.sourceforge.net/mbf-bugs.html
	      if (demo_version == DV_BOOM201)
		mo->momx = mo->momy = 0;

	  break;
	}

      // fudge a bit to make sure it doesn't hit
      
      if ((bestslidefrac -= 0x800) > 0)
	{
	  fixed_t newx = FixedMul(mo->momx, bestslidefrac);
	  fixed_t newy = FixedMul(mo->momy, bestslidefrac);

	  // killough 3/15/98: Allow objects to drop off ledges
	  
	  if (!P_TryMove(mo, mo->x+newx, mo->y+newy, true))
	    goto stairstep;
	}

      // Now continue along the wall.
      // First calculate remainder.

      bestslidefrac = FRACUNIT-(bestslidefrac+0x800);

      if (bestslidefrac > FRACUNIT)
	bestslidefrac = FRACUNIT;

      if (bestslidefrac <= 0)
	break;

      tmxmove = FixedMul(mo->momx, bestslidefrac);
      tmymove = FixedMul(mo->momy, bestslidefrac);

      P_HitSlideLine(bestslideline); // clip the moves

      mo->momx = tmxmove;
      mo->momy = tmymove;

      // killough 10/98: affect the bobbing the same way (but not voodoo dolls)
      if (mo->player && mo->player->mo == mo)
	{
	  if (abs(mo->player->momx) > abs(tmxmove))
	    mo->player->momx = tmxmove;
	  if (abs(mo->player->momy) > abs(tmymove))
	    mo->player->momy = tmymove;
	}
    }  // killough 3/15/98: Allow objects to drop off ledges:
  while (!P_TryMove(mo, mo->x+tmxmove, mo->y+tmymove, true));
}

//
// P_LineAttack
//
mobj_t *linetarget; // who got hit (or NULL)
static mobj_t *shootthing;

static int aim_flags_mask; // killough 8/2/98: for more intelligent autoaiming

static fixed_t shootz;  // Height if not aiming up or down
static int la_damage;
fixed_t attackrange;

static fixed_t   aimslope;

// slopes to top and bottom of target
// killough 4/20/98: make static instead of using ones in p_sight.c

static fixed_t  topslope;
static fixed_t  bottomslope;

//
// PTR_AimTraverse
// Sets linetaget and aimslope when a target is aimed at.
//
static boolean PTR_AimTraverse (intercept_t *in)
{
  fixed_t slope, thingtopslope, thingbottomslope, dist;
  line_t *li;
  mobj_t *th;

  if (in->isaline)
    {
      li = in->d.line;

      if (!(li->flags & ML_TWOSIDED))
	return false;   // stop

      // Crosses a two sided line.
      // A two sided line will restrict
      // the possible target ranges.

      P_LineOpening (li);

      if (openbottom >= opentop)
	return false;   // stop

      dist = FixedMul (attackrange, in->frac);

      // Andrey Budko: emulation of missed back side on two-sided lines.
      // backsector can be NULL when emulating missing back side.
      if (li->backsector == NULL ||
          li->frontsector->floorheight != li->backsector->floorheight)
	{
	  slope = FixedDiv (openbottom - shootz , dist);
	  if (slope > bottomslope)
	    bottomslope = slope;
	}

      if (li->backsector == NULL ||
          li->frontsector->ceilingheight != li->backsector->ceilingheight)
	{
	  slope = FixedDiv (opentop - shootz , dist);
	  if (slope < topslope)
	    topslope = slope;
	}

      if (topslope <= bottomslope)
	return false;   // stop

      return true;    // shot continues
    }

  // shoot a thing

  th = in->d.thing;
  if (th == shootthing)
    return true;    // can't shoot self

  if (!(th->flags&MF_SHOOTABLE))
    return true;    // corpse or something

  // killough 7/19/98, 8/2/98:
  // friends don't aim at friends (except players), at least not first
  if (th->flags & shootthing->flags & aim_flags_mask && !th->player)
    return true;

  // check angles to see if the thing can be aimed at

  dist = FixedMul(attackrange, in->frac);
  thingtopslope = FixedDiv(th->z+thingheight(th, shootthing) - shootz , dist);

  if (thingtopslope < bottomslope)
    return true;    // shot over the thing

  thingbottomslope = FixedDiv (th->z - shootz, dist);

  if (thingbottomslope > topslope)
    return true;    // shot under the thing

  // this thing can be hit!

  if (thingtopslope > topslope)
    thingtopslope = topslope;

  if (thingbottomslope < bottomslope)
    thingbottomslope = bottomslope;

  aimslope = (thingtopslope+thingbottomslope)/2;
  linetarget = th;

  return false;   // don't go any farther
}

//
// PTR_ShootTraverse
//
static boolean PTR_ShootTraverse(intercept_t *in)
{
  fixed_t dist, thingtopslope, thingbottomslope, x, y, z, frac;
  mobj_t *th;

  if (in->isaline)
    {
      line_t *li = in->d.line;

      if (li->special)
	{
	  int side = P_PointOnLineSide(shootthing->x, shootthing->y, li);
	  P_ShootSpecialLine(shootthing, li, side);
	}

      if (li->flags & ML_TWOSIDED)
	{  // crosses a two sided (really 2s) line
	  P_LineOpening (li);
	  dist = FixedMul(attackrange, in->frac);

	  // killough 11/98: simplify

	  // Andrey Budko: emulation of missed back side on two-sided lines.
	  // backsector can be NULL when emulating missing back side.
	  if (li->backsector == NULL)
	  {
	    if (FixedDiv(openbottom - shootz , dist) <= aimslope &&
	        FixedDiv(opentop - shootz , dist) >= aimslope)
	      return true;      // shot continues
	  }
	  else
	  if ((li->frontsector->floorheight==li->backsector->floorheight ||
	       FixedDiv(openbottom - shootz , dist) <= aimslope) &&
	      (li->frontsector->ceilingheight==li->backsector->ceilingheight ||
	       FixedDiv (opentop - shootz , dist) >= aimslope))
	    return true;      // shot continues
	}

      // hit line
      // position a bit closer

      frac = in->frac - FixedDiv(4*FRACUNIT,attackrange);
      x = trace.x + FixedMul(trace.dx, frac);
      y = trace.y + FixedMul(trace.dy, frac);
      z = shootz + FixedMul(aimslope, FixedMul(frac, attackrange));

      if (li->frontsector->ceilingpic == skyflatnum)
	{
	  // don't shoot the sky!

	  if (z > li->frontsector->ceilingheight)
	    return false;

	  // it's a sky hack wall
	  // fix bullet-eaters -- killough:
	  if  (li->backsector && li->backsector->ceilingpic == skyflatnum)
	    if (demo_compatibility || li->backsector->ceilingheight < z)
	      return false;
	}

      // [crispy] check if the pullet puff's z-coordinate is below or above
      // its spawning sector's floor or ceiling, respectively, and move its
      // coordinates to the point where the trajectory hits the plane
      if (direct_vertical_aiming && aimslope)
      {
        const int lineside = P_PointOnLineSide(x, y, li);
        int side;

        if ((side = li->sidenum[lineside]) != NO_INDEX)
        {
          const sector_t *const sector = sides[side].sector;

          if (z < sector->floorheight ||
             (z > sector->ceilingheight && sector->ceilingpic != skyflatnum))
          {
            z = CLAMP(z, sector->floorheight, sector->ceilingheight);
            frac = FixedDiv(z - shootz, FixedMul(aimslope, attackrange));
            x = trace.x + FixedMul (trace.dx, frac);
            y = trace.y + FixedMul (trace.dy, frac);
          }
        }
      }

      // Spawn bullet puffs.

      P_SpawnPuff (x,y,z);

      // don't go any farther

      return false;
    }

  // shoot a thing

  th = in->d.thing;
  if (th == shootthing)
    return true;  // can't shoot self

  if (!(th->flags&MF_SHOOTABLE))
    return true;  // corpse or something

  // check angles to see if the thing can be aimed at

  dist = FixedMul (attackrange, in->frac);
  thingtopslope = FixedDiv (th->z+thingheight(th, shootthing) - shootz , dist);

  if (thingtopslope < aimslope)
    return true;  // shot over the thing

  thingbottomslope = FixedDiv (th->z - shootz, dist);

  if (thingbottomslope > aimslope)
    return true;  // shot under the thing

  // hit thing
  // position a bit closer

  frac = in->frac - FixedDiv (10*FRACUNIT,attackrange);

  x = trace.x + FixedMul (trace.dx, frac);
  y = trace.y + FixedMul (trace.dy, frac);
  z = shootz + FixedMul (aimslope, FixedMul(frac, attackrange));

  // Spawn bullet puffs or blod spots,
  // depending on target type.
  if (in->d.thing->flags & MF_NOBLOOD)
    P_SpawnPuff (x,y,z);
  else
    P_SpawnBlood (x,y,z, la_damage, th);

  if (la_damage)
    P_DamageMobj (th, shootthing, shootthing, la_damage);

  // don't go any farther
  return false;
}

//
// P_AimLineAttack
//
// killough 8/2/98: add mask parameter, which, if set to MF_FRIEND,
// makes autoaiming skip past friends.

fixed_t P_AimLineAttack(mobj_t *t1,angle_t angle,fixed_t distance,int mask)
{
  fixed_t x2, y2;

  t1 = P_SubstNullMobj(t1);

  angle >>= ANGLETOFINESHIFT;
  shootthing = t1;

  x2 = t1->x + (distance>>FRACBITS)*finecosine[angle];
  y2 = t1->y + (distance>>FRACBITS)*finesine[angle];
  shootz = t1->z + (t1->height>>1) + 8*FRACUNIT;

  // can't shoot outside view angles

  if (t1->player && direct_vertical_aiming && (mask & CROSSHAIR_AIM))
  {
    topslope = t1->player->slope + 1;
    bottomslope = t1->player->slope - 1;
  }
  else
  {
    topslope = 100*FRACUNIT/160;
    bottomslope = -100*FRACUNIT/160;
  }

  attackrange = distance;
  linetarget = NULL;

  // killough 8/2/98: prevent friends from aiming at friends
  aim_flags_mask = mask & MF_FRIEND;

  P_PathTraverse(t1->x,t1->y,x2,y2,PT_ADDLINES|PT_ADDTHINGS,PTR_AimTraverse);

  if (linetarget)
    return aimslope;

  return 0;
}

//
// P_LineAttack
// If damage == 0, it is just a test trace
// that will leave linetarget set.
//

void P_LineAttack(mobj_t *t1, angle_t angle, fixed_t distance,
		  fixed_t slope, int damage)
{
  fixed_t x2, y2;

  angle >>= ANGLETOFINESHIFT;
  shootthing = t1;
  la_damage = damage;
  x2 = t1->x + (distance>>FRACBITS)*finecosine[angle];
  y2 = t1->y + (distance>>FRACBITS)*finesine[angle];
  shootz = t1->z + (t1->height>>1) + 8*FRACUNIT;
  attackrange = distance;
  aimslope = slope;
  P_PathTraverse(t1->x,t1->y,x2,y2,PT_ADDLINES|PT_ADDTHINGS,PTR_ShootTraverse);
}

//
// USE LINES
//

static mobj_t *usething;

// killough 11/98: reformatted

static boolean PTR_UseTraverse(intercept_t *in)
{
  return in->d.line->special ?
    P_UseSpecialLine(usething, in->d.line, 
		     P_PointOnLineSide(usething->x,usething->y,in->d.line)==1, false),

    //WAS can't use for than one special line in a row
    //jff 3/21/98 NOW multiple use allowed with enabling line flag
    
    !demo_compatibility && in->d.line->flags & ML_PASSUSE :

    (P_LineOpening(in->d.line), openrange <= 0) ?

    // can't use through a wall / not a special line, but keep checking

    S_StartSound (usething, sfx_noway), false : true;
}

// Returns false if a "oof" sound should be made because of a blocking
// linedef. Makes 2s middles which are impassable, as well as 2s uppers
// and lowers which block the player, cause the sound effect when the
// player tries to activate them. Specials are excluded, although it is
// assumed that all special linedefs within reach have been considered
// and rejected already (see P_UseLines).
//
// by Lee Killough
//

static boolean PTR_NoWayTraverse(intercept_t *in)
{
  line_t *ld = in->d.line;                        // This linedef

  return ld->special ||                           // Ignore specials
    !(ld->flags & ML_BLOCKING ||                  // Always blocking
      (P_LineOpening(ld),                         // Find openings
       openrange <= 0 ||                          // No opening
       openbottom > usething->z+24*FRACUNIT ||    // Too high it blocks
       opentop < usething->z+usething->height));  // Too low it blocks
}

//
// P_UseLines
// Looks for special lines in front of the player to activate.
//

void P_UseLines(player_t *player)
{
  fixed_t x1, y1, x2, y2;
  int angle;

  usething = player->mo;

  angle = player->mo->angle >> ANGLETOFINESHIFT;

  x1 = player->mo->x;
  y1 = player->mo->y;
  x2 = x1 + (USERANGE>>FRACBITS)*finecosine[angle];
  y2 = y1 + (USERANGE>>FRACBITS)*finesine[angle];

  // old code:
  //
  // P_PathTraverse ( x1, y1, x2, y2, PT_ADDLINES, PTR_UseTraverse );
  //
  // This added test makes the "oof" sound work on 2s lines -- killough:

  if (P_PathTraverse(x1, y1, x2, y2, PT_ADDLINES, PTR_UseTraverse))
    if (!P_PathTraverse(x1, y1, x2, y2, PT_ADDLINES, PTR_NoWayTraverse))
      S_StartSound (usething, sfx_noway);
}

/*
==============
=
= PTR_SightTraverse
=
==============
*/

static fixed_t sightzstart;            // eye z of looker

boolean PTR_SightTraverse(intercept_t *in)
{
  line_t *li;
  fixed_t slope;

  li = in->d.line;

  //
  // crosses a two sided line
  //
  P_LineOpening(li);

  if (openbottom >= opentop)  // quick test for totally closed doors
    return false;  // stop

  if (li->frontsector->floorheight != li->backsector->floorheight)
  {
    slope = FixedDiv(openbottom - sightzstart , in->frac);
    if (slope > bottomslope)
      bottomslope = slope;
  }

  if (li->frontsector->ceilingheight != li->backsector->ceilingheight)
  {
    slope = FixedDiv(opentop - sightzstart, in->frac);
    if (slope < topslope)
      topslope = slope;
  }

  if (topslope <= bottomslope)
    return false;  // stop

  return true;  // keep going
}

// This is the P_CheckSight() implementation in Doom 1.2,
// taken from the Heretic source code.
//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 1993-2008 Raven Software
//
// p_map.c:PTR_SightTraverse()
// p_map.c:P_CheckSight_12()
// p_maputl.c:P_SightBlockLinesIterator()
// p_maputl.c:P_SightTraverseIntercepts()
// p_maputl.c:P_SightPathTraverse()

/*
=====================
=
= P_CheckSight
=
= Returns true if a straight line between t1 and t2 is unobstructed
= look from eyes of t1 to any part of t2
=
=====================
*/

boolean P_CheckSight_12(mobj_t *t1, mobj_t *t2)
{
  int s1, s2;
  int pnum, bytenum, bitnum;

  //
  // check for trivial rejection
  //
  s1 = (t1->subsector->sector - sectors);
  s2 = (t2->subsector->sector - sectors);
  pnum = s1*numsectors + s2;
  bytenum = pnum>>3;
  bitnum = 1 << (pnum&7);

  if (rejectmatrix[bytenum]&bitnum)
  {
    return false;    // can't possibly be connected
  }

  //
  // check precisely
  //
  sightzstart = t1->z + t1->height - (t1->height>>2);
  topslope = (t2->z+t2->height) - sightzstart;
  bottomslope = (t2->z) - sightzstart;

  return P_SightPathTraverse (t1->x, t1->y, t2->x, t2->y);
}

//
// RADIUS ATTACK
//

static mobj_t *bombsource, *bombspot;
static int bombdamage;
static int bombdistance;

//
// PIT_RadiusAttack
// "bombsource" is the creature
// that caused the explosion at "bombspot".
//

// mbf21: dehacked splash groups
static boolean P_SplashImmune(mobj_t *target, mobj_t *spot)
{
  return // not default behaviour and same group
    mobjinfo[target->type].splash_group != SG_DEFAULT &&
    mobjinfo[target->type].splash_group == mobjinfo[spot->type].splash_group;
}

boolean PIT_RadiusAttack(mobj_t *thing)
{
  fixed_t dx, dy, dist;

  // killough 8/20/98: allow bouncers to take damage 
  // (missile bouncers are already excluded with MF_NOBLOCKMAP)

  if (!(thing->flags & (MF_SHOOTABLE | MF_BOUNCES)))
    return true;

  if (P_SplashImmune(thing, bombspot))
    return true;

  // Boss spider and cyborg
  // take no damage from concussion.

  // killough 8/10/98: allow grenades to hurt anyone, unless
  // fired by Cyberdemons, in which case it won't hurt Cybers.

  if (bombspot->flags & MF_BOUNCES ?
      thing->type == MT_CYBORG && bombsource->type == MT_CYBORG :
      thing->flags2 & (MF2_NORADIUSDMG | MF2_BOSS) &&
      !(bombspot->flags2 & MF2_FORCERADIUSDMG))
    return true;

  dx = abs(thing->x - bombspot->x);
  dy = abs(thing->y - bombspot->y);

  dist = dx>dy ? dx : dy;
  dist = (dist - thing->radius) >> FRACBITS;

  if (dist < 0)
    dist = 0;

  if (dist >= bombdistance)
    return true;  // out of range

  if (P_CheckSight(thing, bombspot))      // must be in direct path
    {
      int damage;
      // [XA] independent damage/distance calculation.
      //      same formula as eternity; thanks Quas :P
      if (bombdamage == bombdistance)
        damage = bombdamage - dist;
      else
        damage = (bombdamage * (bombdistance - dist) / bombdistance) + 1;

      P_DamageMobj(thing, bombspot, bombsource, damage);
    }

  return true;
}

//
// P_RadiusAttack
// Source is the creature that caused the explosion at spot.
//

void P_RadiusAttack(mobj_t *spot, mobj_t *source, int damage, int distance)
{
  fixed_t dist = (distance+MAXRADIUS)<<FRACBITS;
  int yh = (spot->y + dist - bmaporgy)>>MAPBLOCKSHIFT;
  int yl = (spot->y - dist - bmaporgy)>>MAPBLOCKSHIFT;
  int xh = (spot->x + dist - bmaporgx)>>MAPBLOCKSHIFT;
  int xl = (spot->x - dist - bmaporgx)>>MAPBLOCKSHIFT;
  int x, y;

  bombspot = spot;
  bombsource = source;
  bombdamage = damage;
  bombdistance = distance;

  for (y=yl ; y<=yh ; y++)
    for (x=xl ; x<=xh ; x++)
      P_BlockThingsIterator(x, y, PIT_RadiusAttack, false);
}

//
// SECTOR HEIGHT CHANGING
// After modifying a sectors floor or ceiling height,
// call this routine to adjust the positions
// of all things that touch the sector.
//
// If anything doesn't fit anymore, true will be returned.
// If crunch is true, they will take damage
//  as they are being crushed.
// If Crunch is false, you should set the sector height back
//  the way it was and call P_ChangeSector again
//  to undo the changes.
//

static boolean crushchange, nofit;

//
// PIT_ChangeSector
//

boolean PIT_ChangeSector(mobj_t *thing)
{
  mobj_t *mo;

  if (P_ThingHeightClip(thing))
    return true; // keep checking

  // crunch bodies to giblets

  if (thing->health <= 0)
    {
      P_SetMobjState(thing, S_GIBS);
      thing->flags &= ~MF_SOLID;
      thing->height = thing->radius = 0;
      if (thing->info->bloodcolor)
      {
        thing->flags_extra |= MFX_COLOREDBLOOD;
        thing->bloodcolor = V_BloodColor(thing->info->bloodcolor);
      }
      return true;      // keep checking
    }

  // crunch dropped items

  if (thing->flags & MF_DROPPED)
    {
      P_RemoveMobj(thing);
      return true;      // keep checking
    }

  // killough 11/98: kill touchy things immediately
  if (thing->flags & MF_TOUCHY &&
      (thing->intflags & MIF_ARMED || sentient(thing)))
    {
      P_DamageMobj(thing, NULL, NULL, thing->health);  // kill object
      return true;   // keep checking
    }

  if (!(thing->flags & MF_SHOOTABLE))
    return true;        // assume it is bloody gibs or something

  nofit = true;

  if (crushchange && !(leveltime&3))
    {
      int t;         // killough 8/10/98

      P_DamageMobjBy(thing,NULL,NULL,10,MOD_Crush);

      // spray blood in a random direction
      mo = P_SpawnMobj (thing->x,
			thing->y,
			thing->z + thing->height/2, MT_BLOOD);

      if (thing->info->bloodcolor)
      {
        mo->flags_extra |= MFX_COLOREDBLOOD;
        mo->bloodcolor = V_BloodColor(thing->info->bloodcolor);
      }

      // killough 8/10/98: remove dependence on order of evaluation
      t = P_Random(pr_crush);
      mo->momx = (t - P_Random (pr_crush))<<12;
      t = P_Random(pr_crush);
      mo->momy = (t - P_Random (pr_crush))<<12;
    }

  // keep checking (crush other things)
  return true;
}

//
// P_ChangeSector
//
// [FG] Compatibility bug in T_MovePlane
// http://prboom.sourceforge.net/mbf-bugs.html
boolean P_ChangeSector(sector_t *sector,boolean crunch)
{
  int x, y;

  nofit = false;
  crushchange = crunch;

  // ARRGGHHH!!!!
  // This is horrendously slow!!!
  // killough 3/14/98

  // re-check heights for all things near the moving sector

  for (x=sector->blockbox[BOXLEFT] ; x<= sector->blockbox[BOXRIGHT] ; x++)
    for (y=sector->blockbox[BOXBOTTOM];y<= sector->blockbox[BOXTOP] ; y++)
      P_BlockThingsIterator (x, y, PIT_ChangeSector, false);

  return nofit;
}

//
// P_CheckSector
// jff 3/19/98 added to just check monsters on the periphery
// of a moving sector instead of all in bounding box of the
// sector. Both more accurate and faster.
//

boolean P_CheckSector(sector_t *sector,boolean crunch)
{
  msecnode_t *n;

  // killough 10/98: sometimes use Doom's method
  if (comp[comp_floors] && (demo_version >= DV_MBF || demo_compatibility))
    return P_ChangeSector(sector,crunch);

  nofit = false;
  crushchange = crunch;

  // killough 4/4/98: scan list front-to-back until empty or exhausted,
  // restarting from beginning after each thing is processed. Avoids
  // crashes, and is sure to examine all things in the sector, and only
  // the things which are in the sector, until a steady-state is reached.
  // Things can arbitrarily be inserted and removed and it won't mess up.
  //
  // killough 4/7/98: simplified to avoid using complicated counter

  // Mark all things invalid

  for (n=sector->touching_thinglist; n; n=n->m_snext)
    n->visited = false;

  do
    for (n=sector->touching_thinglist; n; n=n->m_snext)  // go through list
      if (!n->visited)               // unprocessed thing found
        {
	  n->visited  = true;          // mark thing as processed
	  if (!(n->m_thing->flags & MF_NOBLOCKMAP)) //jff 4/7/98 don't do these
	    PIT_ChangeSector(n->m_thing);    // process it
	  break;                 // exit and start over
        }
  while (n);  // repeat from scratch until all things left are marked valid

  return nofit;
}

// phares 3/21/98
//
// Maintain a freelist of msecnode_t's to reduce memory allocs and frees.

msecnode_t *headsecnode = NULL;

// P_GetSecnode() retrieves a node from the freelist. The calling routine
// should make sure it sets all fields properly.
//
// killough 11/98: reformatted

static msecnode_t *P_GetSecnode(void)
{
  msecnode_t *node;

  return headsecnode ?
    node = headsecnode, headsecnode = node->m_snext, node :
    arena_alloc(msecnodes_arena, msecnode_t);
}

// P_PutSecnode() returns a node to the freelist.

static void P_PutSecnode(msecnode_t *node)
{
  node->m_snext = headsecnode;
  headsecnode = node;
}

// phares 3/16/98
//
// P_AddSecnode() searches the current list to see if this sector is
// already there. If not, it adds a sector node at the head of the list of
// sectors this object appears in. This is called when creating a list of
// nodes that will get linked in later. Returns a pointer to the new node.
//
// killough 11/98: reformatted

static msecnode_t *P_AddSecnode(sector_t *s, mobj_t *thing, 
				msecnode_t *nextnode)
{
  msecnode_t *node;

  for (node = nextnode; node; node = node->m_tnext)
    if (node->m_sector == s)   // Already have a node for this sector?
      {
	node->m_thing = thing; // Yes. Setting m_thing says 'keep it'.
	return nextnode;
      }

  // Couldn't find an existing node for this sector. Add one at the head
  // of the list.

  node = P_GetSecnode();

  node->visited = 0;  // killough 4/4/98, 4/7/98: mark new nodes unvisited.

  node->m_sector = s;         // sector
  node->m_thing  = thing;     // mobj
  node->m_tprev  = NULL;      // prev node on Thing thread
  node->m_tnext  = nextnode;  // next node on Thing thread

  if (nextnode)
    nextnode->m_tprev = node; // set back link on Thing

  // Add new node at head of sector thread starting at s->touching_thinglist

  node->m_sprev  = NULL;    // prev node on sector thread
  node->m_snext  = s->touching_thinglist; // next node on sector thread
  if (s->touching_thinglist)
    node->m_snext->m_sprev = node;
  return s->touching_thinglist = node;
}

// P_DelSecnode() deletes a sector node from the list of
// sectors this object appears in. Returns a pointer to the next node
// on the linked list, or NULL.
//
// killough 11/98: reformatted

static msecnode_t *P_DelSecnode(msecnode_t *node)
{
  if (node)
    {
      msecnode_t *tp = node->m_tprev;  // prev node on thing thread
      msecnode_t *tn = node->m_tnext;  // next node on thing thread
      msecnode_t *sp;  // prev node on sector thread
      msecnode_t *sn;  // next node on sector thread

      // Unlink from the Thing thread. The Thing thread begins at
      // sector_list and not from mobj_t->touching_sectorlist.

      if (tp)
	tp->m_tnext = tn;

      if (tn)
	tn->m_tprev = tp;

      // Unlink from the sector thread. This thread begins at
      // sector_t->touching_thinglist.

      sp = node->m_sprev;
      sn = node->m_snext;

      if (sp)
	sp->m_snext = sn;
      else
	node->m_sector->touching_thinglist = sn;

      if (sn)
	sn->m_sprev = sp;

      // Return this node to the freelist

      P_PutSecnode(node);

      node = tn;
    }
  return node;
}

// Delete an entire sector list

void P_DelSeclist(msecnode_t *node)
{
  while (node)
    node = P_DelSecnode(node);
}

// phares 3/14/98
//
// PIT_GetSectors
// Locates all the sectors the object is in by looking at the lines that
// cross through it. You have already decided that the object is allowed
// at this location, so don't bother with checking impassable or
// blocking lines.

static boolean PIT_GetSectors(line_t *ld)
{
  if (tmbbox[BOXRIGHT]  <= ld->bbox[BOXLEFT]   ||
      tmbbox[BOXLEFT]   >= ld->bbox[BOXRIGHT]  ||
      tmbbox[BOXTOP]    <= ld->bbox[BOXBOTTOM] ||
      tmbbox[BOXBOTTOM] >= ld->bbox[BOXTOP])
    return true;

  if (P_BoxOnLineSide(tmbbox, ld) != -1)
    return true;

  // This line crosses through the object.

  // Collect the sector(s) from the line and add to the
  // sector_list you're examining. If the Thing ends up being
  // allowed to move to this position, then the sector_list
  // will be attached to the Thing's mobj_t at touching_sectorlist.

  sector_list = P_AddSecnode(ld->frontsector,tmthing,sector_list);

  // Don't assume all lines are 2-sided, since some Things
  // like MT_TFOG are allowed regardless of whether their radius takes
  // them beyond an impassable linedef.

  // killough 3/27/98, 4/4/98:
  // Use sidedefs instead of 2s flag to determine two-sidedness.
  // killough 8/1/98: avoid duplicate if same sector on both sides

  if (ld->backsector && ld->backsector != ld->frontsector)
    sector_list = P_AddSecnode(ld->backsector, tmthing, sector_list);

  return true;
}

// phares 3/14/98
//
// P_CreateSecNodeList alters/creates the sector_list that shows what sectors
// the object resides in.
//
// killough 11/98: reformatted

void P_CreateSecNodeList(mobj_t *thing,fixed_t x,fixed_t y)
{
  int xl, xh, yl, yh, bx, by;
  msecnode_t *node;

  // [FG] Overlapping uses of global variables in p_map.c
  // http://prboom.sourceforge.net/mbf-bugs.html
  mobj_t* saved_tmthing = tmthing;
  int saved_tmflags = tmflags;
  fixed_t saved_tmx = tmx, saved_tmy = tmy;

  // First, clear out the existing m_thing fields. As each node is
  // added or verified as needed, m_thing will be set properly. When
  // finished, delete all nodes where m_thing is still NULL. These
  // represent the sectors the Thing has vacated.
  
  for (node = sector_list; node; node = node->m_tnext)
    node->m_thing = NULL;

  tmthing = thing;
  tmflags = thing->flags;

  tmx = x;
  tmy = y;

  tmbbox[BOXTOP]  = y + tmthing->radius;
  tmbbox[BOXBOTTOM] = y - tmthing->radius;
  tmbbox[BOXRIGHT]  = x + tmthing->radius;
  tmbbox[BOXLEFT]   = x - tmthing->radius;

  validcount++; // used to make sure we only process a line once

  xl = (tmbbox[BOXLEFT] - bmaporgx)>>MAPBLOCKSHIFT;
  xh = (tmbbox[BOXRIGHT] - bmaporgx)>>MAPBLOCKSHIFT;
  yl = (tmbbox[BOXBOTTOM] - bmaporgy)>>MAPBLOCKSHIFT;
  yh = (tmbbox[BOXTOP] - bmaporgy)>>MAPBLOCKSHIFT;

  for (bx=xl ; bx<=xh ; bx++)
    for (by=yl ; by<=yh ; by++)
      P_BlockLinesIterator(bx,by,PIT_GetSectors);

  // Add the sector of the (x,y) point to sector_list.

  sector_list = P_AddSecnode(thing->subsector->sector,thing,sector_list);

  // Now delete any nodes that won't be used. These are the ones where
  // m_thing is still NULL.
  
  for (node = sector_list; node;)
    if (node->m_thing == NULL)
      {
	if (node == sector_list)
	  sector_list = node->m_tnext;
	node = P_DelSecnode(node);
      }
    else
      node = node->m_tnext;

  // [FG] Overlapping uses of global variables in p_map.c
  // http://prboom.sourceforge.net/mbf-bugs.html
   if (demo_compatibility || mbf21)
   {
     tmthing = saved_tmthing;
     tmflags = saved_tmflags;
   }
   if (demo_compatibility)
   {
     tmx = saved_tmx;
     tmy = saved_tmy;
     if (tmthing)
     {
       tmbbox[BOXTOP]    = tmy + tmthing->radius;
       tmbbox[BOXBOTTOM] = tmy - tmthing->radius;
       tmbbox[BOXRIGHT]  = tmx + tmthing->radius;
       tmbbox[BOXLEFT]   = tmx - tmthing->radius;
     }
   }
}

/* cphipps 2004/08/30 -
 * Must clear tmthing at tic end, as it might contain a pointer to a
 * removed thinker, or the level might have ended/been ended and we
 * clear the objects it was pointing too. Hopefully we don't need to
 * carry this between tics for sync. */

void P_MapStart(void)
{
  if (tmthing)
    I_Error("tmthing set!");
}

void P_MapEnd(void)
{
  tmthing = NULL;
}

// [FG] SPECHITS overflow emulation from Chocolate Doom / PrBoom+

static void SpechitOverrun(line_t *ld)
{
    static unsigned int baseaddr = 0;
    unsigned int addr;

    if (baseaddr == 0)
    {
        int p;

        // This is the first time we have had an overrun.  Work out
        // what base address we are going to use.
        // Allow a spechit value to be specified on the command line.

        //!
        // @category compat
        // @arg <n>
        //
        // Use the specified magic value when emulating spechit overruns.
        //

        p = M_CheckParmWithArgs("-spechit", 1);

        if (p > 0)
        {
            if (!M_StrToInt(myargv[p+1], (int *) &baseaddr))
              I_Error("Invalid parameter '%s' for -spechit.", myargv[p+1]);
        }
        else
        {
            baseaddr = DEFAULT_SPECHIT_MAGIC;
        }
    }

    // Calculate address used in doom2.exe

    addr = baseaddr + (ld - lines) * 0x3E;

    switch(numspechit)
    {
        case 9:
        case 10:
        case 11:
        case 12:
            tmbbox[numspechit-9] = addr;
            break;
        case 13:
            crushchange = addr;
            break;
        case 14:
            nofit = addr;
            break;
        default:
            I_Printf(VB_DEBUG, "SpechitOverrun: Warning: unable to emulate"
                               "an overrun where numspechit=%i",
                               numspechit);
            break;
    }
}

//----------------------------------------------------------------------------
//
// $Log: p_map.c,v $
// Revision 1.35  1998/05/12  12:47:16  phares
// Removed OVER_UNDER code
//
// Revision 1.34  1998/05/07  00:52:38  killough
// beautification
//
// Revision 1.33  1998/05/05  15:35:10  phares
// Documentation and Reformatting changes
//
// Revision 1.32  1998/05/04  12:29:27  phares
// Eliminate player bobbing when stuck against wall
//
// Revision 1.31  1998/05/03  23:22:19  killough
// Fix #includes and remove unnecessary decls at the top, make some vars static
//
// Revision 1.30  1998/04/20  11:12:59  killough
// Make topslope, bottomslope local
//
// Revision 1.29  1998/04/12  01:56:51  killough
// Prevent no-clipping objects from blocking things
//
// Revision 1.28  1998/04/07  11:39:21  jim
// Skip MF_NOBLOCK things in P_CheckSector to get puffs back
//
// Revision 1.27  1998/04/07  06:52:36  killough
// Simplify sector_thinglist traversal to use simpler markers
//
// Revision 1.26  1998/04/06  11:05:11  jim
// Remove LEESFIXES, AMAP bdg->247
//
// Revision 1.25  1998/04/06  04:46:13  killough
// Fix CheckSector problems
//
// Revision 1.24  1998/04/05  10:08:51  jim
// changed crusher check back to old code
//
// Revision 1.23  1998/04/03  14:44:14  jim
// Fixed P_CheckSector problem
//
// Revision 1.21  1998/04/01  14:46:48  jim
// Prevent P_CheckSector from passing NULL things
//
// Revision 1.20  1998/03/29  20:14:35  jim
// Fixed use of deleted link in P_CheckSector
//
// Revision 1.19  1998/03/28  18:00:14  killough
// Fix telefrag/spawnfrag bug, and use sidedefs rather than 2s flag
//
// Revision 1.18  1998/03/23  15:24:25  phares
// Changed pushers to linedef control
//
// Revision 1.17  1998/03/23  06:43:14  jim
// linedefs reference initial version
//
// Revision 1.16  1998/03/20  02:10:43  jim
// Improved crusher code with new mobj data structures
//
// Revision 1.15  1998/03/20  00:29:57  phares
// Changed friction to linedef control
//
// Revision 1.14  1998/03/16  12:25:17  killough
// Allow conveyors to push things off ledges
//
// Revision 1.13  1998/03/12  14:28:42  phares
// friction and IDCLIP changes
//
// Revision 1.12  1998/03/11  17:48:24  phares
// New cheats, clean help code, friction fix
//
// Revision 1.11  1998/03/09  22:27:23  phares
// Fixed friction problem when teleporting
//
// Revision 1.10  1998/03/09  18:27:00  phares
// Fixed bug in neighboring variable friction sectors
//
// Revision 1.9  1998/03/02  12:05:56  killough
// Add demo_compatibility switch around moveangle+=10
//
// Revision 1.8  1998/02/24  08:46:17  phares
// Pushers, recoil, new friction, and over/under work
//
// Revision 1.7  1998/02/17  06:01:51  killough
// Use new RNG calling sequence
//
// Revision 1.6  1998/02/05  12:15:03  phares
// cleaned up comments
//
// Revision 1.5  1998/01/28  23:42:02  phares
// Bug fix to PE->LS code; better line checking
//
// Revision 1.4  1998/01/28  17:36:06  phares
// Expanded comments on Pit_CrossLine
//
// Revision 1.3  1998/01/28  12:22:21  phares
// AV bug fix and Lost Soul trajectory bug fix
//
// Revision 1.2  1998/01/26  19:24:09  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:02:59  rand
// Lee's Jan 19 sources
//
//
//----------------------------------------------------------------------------
