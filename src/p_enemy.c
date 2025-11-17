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
//      Enemy thinking, AI.
//      Action Pointer Functions
//      that are associated with states/frames.
//
//-----------------------------------------------------------------------------

#include <stdlib.h>

#include "d_items.h"
#include "d_player.h"
#include "d_think.h"
#include "doomdata.h"
#include "doomdef.h"
#include "doomstat.h"
#include "doomtype.h"
#include "g_game.h"
#include "g_umapinfo.h"
#include "hu_obituary.h"
#include "i_printf.h"
#include "i_system.h"
#include "info.h"
#include "m_array.h"
#include "m_bbox.h"
#include "m_fixed.h"
#include "m_random.h"
#include "p_action.h"
#include "p_enemy.h"
#include "p_inter.h"
#include "p_map.h"
#include "p_maputl.h"
#include "p_mobj.h"
#include "p_pspr.h"
#include "p_setup.h"
#include "p_spec.h"
#include "p_tick.h"
#include "r_defs.h"
#include "r_main.h"
#include "r_state.h"
#include "r_tranmap.h"
#include "s_sound.h"
#include "sounds.h"
#include "tables.h"
#include "z_zone.h"

static mobj_t *current_actor;

typedef enum {
  DI_EAST,
  DI_NORTHEAST,
  DI_NORTH,
  DI_NORTHWEST,
  DI_WEST,
  DI_SOUTHWEST,
  DI_SOUTH,
  DI_SOUTHEAST,
  DI_NODIR,
  NUMDIRS
} dirtype_t;

static void P_NewChaseDir(mobj_t *actor);

//
// ENEMY THINKING
// Enemies are allways spawned
// with targetplayer = -1, threshold = 0
// Most monsters are spawned unaware of all players,
// but some can be made preaware
//

//
// Called by P_NoiseAlert.
// Recursively traverse adjacent sectors,
// sound blocking lines cut off traversal.
//
// killough 5/5/98: reformatted, cleaned up

static void P_RecursiveSound(sector_t *sec, int soundblocks,
			     mobj_t *soundtarget)
{
  int i;

  // wake up all monsters in this sector
  if (sec->validcount == validcount && sec->soundtraversed <= soundblocks+1)
    return;             // already flooded

  sec->validcount = validcount;
  sec->soundtraversed = soundblocks+1;
  P_SetTarget(&sec->soundtarget, soundtarget);     // killough 11/98

  for (i=0; i<sec->linecount; i++)
    {
      sector_t *other;
      line_t *check = sec->lines[i];

      if (!(check->flags & ML_TWOSIDED))
        continue;

      P_LineOpening(check);

      if (openrange <= 0)
        continue;       // closed door

      other=sides[check->sidenum[sides[check->sidenum[0]].sector==sec]].sector;

      if (!(check->flags & ML_SOUNDBLOCK))
        P_RecursiveSound(other, soundblocks, soundtarget);
      else
        if (!soundblocks)
          P_RecursiveSound(other, 1, soundtarget);
    }
}

//
// P_NoiseAlert
// If a monster yells at a player,
// it will alert other monsters to the player.
//

void P_NoiseAlert(mobj_t *target, mobj_t *emitter)
{
  // [crispy] monsters are deaf with NOTARGET cheat
  if (target && target->player && (target->player->cheats & CF_NOTARGET))
    return;

  validcount++;
  P_RecursiveSound(emitter->subsector->sector, 0, target);
}

//
// P_CheckRange
//

static boolean P_CheckRange(mobj_t *actor, fixed_t range)
{
  mobj_t *pl = actor->target;

  return  // killough 7/18/98: friendly monsters don't attack other friends
    pl && !(actor->flags & pl->flags & MF_FRIEND) &&
    (P_AproxDistance(pl->x-actor->x, pl->y-actor->y) <
     range) &&
    P_CheckSight(actor, actor->target);
}

//
// P_CheckMeleeRange
//
// mbf21: add meleerange property
//

static boolean P_CheckMeleeRange(mobj_t *actor)
{
  int range;

  range = actor->info->meleerange;

  range += actor->target->info->radius - 20 * FRACUNIT;

  return P_CheckRange(actor, range);
}

//
// P_HitFriend()
//
// killough 12/98
// This function tries to prevent shooting at friends

static boolean P_HitFriend(mobj_t *actor)
{
  return actor->target &&
    (P_AimLineAttack(actor, 
		     R_PointToAngle2(actor->x, actor->y,
				     actor->target->x, actor->target->y),
		     P_AproxDistance(actor->x-actor->target->x, 
				     actor->y-actor->target->y), 0),
     linetarget) && linetarget != actor->target &&
    !((linetarget->flags ^ actor->flags) & MF_FRIEND);
}

//
// P_CheckMissileRange
//
static boolean P_CheckMissileRange(mobj_t *actor)
{
  fixed_t dist;

  if (!P_CheckSight(actor, actor->target))
    return false;

  if (actor->flags & MF_JUSTHIT)
    {      // the target just hit the enemy, so fight back!
      actor->flags &= ~MF_JUSTHIT;

      // killough 7/18/98: no friendly fire at corpses
      // killough 11/98: prevent too much infighting among friends

      return 
	!(actor->flags & MF_FRIEND) || 
	(actor->target->health > 0 &&
	 (!(actor->target->flags & MF_FRIEND) ||
	  (actor->target->player ? 
	   monster_infighting || P_Random(pr_defect) >128 :
	   !(actor->target->flags & MF_JUSTHIT) && P_Random(pr_defect) >128)));
    }

  // killough 7/18/98: friendly monsters don't attack other friendly
  // monsters or players (except when attacked, and then only once)
  if (actor->flags & actor->target->flags & MF_FRIEND)
    return false;

  if (actor->reactiontime)
    return false;       // do not attack yet

  // OPTIMIZE: get this from a global checksight
  dist = P_AproxDistance ( actor->x-actor->target->x,
                           actor->y-actor->target->y) - 64*FRACUNIT;

  if (!actor->info->meleestate)
    dist -= 128*FRACUNIT;       // no melee attack, so fire more

  dist >>= FRACBITS;

  if (actor->flags2 & MF2_SHORTMRANGE)
    if (dist > 14*64)
      return false;     // too far away

  if (actor->flags2 & MF2_LONGMELEE)
    {
      if (dist < 196)
        return false;   // close for fist attack
    }

  if (actor->flags2 & MF2_RANGEHALF)
    dist >>= 1;

  if (dist > 200)
    dist = 200;

  if (actor->flags2 & MF2_HIGHERMPROB && dist > 160)
    dist = 160;

  if (P_Random(pr_missrange) < dist)
    return false;
  
  if (actor->flags & MF_FRIEND && P_HitFriend(actor))
    return false;

  return true;
}

//
// P_IsOnLift
//
// killough 9/9/98:
//
// Returns true if the object is on a lift. Used for AI,
// since it may indicate the need for crowded conditions,
// or that a monster should stay on the lift for a while
// while it goes up or down.
//

static boolean P_IsOnLift(const mobj_t *actor)
{
  const sector_t *sec = actor->subsector->sector;
  line_t line = {0}; // [FG] initialize
  int l;

  // Short-circuit: it's on a lift which is active.
  if (sec->floordata && ((thinker_t *) sec->floordata)->function.p1 == T_PlatRaiseAdapter)
    return true;

  // Check to see if it's in a sector which can be activated as a lift.
  if ((line.args[0] = sec->tag))
    for (l = -1; (l = P_FindLineFromLineTag(&line, l)) >= 0;)
      switch (lines[l].special)
	{
	case  10: case  14: case  15: case  20: case  21: case  22:
	case  47: case  53: case  62: case  66: case  67: case  68:
	case  87: case  88: case  95: case 120: case 121: case 122:
	case 123: case 143: case 162: case 163: case 181: case 182:
	case 144: case 148: case 149: case 211: case 227: case 228:
	case 231: case 232: case 235: case 236:
	  return true;
	}
  
  return false;
}

//
// P_IsUnderDamage
//
// killough 9/9/98:
//
// Returns nonzero if the object is under damage based on
// their current position. Returns 1 if the damage is moderate,
// -1 if it is serious. Used for AI.
//

static int P_IsUnderDamage(mobj_t *actor)
{ 
  const struct msecnode_s *seclist;
  const ceiling_t *cl;             // Crushing ceiling
  int dir = 0;
  for (seclist=actor->touching_sectorlist; seclist; seclist=seclist->m_tnext)
    if ((cl = seclist->m_sector->ceilingdata) && 
	cl->thinker.function.p1 == T_MoveCeilingAdapter)
      dir |= cl->direction;
  return dir;
}

//
// P_Move
// Move in the current direction,
// returns false if the move is blocked.
//

static fixed_t xspeed[8] = {FRACUNIT,47000,0,-47000,-FRACUNIT,-47000,0,47000};
static fixed_t yspeed[8] = {0,47000,FRACUNIT,47000,0,-47000,-FRACUNIT,-47000};

static boolean P_Move(mobj_t *actor, int dropoff) // killough 9/12/98
{
  fixed_t tryx, tryy, deltax, deltay;
  boolean try_ok;
  int movefactor = ORIG_FRICTION_FACTOR;    // killough 10/98
  int friction = ORIG_FRICTION;
  int speed;

  if (actor->movedir == DI_NODIR)
    return false;

#ifdef RANGECHECK
  if ((unsigned)actor->movedir >= 8)
    I_Error ("Weird actor->movedir!");
#endif
  
  // killough 10/98: make monsters get affected by ice and sludge too:

  if (monster_friction)
    movefactor = P_GetMoveFactor(actor, &friction);

  speed = actor->info->speed;

  if (friction < ORIG_FRICTION &&     // sludge
      !(speed = ((ORIG_FRICTION_FACTOR - (ORIG_FRICTION_FACTOR-movefactor)/2)
		 * speed) / ORIG_FRICTION_FACTOR))
    speed = 1;      // always give the monster a little bit of speed

  tryx = actor->x + (deltax = speed * xspeed[actor->movedir]);
  tryy = actor->y + (deltay = speed * yspeed[actor->movedir]);

  // killough 12/98: rearrange, fix potential for stickiness on ice

  if (friction <= ORIG_FRICTION)
    try_ok = P_TryMove(actor, tryx, tryy, dropoff);
  else
    {
      fixed_t x = actor->x;
      fixed_t y = actor->y;
#ifdef MBF_STRICT
      // Removed parts incompatible with PrBoom+ complevel 11
      fixed_t floorz = actor->floorz;
      fixed_t ceilingz = actor->ceilingz;
      fixed_t dropoffz = actor->dropoffz;
#endif

      try_ok = P_TryMove(actor, tryx, tryy, dropoff);

      // killough 10/98:
      // Let normal momentum carry them, instead of steptoeing them across ice.

      if (try_ok)
	{
#ifdef MBF_STRICT
	  P_UnsetThingPosition(actor);
#endif
	  actor->x = x;
	  actor->y = y;
#ifdef MBF_STRICT
	  actor->floorz = floorz;
	  actor->ceilingz = ceilingz;
	  actor->dropoffz = dropoffz;
	  P_SetThingPosition(actor);
#endif
	  movefactor *= FRACUNIT / ORIG_FRICTION_FACTOR / 4;
	  actor->momx += FixedMul(deltax, movefactor);
	  actor->momy += FixedMul(deltay, movefactor);
	}
    }

  if (!try_ok)
    {      // open any specials
      int good;

      if (actor->flags & MF_FLOAT && floatok)
        {
          if (actor->z < tmfloorz)          // must adjust height
            actor->z += FLOATSPEED;
          else
            actor->z -= FLOATSPEED;

          actor->flags |= MF_INFLOAT;

	  return true;
        }

      if (!numspechit)
        return false;

      actor->movedir = DI_NODIR;

      // if the special is not a door that can be opened, return false
      //
      // killough 8/9/98: this is what caused monsters to get stuck in
      // doortracks, because it thought that the monster freed itself
      // by opening a door, even if it was moving towards the doortrack,
      // and not the door itself.
      //
      // killough 9/9/98: If a line blocking the monster is activated,
      // return true 90% of the time. If a line blocking the monster is
      // not activated, but some other line is, return false 90% of the
      // time. A bit of randomness is needed to ensure it's free from
      // lockups, but for most cases, it returns the correct result.
      //
      // Do NOT simply return false 1/4th of the time (causes monsters to
      // back out when they shouldn't, and creates secondary stickiness).

      for (good = false; numspechit--; )
        if (P_UseSpecialLine(actor, spechit[numspechit], 0, false))
	  good |= spechit[numspechit] == blockline ? 1 : 2;

      // There are checks elsewhere for numspechit == 0, so we don't want to
      // leave numspechit == -1.
      numspechit = 0;

      // [FG] compatibility maze here
      // Boom v2.01 and orig. Doom return "good"
      // Boom v2.02 and LxDoom return good && (P_Random(pr_trywalk)&3)
      // MBF plays even more games

      if (demo_version < DV_BOOM)
        return good;
      if (demo_version < DV_MBF)
        return good && (compatibility || (P_Random(pr_trywalk)&3)); //jff 8/13/98
      else
      return good && (demo_version < DV_MBF || comp[comp_doorstuck] ||
		      (P_Random(pr_opendoor) >= 230) ^ (good & 1));
    }
  else
    actor->flags &= ~MF_INFLOAT;

  // killough 11/98: fall more slowly, under gravity, if felldown==true
  if (!(actor->flags & MF_FLOAT) && (!felldown || demo_version < DV_MBF))
    actor->z = actor->floorz;

  return true;
}

//
// P_SmartMove
//
// killough 9/12/98: Same as P_Move, except smarter
//

static boolean P_SmartMove(mobj_t *actor)
{
  mobj_t *target = actor->target;
  int on_lift, dropoff = false, under_damage;

  // killough 9/12/98: Stay on a lift if target is on one
  on_lift = !comp[comp_staylift] && target && target->health > 0
    && target->subsector->sector->tag==actor->subsector->sector->tag &&
    P_IsOnLift(actor);

  under_damage = monster_avoid_hazards && P_IsUnderDamage(actor);

  // killough 10/98: allow dogs to drop off of taller ledges sometimes.
  // dropoff==1 means always allow it, dropoff==2 means only up to 128 high,
  // and only if the target is immediately on the other side of the line.

  if (actor->type == MT_DOGS && target && dog_jumping &&
      !((target->flags ^ actor->flags) & MF_FRIEND) &&
      P_AproxDistance(actor->x - target->x,
		      actor->y - target->y) < FRACUNIT*144 &&
      P_Random(pr_dropoff) < 235)
    dropoff = 2;

  if (!P_Move(actor, dropoff))
    return false;

  // killough 9/9/98: avoid crushing ceilings or other damaging areas
  if (
      (on_lift && P_Random(pr_stayonlift) < 230 &&      // Stay on lift
       !P_IsOnLift(actor))
      ||
      (monster_avoid_hazards && !under_damage &&  // Get away from damage
       (under_damage = P_IsUnderDamage(actor)) &&
       (under_damage < 0 || P_Random(pr_avoidcrush) < 200))
      )
    actor->movedir = DI_NODIR;    // avoid the area (most of the time anyway)

  return true;
}

//
// TryWalk
// Attempts to move actor on
// in its current (ob->moveangle) direction.
// If blocked by either a wall or an actor
// returns FALSE
// If move is either clear or blocked only by a door,
// returns TRUE and sets...
// If a door is in the way,
// an OpenDoor call is made to start it opening.
//

static boolean P_TryWalk(mobj_t *actor)
{
  if (!P_SmartMove(actor))
    return false;
  actor->movecount = P_Random(pr_trywalk)&15;
  return true;
}

//
// P_DoNewChaseDir
//
// killough 9/8/98:
//
// Most of P_NewChaseDir(), except for what
// determines the new direction to take
//

static void P_DoNewChaseDir(mobj_t *actor, fixed_t deltax, fixed_t deltay)
{
  dirtype_t xdir, ydir, tdir;
  dirtype_t olddir = actor->movedir;
  dirtype_t turnaround = olddir;

  if (turnaround != DI_NODIR)         // find reverse direction
    turnaround ^= 4;

  xdir = 
    deltax >  10*FRACUNIT ? DI_EAST :
    deltax < -10*FRACUNIT ? DI_WEST : DI_NODIR;

  ydir = 
    deltay < -10*FRACUNIT ? DI_SOUTH :
    deltay >  10*FRACUNIT ? DI_NORTH : DI_NODIR;

  // try direct route
  if (xdir != DI_NODIR && ydir != DI_NODIR && turnaround != 
      (actor->movedir = deltay < 0 ? deltax > 0 ? DI_SOUTHEAST : DI_SOUTHWEST :
       deltax > 0 ? DI_NORTHEAST : DI_NORTHWEST) && P_TryWalk(actor))
    return;

  // try other directions
  if (P_Random(pr_newchase) > 200 || abs(deltay)>abs(deltax))
    tdir = xdir, xdir = ydir, ydir = tdir;

  if ((xdir == turnaround ? xdir = DI_NODIR : xdir) != DI_NODIR &&
      (actor->movedir = xdir, P_TryWalk(actor)))
    return;         // either moved forward or attacked

  if ((ydir == turnaround ? ydir = DI_NODIR : ydir) != DI_NODIR &&
      (actor->movedir = ydir, P_TryWalk(actor)))
    return;

  // there is no direct path to the player, so pick another direction.
  if (olddir != DI_NODIR && (actor->movedir = olddir, P_TryWalk(actor)))
    return;

  // randomly determine direction of search
  if (P_Random(pr_newchasedir) & 1)
    {
      for (tdir = DI_EAST; tdir <= DI_SOUTHEAST; tdir++)
        if (tdir != turnaround && (actor->movedir = tdir, P_TryWalk(actor)))
	  return;
    }
  else
    for (tdir = DI_SOUTHEAST; tdir != DI_EAST-1; tdir--)
      if (tdir != turnaround && (actor->movedir = tdir, P_TryWalk(actor)))
	return;
  
  if ((actor->movedir = turnaround) != DI_NODIR && !P_TryWalk(actor))
    actor->movedir = DI_NODIR;
}

//
// killough 11/98:
//
// Monsters try to move away from tall dropoffs.
//
// In Doom, they were never allowed to hang over dropoffs,
// and would remain stuck if involuntarily forced over one.
// This logic, combined with p_map.c (P_TryMove), allows
// monsters to free themselves without making them tend to
// hang over dropoffs.

static fixed_t dropoff_deltax, dropoff_deltay, floorz;

static boolean PIT_AvoidDropoff(line_t *line)
{
  if (line->backsector                          && // Ignore one-sided linedefs
      tmbbox[BOXRIGHT]  > line->bbox[BOXLEFT]   &&
      tmbbox[BOXLEFT]   < line->bbox[BOXRIGHT]  &&
      tmbbox[BOXTOP]    > line->bbox[BOXBOTTOM] && // Linedef must be contacted
      tmbbox[BOXBOTTOM] < line->bbox[BOXTOP]    &&
      P_BoxOnLineSide(tmbbox, line) == -1)
    {
      fixed_t front = line->frontsector->floorheight;
      fixed_t back  = line->backsector->floorheight;
      angle_t angle;

      // The monster must contact one of the two floors,
      // and the other must be a tall dropoff (more than 24).

      if (back == floorz && front < floorz - FRACUNIT*24)
	angle = R_PointToAngle2(0,0,line->dx,line->dy);   // front side dropoff
      else
	if (front == floorz && back < floorz - FRACUNIT*24)
	  angle = R_PointToAngle2(line->dx,line->dy,0,0); // back side dropoff
	else
	  return true;

      // Move away from dropoff at a standard speed.
      // Multiple contacted linedefs are cumulative (e.g. hanging over corner)
      dropoff_deltax -= finesine[angle >> ANGLETOFINESHIFT]*32;
      dropoff_deltay += finecosine[angle >> ANGLETOFINESHIFT]*32;
    }
  return true;
}

//
// Driver for above
//

static fixed_t P_AvoidDropoff(mobj_t *actor)
{
  int yh=((tmbbox[BOXTOP]   = actor->y+actor->radius)-bmaporgy)>>MAPBLOCKSHIFT;
  int yl=((tmbbox[BOXBOTTOM]= actor->y-actor->radius)-bmaporgy)>>MAPBLOCKSHIFT;
  int xh=((tmbbox[BOXRIGHT] = actor->x+actor->radius)-bmaporgx)>>MAPBLOCKSHIFT;
  int xl=((tmbbox[BOXLEFT]  = actor->x-actor->radius)-bmaporgx)>>MAPBLOCKSHIFT;
  int bx, by;

  floorz = actor->z;            // remember floor height

  dropoff_deltax = dropoff_deltay = 0;

  // check lines

  validcount++;
  for (bx=xl ; bx<=xh ; bx++)
    for (by=yl ; by<=yh ; by++)
      P_BlockLinesIterator(bx, by, PIT_AvoidDropoff);  // all contacted lines

  return dropoff_deltax | dropoff_deltay;   // Non-zero if movement prescribed
}

//
// P_NewChaseDir
//
// killough 9/8/98: Split into two functions
//

static void P_NewChaseDir(mobj_t *actor)
{
  mobj_t *target = actor->target;
  fixed_t deltax = target->x - actor->x;
  fixed_t deltay = target->y - actor->y;

  // killough 8/8/98: sometimes move away from target, keeping distance
  //
  // 1) Stay a certain distance away from a friend, to avoid being in their way
  // 2) Take advantage over an enemy without missiles, by keeping distance

  actor->strafecount = 0;

  if (demo_version >= DV_MBF)
  {
    if (actor->floorz - actor->dropoffz > FRACUNIT*24 &&
	actor->z <= actor->floorz && !(actor->flags & (MF_DROPOFF|MF_FLOAT)) &&
	!comp[comp_dropoff] && P_AvoidDropoff(actor)) // Move away from dropoff
      {
	P_DoNewChaseDir(actor, dropoff_deltax, dropoff_deltay);

	// If moving away from dropoff, set movecount to 1 so that 
	// small steps are taken to get monster away from dropoff.

	actor->movecount = 1;
	return;
      }
    else
      {
	fixed_t dist = P_AproxDistance(deltax, deltay);

	// Move away from friends when too close, except
	// in certain situations (e.g. a crowded lift)

	if (actor->flags & target->flags & MF_FRIEND &&
	    distfriend << FRACBITS > dist && 
	    !P_IsOnLift(target) && !P_IsUnderDamage(actor))
	  deltax = -deltax, deltay = -deltay;
	else
	  if (target->health > 0 && (actor->flags ^ target->flags) & MF_FRIEND)
	    {   // Live enemy target
	      if (monster_backing &&
		  actor->info->missilestate && actor->type != MT_SKULL &&
		  ((!target->info->missilestate && dist < target->info->meleerange*2) ||
		   (target->player && dist < target->info->meleerange*3 &&
		    weaponinfo[target->player->readyweapon].flags & WPF_FLEEMELEE)))
		{       // Back away from melee attacker
		  actor->strafecount = P_Random(pr_enemystrafe) & 15;
		  deltax = -deltax, deltay = -deltay;
		}
	    }
      }
  }

  P_DoNewChaseDir(actor, deltax, deltay);

  // If strafing, set movecount to strafecount so that old Doom
  // logic still works the same, except in the strafing part

  if (actor->strafecount)
    actor->movecount = actor->strafecount;
}

//
// P_IsVisible
//
// killough 9/9/98: whether a target is visible to a monster
//

static boolean P_IsVisible(mobj_t *actor, mobj_t *mo, boolean allaround)
{
  if (!allaround)
    {
      angle_t an = R_PointToAngle2(actor->x, actor->y, 
				   mo->x, mo->y) - actor->angle;
      if (an > ANG90 && an < ANG270 &&
	  P_AproxDistance(mo->x-actor->x, mo->y-actor->y) > WAKEUPRANGE)
	return false;
    }
  return P_CheckSight(actor, mo);
}

//
// PIT_FindTarget
//
// killough 9/5/98
//
// Finds monster targets for other monsters
//

static int current_allaround;

static boolean PIT_FindTarget(mobj_t *mo)
{
  mobj_t *actor = current_actor;

  if (!((mo->flags ^ actor->flags) & MF_FRIEND &&        // Invalid target
	mo->health > 0 && (mo->flags & MF_COUNTKILL || mo->type == MT_SKULL)))
    return true;

  // If the monster is already engaged in a one-on-one attack
  // with a healthy friend, don't attack around 60% the time
  {
    const mobj_t *targ = mo->target;
    if (targ && targ->target == mo &&
	P_Random(pr_skiptarget) > 100 &&
	(targ->flags ^ mo->flags) & MF_FRIEND &&
	targ->health*2 >= targ->info->spawnhealth)
      return true;
  }

  if (!P_IsVisible(actor, mo, current_allaround))
    return true;

  P_SetTarget(&actor->lastenemy, actor->target);  // Remember previous target
  P_SetTarget(&actor->target, mo);                // Found target

  // Move the selected monster to the end of its associated
  // list, so that it gets searched last next time.
	  
  {
    thinker_t *cap = &thinkerclasscap[mo->flags & MF_FRIEND ?
				     th_friends : th_enemies];
    (mo->thinker.cprev->cnext = mo->thinker.cnext)->cprev = mo->thinker.cprev;
    (mo->thinker.cprev = cap->cprev)->cnext = &mo->thinker;
    (mo->thinker.cnext = cap)->cprev = &mo->thinker;
  }

  return false;
}

//
// P_LookForPlayers
// If allaround is false, only look 180 degrees in front.
// Returns true if a player is targeted.
//

static boolean P_LookForPlayers(mobj_t *actor, boolean allaround)
{
  player_t *player;
  int stop, stopc, c;
  boolean unseen[MAXPLAYERS] = {0};

  if (actor->flags & MF_FRIEND)
    {  // killough 9/9/98: friendly monsters go about players differently
      int anyone;

#if 0
      if (!allaround) // If you want friendly monsters not to awaken unprovoked
	return false;
#endif

      // Go back to a player, no matter whether it's visible or not
      for (anyone=0; anyone<=1; anyone++)
	for (c=0; c<MAXPLAYERS; c++)
	  if (playeringame[c] && players[c].playerstate==PST_LIVE &&
	      (anyone || P_IsVisible(actor, players[c].mo, allaround)))
	    {
	      P_SetTarget(&actor->target, players[c].mo);

	      // killough 12/98:
	      // get out of refiring loop, to avoid hitting player accidentally

	      if (actor->info->missilestate)
		{
		  P_SetMobjState(actor, actor->info->seestate);
		  actor->flags &= ~MF_JUSTHIT;
		}

	      return true;
	    }

      return false;
    }

  // Change mask of 3 to (MAXPLAYERS-1) -- killough 2/15/98:
  stop = (actor->lastlook-1)&(MAXPLAYERS-1);

  c = 0;

  stopc = demo_version < DV_MBF && !demo_compatibility && monsters_remember ?
    MAXPLAYERS : 2;       // killough 9/9/98

  for (;; actor->lastlook = (actor->lastlook+1)&(MAXPLAYERS-1))
    {
      if (!playeringame[actor->lastlook])
	continue;

      // killough 2/15/98, 9/9/98:
      if (c++ == stopc || actor->lastlook == stop)  // done looking
      {
        // Andrey Budko
        // Fixed Boom incompatibilities. The following code was missed.
        // There are no more desyncs on Donce's demos on horror.wad

        // Use last known enemy if no players sighted -- killough 2/15/98:
        if (demo_version < DV_MBF && !demo_compatibility && monsters_remember)
        {
          if (actor->lastenemy && actor->lastenemy->health > 0)
          {
            actor->target = actor->lastenemy;
            actor->lastenemy = NULL;
            return true;
          }
        }
	return false;
      }

      player = &players[actor->lastlook];

      // [crispy] monsters don't look for players with NOTARGET cheat
      if (player->cheats & CF_NOTARGET)
	continue;

      if (player->health <= 0)
	continue;               // dead

      if (unseen[actor->lastlook] || !P_IsVisible(actor, player->mo, allaround))
      {
	unseen[actor->lastlook] = true;
	continue;
      }
      
      P_SetTarget(&actor->target, player->mo);

      // killough 9/9/98: give monsters a threshold towards getting players
      // (we don't want it to be too easy for a player with dogs :)
      if (demo_version >= DV_MBF && !comp[comp_pursuit])
	actor->threshold = 60;

      return true;
    }
}

// 
// Friendly monsters, by Lee Killough 7/18/98
//
// Friendly monsters go after other monsters first, but 
// also return to owner if they cannot find any targets.
// A marine's best friend :)  killough 7/18/98, 9/98
//

static boolean P_LookForMonsters(mobj_t *actor, boolean allaround)
{
  thinker_t *cap, *th;

  if (demo_compatibility)
    return false;

  if (actor->lastenemy && actor->lastenemy->health > 0 && monsters_remember &&
      !(actor->lastenemy->flags & actor->flags & MF_FRIEND)) // not friends
    {
      P_SetTarget(&actor->target, actor->lastenemy);
      P_SetTarget(&actor->lastenemy, NULL);
      return true;
    }

  if (demo_version < DV_MBF)  // Old demos do not support monster-seeking bots
    return false;

  // Search the threaded list corresponding to this object's potential targets
  cap = &thinkerclasscap[actor->flags & MF_FRIEND ? th_enemies : th_friends];

  // Search for new enemy

  if (cap->cnext != cap)        // Empty list? bail out early
    {
      int x = (actor->x - bmaporgx)>>MAPBLOCKSHIFT;
      int y = (actor->y - bmaporgy)>>MAPBLOCKSHIFT;
      int d;

      current_actor = actor;
      current_allaround = allaround;

      // There is a bug in cl11+ that causes the player to get added
      //   to the monster friend list when damaged to below 50% health.
      // This causes all monsters to believe friend monsters exist.
      // The search algorithm is expensive and massively so on maps with many monsters.
      // We still need to match rng calls for demo sync, but PIT_FindTarget is a no op.
      if (((mobj_t *) cap->cnext)->player && cap->cnext == cap->cprev)
      {
        P_Random(pr_friends);
        return false;
      }

      // Search first in the immediate vicinity.

      if (!P_BlockThingsIterator(x, y, PIT_FindTarget, true))
	return true;

      for (d=1; d<5; d++)
	{
	  int i = 1 - d;
	  do
	    if (!P_BlockThingsIterator(x+i, y-d, PIT_FindTarget, true) ||
		!P_BlockThingsIterator(x+i, y+d, PIT_FindTarget, true))
	      return true;
	  while (++i < d);
	  do
	    if (!P_BlockThingsIterator(x-d, y+i, PIT_FindTarget, true) ||
		!P_BlockThingsIterator(x+d, y+i, PIT_FindTarget, true))
	      return true;
	  while (--i + d >= 0);
	}

      {   // Random number of monsters, to prevent patterns from forming
	int n = (P_Random(pr_friends) & 31) + 15;

	for (th = cap->cnext; th != cap; th = th->cnext)
	  if (--n < 0)
	    { 
	      // Only a subset of the monsters were searched. Move all of
	      // the ones which were searched so far, to the end of the list.

	      (cap->cnext->cprev = cap->cprev)->cnext = cap->cnext;
	      (cap->cprev = th->cprev)->cnext = cap;
	      (th->cprev = cap)->cnext = th;
	      break;
	   }
	  else
	    if (!PIT_FindTarget((mobj_t *) th))   // If target sighted
	      return true;
      }
    }

  return false;  // No monster found
}

//
// P_LookForTargets
//
// killough 9/5/98: look for targets to go after, depending on kind of monster
//

static boolean P_LookForTargets(mobj_t *actor, int allaround)
{
  return actor->flags & MF_FRIEND ?
    P_LookForMonsters(actor, allaround) || P_LookForPlayers (actor, allaround):
    P_LookForPlayers (actor, allaround) || P_LookForMonsters(actor, allaround);
}

//
// P_HelpFriend
//
// killough 9/8/98: Help friends in danger of dying
//

static boolean P_HelpFriend(mobj_t *actor)
{
  thinker_t *cap, *th;

  // If less than 33% health, self-preservation rules
  if (actor->health*3 < actor->info->spawnhealth)
    return false;

  current_actor = actor;
  current_allaround = true;

  // Possibly help a friend under 50% health
  cap = &thinkerclasscap[actor->flags & MF_FRIEND ? th_friends : th_enemies];

  for (th = cap->cnext; th != cap; th = th->cnext)
    if (((mobj_t *) th)->health*2 >= ((mobj_t *) th)->info->spawnhealth)
      {
	if (P_Random(pr_helpfriend) < 180)
	  break;
      }
    else
      if (((mobj_t *) th)->flags & MF_JUSTHIT &&
	  ((mobj_t *) th)->target && 
	  ((mobj_t *) th)->target != actor->target &&
	  !PIT_FindTarget(((mobj_t *) th)->target))
	{
	  // Ignore any attacking monsters, while searching for friend
	  actor->threshold = BASETHRESHOLD;
	  return true;
	}

  return false;
}

//
// ACTION ROUTINES
//

//
// A_Look
// Stay in state until a player is sighted.
//

void A_Look(mobj_t *actor)
{
  mobj_t *targ;

  targ = actor->subsector->sector->soundtarget;

  // [crispy] monsters don't look for players with NOTARGET cheat
  if (targ && targ->player && (targ->player->cheats & CF_NOTARGET))
    return;

  // killough 7/18/98:
  // Friendly monsters go after other monsters first, but 
  // also return to player, without attacking them, if they
  // cannot find any targets. A marine's best friend :)
  
  actor->threshold = actor->pursuecount = 0;
  if (!(actor->flags & MF_FRIEND && P_LookForTargets(actor, false)) &&
      !(targ &&
	targ->flags & MF_SHOOTABLE &&
	(P_SetTarget(&actor->target, targ),
	 !(actor->flags & MF_AMBUSH) || P_CheckSight(actor, targ))) &&
      (actor->flags & MF_FRIEND || !P_LookForTargets(actor, false)))
    return;

  // go into chase state

  if (actor->info->seesound)
    {
      int sound;
      switch (actor->info->seesound)
        {
        case sfx_posit1:
        case sfx_posit2:
        case sfx_posit3:
          sound = sfx_posit1+P_Random(pr_see)%3;
          break;

        case sfx_bgsit1:
        case sfx_bgsit2:
          sound = sfx_bgsit1+P_Random(pr_see)%2;
          break;

        default:
          sound = actor->info->seesound;
          break;
        }
      if (actor->flags2 & (MF2_BOSS | MF2_FULLVOLSOUNDS))
        S_StartSound(NULL, sound);          // full volume
      else
      {
        S_StartSound(actor, sound);
        // [FG] make seesounds uninterruptible
        if (full_sounds)
          S_UnlinkSound(actor);
      }
    }

  P_SetMobjState(actor, actor->info->seestate);
}

//
// A_KeepChasing
//
// killough 10/98:
// Allows monsters to continue movement while attacking
//

void A_KeepChasing(mobj_t *actor)
{
  if (actor->movecount)
    {
      actor->movecount--;
      if (actor->strafecount)
	actor->strafecount--;
      P_SmartMove(actor);
    }
}

//
// A_Chase
// Actor has a melee attack,
// so it tries to close as fast as possible
//

void A_Chase(mobj_t *actor)
{
  if (actor->reactiontime)
    actor->reactiontime--;

  // modify target threshold
  if (actor->threshold)
  {
    if (!actor->target || actor->target->health <= 0)
      actor->threshold = 0;
    else
      actor->threshold--;
  }

  // turn towards movement direction if not there yet
  // killough 9/7/98: keep facing towards target if strafing or backing out

  if (actor->strafecount)
    A_FaceTarget(actor);
  else
    if (actor->movedir < 8)
      {
	int delta = (actor->angle &= (7u<<29)) - (actor->movedir << 29);
	if (delta > 0)
	  actor->angle -= ANG90/2;
	else
	  if (delta < 0)
	    actor->angle += ANG90/2;
      }

  if (!actor->target || !(actor->target->flags&MF_SHOOTABLE))
    {    
      if (!P_LookForTargets(actor,true)) // look for a new target
	P_SetMobjState(actor, actor->info->spawnstate); // no new target
      return;
    }

  // do not attack twice in a row
  if (actor->flags & MF_JUSTATTACKED)
    {
      actor->flags &= ~MF_JUSTATTACKED;
      if (gameskill != sk_nightmare && !fastparm)
        P_NewChaseDir(actor);
      return;
    }

  // check for melee attack
  if (actor->info->meleestate && P_CheckMeleeRange(actor))
    {
      if (actor->info->attacksound)
        S_StartSound(actor, actor->info->attacksound);
      P_SetMobjState(actor, actor->info->meleestate);
      if (!actor->info->missilestate)
	actor->flags |= MF_JUSTHIT;   // killough 8/98: remember an attack
      return;
    }

  // check for missile attack
  if (actor->info->missilestate)
    if (!actor->movecount || gameskill >= sk_nightmare || fastparm)
      if (P_CheckMissileRange(actor))
        {
          P_SetMobjState(actor, actor->info->missilestate);
          actor->flags |= MF_JUSTATTACKED;
          return;
        }

  if (!actor->threshold)
  {
    if (demo_version < DV_MBF)
      {   // killough 9/9/98: for backward demo compatibility
	if (netgame && !P_CheckSight(actor, actor->target) &&
	    P_LookForPlayers(actor, true))
	  return;  
      }
    else  // killough 7/18/98, 9/9/98: new monster AI
      if (help_friends && P_HelpFriend(actor))
	return;      // killough 9/8/98: Help friends in need
      else  // Look for new targets if current one is bad or is out of view
	if (actor->pursuecount)
	  actor->pursuecount--;
	else
	  {
	    actor->pursuecount = BASETHRESHOLD;
	    
	    // If current target is bad and a new one is found, return:

	    if (!(actor->target && actor->target->health > 0 &&
		  ((comp[comp_pursuit] && !netgame) || 
		   (((actor->target->flags ^ actor->flags) & MF_FRIEND ||
		     (!(actor->flags & MF_FRIEND) && monster_infighting)) &&
		    P_CheckSight(actor, actor->target)))) &&
		P_LookForTargets(actor, true))
	      return;
	    
	    // (Current target was good, or no new target was found.)
	    //
	    // If monster is a missile-less friend, give up pursuit and
	    // return to player, if no attacks have occurred recently.

	    if (!actor->info->missilestate && actor->flags & MF_FRIEND)
	    {
	      if (actor->flags & MF_JUSTHIT)        // if recent action,
		actor->flags &= ~MF_JUSTHIT;        // keep fighting
	      else
		if (P_LookForPlayers(actor, true))  // else return to player
		  return;
	    }
	  }
  }
  
  if (actor->strafecount)
    actor->strafecount--;
  
  // chase towards player
  if (--actor->movecount<0 || !P_SmartMove(actor))
    P_NewChaseDir(actor);

  // make active sound
  if (actor->info->activesound && P_Random(pr_see)<3)
    S_StartSound(actor, actor->info->activesound);
}

//
// A_FaceTarget
//
void A_FaceTarget(mobj_t *actor)
{
  if (!actor->target)
    return;

  actor->flags &= ~MF_AMBUSH;
  actor->angle = R_PointToAngle2(actor->x, actor->y,
                                 actor->target->x, actor->target->y);
  if (actor->target->flags & MF_SHADOW)
    { // killough 5/5/98: remove dependence on order of evaluation:
      int t = P_Random(pr_facetarget);
      actor->angle += (t-P_Random(pr_facetarget))<<21;
    }
}

//
// A_PosAttack
//

void A_PosAttack(mobj_t *actor)
{
  int angle, damage, slope, t;

  if (!actor->target)
    return;
  A_FaceTarget(actor);
  angle = actor->angle;
  slope = P_AimLineAttack(actor, angle, MISSILERANGE, 0); // killough 8/2/98
  S_StartSound(actor, sfx_pistol);

  // killough 5/5/98: remove dependence on order of evaluation:
  t = P_Random(pr_posattack);
  angle += (t - P_Random(pr_posattack))<<20;
  damage = (P_Random(pr_posattack)%5 + 1)*3;
  P_LineAttack(actor, angle, MISSILERANGE, slope, damage);
}

void A_SPosAttack(mobj_t* actor)
{
  int i, bangle, slope;

  if (!actor->target)
    return;
  S_StartSound(actor, sfx_shotgn);
  A_FaceTarget(actor);
  bangle = actor->angle;
  slope = P_AimLineAttack(actor, bangle, MISSILERANGE, 0); // killough 8/2/98
  for (i=0; i<3; i++)
    {  // killough 5/5/98: remove dependence on order of evaluation:
      int t = P_Random(pr_sposattack);
      int angle = bangle + ((t - P_Random(pr_sposattack))<<20);
      int damage = ((P_Random(pr_sposattack)%5)+1)*3;
      P_LineAttack(actor, angle, MISSILERANGE, slope, damage);
    }
}

void A_CPosAttack(mobj_t *actor)
{
  int angle, bangle, damage, slope, t;

  if (!actor->target)
    return;
  S_StartSound(actor, sfx_shotgn);
  A_FaceTarget(actor);
  bangle = actor->angle;
  slope = P_AimLineAttack(actor, bangle, MISSILERANGE, 0); // killough 8/2/98

  // killough 5/5/98: remove dependence on order of evaluation:
  t = P_Random(pr_cposattack);
  angle = bangle + ((t - P_Random(pr_cposattack))<<20);
  damage = ((P_Random(pr_cposattack)%5)+1)*3;
  P_LineAttack(actor, angle, MISSILERANGE, slope, damage);
}

void A_CPosRefire(mobj_t *actor)
{
  // keep firing unless target got out of sight
  A_FaceTarget(actor);

  // killough 12/98: Stop firing if a friend has gotten in the way
  if (actor->flags & MF_FRIEND && P_HitFriend(actor))
    goto stop;

  // killough 11/98: prevent refiring on friends continuously
  if (P_Random(pr_cposrefire) < 40)
  {
    if (actor->target && actor->flags & actor->target->flags & MF_FRIEND)
      goto stop;
    else
      return;
  }

  if (!actor->target || actor->target->health <= 0
      || !P_CheckSight(actor, actor->target))
    stop: P_SetMobjState(actor, actor->info->seestate);
}

void A_SpidRefire(mobj_t* actor)
{
  // keep firing unless target got out of sight
  A_FaceTarget(actor);

  // killough 12/98: Stop firing if a friend has gotten in the way
  if (actor->flags & MF_FRIEND && P_HitFriend(actor))
    goto stop;

  if (P_Random(pr_spidrefire) < 10)
    return;

  // killough 11/98: prevent refiring on friends continuously
  if (!actor->target || actor->target->health <= 0
      || actor->flags & actor->target->flags & MF_FRIEND
      || !P_CheckSight(actor, actor->target))
    stop: P_SetMobjState(actor, actor->info->seestate);
}

void A_BspiAttack(mobj_t *actor)
{
  if (!actor->target)
    return;
  A_FaceTarget(actor);
  P_SpawnMissile(actor, actor->target, MT_ARACHPLAZ);  // launch a missile
}

//
// A_TroopAttack
//

void A_TroopAttack(mobj_t *actor)
{
  if (!actor->target)
    return;
  A_FaceTarget(actor);
  if (P_CheckMeleeRange(actor))
    {
      int damage;
      S_StartSound(actor, sfx_claw);
      damage = (P_Random(pr_troopattack)%8+1)*3;
      P_DamageMobjBy(actor->target, actor, actor, damage, MOD_Melee);
      return;
    }
  P_SpawnMissile(actor, actor->target, MT_TROOPSHOT);  // launch a missile
}

void A_SargAttack(mobj_t *actor)
{
  if (!actor->target)
    return;
  A_FaceTarget(actor);
  if (P_CheckMeleeRange(actor))
    {
      int damage = ((P_Random(pr_sargattack)%10)+1)*4;
      P_DamageMobjBy(actor->target, actor, actor, damage, MOD_Melee);
    }
}

void A_HeadAttack(mobj_t *actor)
{
  if (!actor->target)
    return;
  A_FaceTarget (actor);
  if (P_CheckMeleeRange(actor))
    {
      int damage = (P_Random(pr_headattack)%6+1)*10;
      P_DamageMobjBy(actor->target, actor, actor, damage, MOD_Melee);
      return;
    }
  P_SpawnMissile(actor, actor->target, MT_HEADSHOT);  // launch a missile
}

void A_CyberAttack(mobj_t *actor)
{
  if (!actor->target)
    return;
  A_FaceTarget(actor);
  P_SpawnMissile(actor, actor->target, MT_ROCKET);
}

void A_BruisAttack(mobj_t *actor)
{
  if (!actor->target)
    return;
  if (P_CheckMeleeRange(actor))
    {
      int damage;
      S_StartSound(actor, sfx_claw);
      damage = (P_Random(pr_bruisattack)%8+1)*10;
      P_DamageMobjBy(actor->target, actor, actor, damage, MOD_Melee);
      return;
    }
  P_SpawnMissile(actor, actor->target, MT_BRUISERSHOT);  // launch a missile
}

//
// A_SkelMissile
//

void A_SkelMissile(mobj_t *actor)
{
  mobj_t *mo;

  if (!actor->target)
    return;

  A_FaceTarget (actor);
  actor->z += 16*FRACUNIT;      // so missile spawns higher
  mo = P_SpawnMissile (actor, actor->target, MT_TRACER);
  actor->z -= 16*FRACUNIT;      // back to normal

  mo->x += mo->momx;
  mo->y += mo->momy;
  P_SetTarget(&mo->tracer, actor->target);  // killough 11/98
}

#define TRACEANGLE 0xc000000   /* killough 9/9/98: change to #define */

void A_Tracer(mobj_t *actor)
{
  angle_t       exact;
  fixed_t       dist;
  fixed_t       slope;
  mobj_t        *dest;
  mobj_t        *th;

  // killough 1/18/98: this is why some missiles do not have smoke
  // and some do. Also, internal demos start at random gametics, thus
  // the bug in which revenants cause internal demos to go out of sync.
  //
  // killough 3/6/98: fix revenant internal demo bug by subtracting
  // levelstarttic from gametic.
  //
  // killough 9/29/98: use new "basetic" so that demos stay in sync
  // during pauses and menu activations, while retaining old demo sync.
  //
  // leveltime would have been better to use to start with in Doom, but
  // since old demos were recorded using gametic, we must stick with it, 
  // and improvise around it (using leveltime causes desync across levels).

  if ((gametic - boom_basetic) & 3)
    return;

  // spawn a puff of smoke behind the rocket
  P_SpawnPuff(actor->x, actor->y, actor->z);

  th = P_SpawnMobj (actor->x-actor->momx,
                    actor->y-actor->momy,
                    actor->z, MT_SMOKE);

  th->momz = FRACUNIT;
  th->tics -= P_Random(pr_tracer) & 3;
  if (th->tics < 1)
    th->tics = 1;

  // adjust direction
  dest = actor->tracer;

  if (!dest || dest->health <= 0)
    return;

  // change angle
  exact = R_PointToAngle2(actor->x, actor->y, dest->x, dest->y);

  if (exact != actor->angle)
  {
    if (exact - actor->angle > 0x80000000)
      {
        actor->angle -= TRACEANGLE;
        if (exact - actor->angle < 0x80000000)
          actor->angle = exact;
      }
    else
      {
        actor->angle += TRACEANGLE;
        if (exact - actor->angle > 0x80000000)
          actor->angle = exact;
      }
  }

  exact = actor->angle>>ANGLETOFINESHIFT;
  actor->momx = FixedMul(actor->info->speed, finecosine[exact]);
  actor->momy = FixedMul(actor->info->speed, finesine[exact]);

  // change slope
  dist = P_AproxDistance(dest->x - actor->x, dest->y - actor->y);

  dist = dist / actor->info->speed;

  if (dist < 1)
    dist = 1;

  slope = (dest->z+40*FRACUNIT - actor->z) / dist;

  if (slope < actor->momz)
    actor->momz -= FRACUNIT/8;
  else
    actor->momz += FRACUNIT/8;
}

void A_SkelWhoosh(mobj_t *actor)
{
  if (!actor->target)
    return;
  A_FaceTarget(actor);
  S_StartSound(actor,sfx_skeswg);
}

void A_SkelFist(mobj_t *actor)
{
  if (!actor->target)
    return;
  A_FaceTarget(actor);
  if (P_CheckMeleeRange(actor))
    {
      int damage = ((P_Random(pr_skelfist)%10)+1)*6;
      S_StartSound(actor, sfx_skepch);
      P_DamageMobjBy(actor->target, actor, actor, damage, MOD_Melee);
    }
}

//
// PIT_VileCheck
// Detect a corpse that could be raised.
//

mobj_t* corpsehit;
mobj_t* vileobj;
fixed_t viletryx;
fixed_t viletryy;
int viletryradius;

boolean PIT_VileCheck(mobj_t *thing)
{
  int     maxdist;
  boolean check;

  if (!(thing->flags & MF_CORPSE))
    return true;        // not a monster

  if (thing->tics != -1)
    return true;        // not lying still yet

  if (thing->info->raisestate == S_NULL)
    return true;        // monster doesn't have a raise state

  maxdist = thing->info->radius + viletryradius;

  if (abs(thing->x-viletryx) > maxdist || abs(thing->y-viletryy) > maxdist)
  {
    return true;                // not actually touching
  }

// Check to see if the radius and height are zero. If they are      // phares
// then this is a crushed monster that has been turned into a       //   |
// gib. One of the options may be to ignore this guy.               //   V

// Option 1: the original, buggy method, -> ghost (compatibility)
// Option 2: ressurect the monster, but not as a ghost
// Option 3: ignore the gib

//    if (Option3)                                                  //   ^
//        if ((thing->height == 0) && (thing->radius == 0))         //   |
//            return true;                                          // phares

    corpsehit = thing;
    corpsehit->momx = corpsehit->momy = 0;
    if (comp[comp_vile])
      {                                                             // phares
        corpsehit->height <<= 2;                                    //   V
        check = P_CheckPosition(corpsehit,corpsehit->x,corpsehit->y);
        corpsehit->height >>= 2;
      }
    else
      {
        int height,radius;

        height = corpsehit->height; // save temporarily
        radius = corpsehit->radius; // save temporarily
        corpsehit->height = corpsehit->info->height;
        corpsehit->radius = corpsehit->info->radius;
        corpsehit->flags |= MF_SOLID;
        check = P_CheckPosition(corpsehit,corpsehit->x,corpsehit->y);
        corpsehit->height = height; // restore
        corpsehit->radius = radius; // restore                      //   ^
        corpsehit->flags &= ~MF_SOLID;
      }                                                             //   |
                                                                    // phares
    if (!check)
      return true;              // doesn't fit here
    return false;               // got one, so stop checking
}

//
// mbf21: P_HealCorpse
// Check for ressurecting a body
//

boolean ghost_monsters;

static void WatchResurrection(mobj_t* target, mobj_t* raiser)
{
  int i;

  if (raiser && (raiser->intflags & MIF_SPAWNED_BY_ICON))
  {
    target->intflags |= MIF_SPAWNED_BY_ICON;
  }

  if (((target->flags ^ MF_COUNTKILL) & (MF_FRIEND | MF_COUNTKILL)) ||
      (target->intflags & MIF_SPAWNED_BY_ICON))
  {
    return;
  }

  for (i = 0; i < MAXPLAYERS; ++i)
  {
    if (!playeringame[i] || players[i].killcount == 0)
    {
      continue;
    }

    if (players[i].killcount > 0)
    {
      players[i].maxkilldiscount++;
      return;
    }
  }
}

static boolean P_HealCorpse(mobj_t* actor, int radius, statenum_t healstate, sfxenum_t healsound)
{
  int xl, xh;
  int yl, yh;
  int bx, by;

  if (actor->movedir != DI_NODIR)
    {
      // check for corpses to raise
      viletryx =
        actor->x + actor->info->speed*xspeed[actor->movedir];
      viletryy =
        actor->y + actor->info->speed*yspeed[actor->movedir];

      xl = (viletryx - bmaporgx - MAXRADIUS*2)>>MAPBLOCKSHIFT;
      xh = (viletryx - bmaporgx + MAXRADIUS*2)>>MAPBLOCKSHIFT;
      yl = (viletryy - bmaporgy - MAXRADIUS*2)>>MAPBLOCKSHIFT;
      yh = (viletryy - bmaporgy + MAXRADIUS*2)>>MAPBLOCKSHIFT;

      vileobj = actor;
      viletryradius = radius;
      for (bx=xl ; bx<=xh ; bx++)
        {
          for (by=yl ; by<=yh ; by++)
            {
              // Call PIT_VileCheck to check
              // whether object is a corpse
              // that canbe raised.
              if (!P_BlockThingsIterator(bx, by, PIT_VileCheck, true))
                {
		  mobjinfo_t *info;

                  // got one!
                  mobj_t *temp = actor->target;
                  actor->target = corpsehit;
                  A_FaceTarget(actor);
                  actor->target = temp;

                  P_SetMobjState(actor, healstate);
                  S_StartSound(corpsehit, healsound);
                  info = corpsehit->info;

                  P_SetMobjState(corpsehit,info->raisestate);

                  if (comp[comp_vile])
                    corpsehit->height <<= 2;                        // phares
                  else                                              //   V
                    {
                      corpsehit->height = info->height; // fix Ghost bug
                      corpsehit->radius = info->radius; // fix Ghost bug
                    }                                               // phares

		  // killough 7/18/98: 
		  // friendliness is transferred from AV to raised corpse
		  corpsehit->flags = 
		    (info->flags & ~MF_FRIEND) | (actor->flags & MF_FRIEND);

		  WatchResurrection(corpsehit, actor);

		  // [crispy] resurrected pools of gore ("ghost monsters") are translucent
		  if (STRICTMODE(ghost_monsters) && corpsehit->height == 0
		      && corpsehit->radius == 0)
		  {
		      corpsehit->tranmap = GetNormalTranMap(66);
		      I_Printf(VB_WARNING, "A_VileChase: Resurrected ghost monster (%d) at (%d/%d)!",
		              corpsehit->type, corpsehit->x>>FRACBITS, corpsehit->y>>FRACBITS);
		  }

		  corpsehit->flags_extra &= ~MFX_COLOREDBLOOD;
		  corpsehit->bloodcolor = 0;

                  corpsehit->health = info->spawnhealth;
		  P_SetTarget(&corpsehit->target, NULL);  // killough 11/98

		  if (demo_version >= DV_MBF)
		    {         // kilough 9/9/98
		      P_SetTarget(&corpsehit->lastenemy, NULL);
		      corpsehit->flags &= ~MF_JUSTHIT;
		    }

		  // killough 8/29/98: add to appropriate thread
		  P_UpdateThinker(&corpsehit->thinker);

                  return true;
                }
            }
        }
    }
  return false;
}

//
// A_VileChase
// Check for ressurecting a body
//

void A_VileChase(mobj_t* actor)
{
  if (!P_HealCorpse(actor, mobjinfo[MT_VILE].radius, S_VILE_HEAL1, sfx_slop))
    A_Chase(actor);
}

//
// A_VileStart
//

void A_VileStart(mobj_t *actor)
{
  S_StartSound(actor, sfx_vilatk);
}

//
// A_Fire
// Keep fire in front of player unless out of sight
//

void A_StartFire(mobj_t *actor)
{
  S_StartSound(actor,sfx_flamst);
  A_Fire(actor);
}

void A_FireCrackle(mobj_t* actor)
{
  S_StartSound(actor,sfx_flame);
  A_Fire(actor);
}

void A_Fire(mobj_t *actor)
{
  mobj_t* target;
  unsigned an;
  mobj_t *dest = actor->tracer;

  if (!dest)
    return;

  target = P_SubstNullMobj(actor->target);

  // don't move it if the vile lost sight
  if (!P_CheckSight(target, dest) )
    return;

  an = dest->angle >> ANGLETOFINESHIFT;

  P_UnsetThingPosition(actor);
  actor->x = dest->x + FixedMul(24*FRACUNIT, finecosine[an]);
  actor->y = dest->y + FixedMul(24*FRACUNIT, finesine[an]);
  actor->z = dest->z;
  P_SetThingPosition(actor);
}

//
// A_VileTarget
// Spawn the hellfire
//

void A_VileTarget(mobj_t *actor)
{
  mobj_t *fog;

  if (!actor->target)
    return;

  A_FaceTarget(actor);

  // killough 12/98: fix Vile fog coordinates
  fog = P_SpawnMobj(actor->target->x,
                    demo_version < DV_MBF ? actor->target->x : actor->target->y,
                    actor->target->z,MT_FIRE);

  P_SetTarget(&actor->tracer, fog);   // killough 11/98
  P_SetTarget(&fog->target, actor);
  P_SetTarget(&fog->tracer, actor->target);
  A_Fire(fog);
}

//
// A_VileAttack
//

void A_VileJump(mobj_t *mo)
{
  mo->momz = 1000*FRACUNIT/mo->info->mass;
}

void A_VileAttack(mobj_t *actor)
{
  mobj_t *fire;
  int    an;

  if (!actor->target)
    return;

  A_FaceTarget(actor);

  if (!P_CheckSight(actor, actor->target))
    return;

  S_StartSound(actor, sfx_barexp);
  P_DamageMobj(actor->target, actor, actor, 20);
  A_VileJump(actor->target);

  an = actor->angle >> ANGLETOFINESHIFT;

  fire = actor->tracer;

  if (!fire)
    return;

  // move the fire between the vile and the player
  fire->x = actor->target->x - FixedMul (24*FRACUNIT, finecosine[an]);
  fire->y = actor->target->y - FixedMul (24*FRACUNIT, finesine[an]);
  P_RadiusAttack(fire, actor, 70, 70);
}

//
// Mancubus attack,
// firing three missiles (bruisers)
// in three different directions?
// Doesn't look like it.
//

#define FATSPREAD       (ANG90/8)

void A_FatRaise(mobj_t *actor)
{
  A_FaceTarget(actor);
  S_StartSound(actor, sfx_manatk);
}

void A_FatAttack1(mobj_t *actor)
{
  mobj_t *mo;
  mobj_t *target;
  int    an;

  A_FaceTarget(actor);

  // Change direction  to ...
  actor->angle += FATSPREAD;
  target = P_SubstNullMobj(actor->target);
  P_SpawnMissile(actor, target, MT_FATSHOT);

  mo = P_SpawnMissile (actor, target, MT_FATSHOT);
  mo->angle += FATSPREAD;
  an = mo->angle >> ANGLETOFINESHIFT;
  mo->momx = FixedMul(mo->info->speed, finecosine[an]);
  mo->momy = FixedMul(mo->info->speed, finesine[an]);
}

void A_FatAttack2(mobj_t *actor)
{
  mobj_t *mo;
  mobj_t *target;
  int    an;

  A_FaceTarget(actor);
  // Now here choose opposite deviation.
  actor->angle -= FATSPREAD;
  target = P_SubstNullMobj(actor->target);
  P_SpawnMissile(actor, target, MT_FATSHOT);

  mo = P_SpawnMissile(actor, target, MT_FATSHOT);
  mo->angle -= FATSPREAD*2;
  an = mo->angle >> ANGLETOFINESHIFT;
  mo->momx = FixedMul(mo->info->speed, finecosine[an]);
  mo->momy = FixedMul(mo->info->speed, finesine[an]);
}

void A_FatAttack3(mobj_t *actor)
{
  mobj_t *mo;
  mobj_t *target;
  int    an;

  A_FaceTarget(actor);

  target = P_SubstNullMobj(actor->target);

  mo = P_SpawnMissile(actor, target, MT_FATSHOT);
  mo->angle -= FATSPREAD/2;
  an = mo->angle >> ANGLETOFINESHIFT;
  mo->momx = FixedMul(mo->info->speed, finecosine[an]);
  mo->momy = FixedMul(mo->info->speed, finesine[an]);

  mo = P_SpawnMissile(actor, target, MT_FATSHOT);
  mo->angle += FATSPREAD/2;
  an = mo->angle >> ANGLETOFINESHIFT;
  mo->momx = FixedMul(mo->info->speed, finecosine[an]);
  mo->momy = FixedMul(mo->info->speed, finesine[an]);
}

//
// SkullAttack
// Fly at the player like a missile.
//
#define SKULLSPEED              (20*FRACUNIT)

void A_SkullAttack(mobj_t *actor)
{
  mobj_t  *dest;
  angle_t an;
  int     dist;

  if (!actor->target)
    return;

  dest = actor->target;
  actor->flags |= MF_SKULLFLY;

  // [FG] fix crash when attack sound is missing
  if (actor->info->attacksound)
  {
  S_StartSound(actor, actor->info->attacksound);
  }
  A_FaceTarget(actor);
  an = actor->angle >> ANGLETOFINESHIFT;
  actor->momx = FixedMul(SKULLSPEED, finecosine[an]);
  actor->momy = FixedMul(SKULLSPEED, finesine[an]);
  dist = P_AproxDistance(dest->x - actor->x, dest->y - actor->y);
  dist = dist / SKULLSPEED;

  if (dist < 1)
    dist = 1;
  actor->momz = (dest->z+(dest->height>>1) - actor->z) / dist;
}

//
// A_BetaSkullAttack()
// killough 10/98: this emulates the beta version's lost soul attacks
//

void A_BetaSkullAttack(mobj_t *actor)
{
  int damage;

  if (demo_version < DV_MBF)
    return;

  if (!actor->target || actor->target->type == MT_SKULL)
    return;
  // [FG] fix crash when attack sound is missing
  if (actor->info->attacksound)
  {
  S_StartSound(actor, actor->info->attacksound);
  }
  A_FaceTarget(actor);
  damage = (P_Random(pr_skullfly)%8+1)*actor->info->damage;
  P_DamageMobj(actor->target, actor, actor, damage);
}

void A_Stop(mobj_t *actor)
{
  if (demo_version < DV_MBF)
    return;
  actor->momx = actor->momy = actor->momz = 0;
}

//
// A_PainShootSkull
// Spawn a lost soul and launch it at the target
//

void A_PainShootSkull(mobj_t *actor, angle_t angle)
{
  fixed_t       x,y,z;
  mobj_t        *newmobj;
  angle_t       an;
  int           prestep;

// The original code checked for 20 skulls on the level,            // phares
// and wouldn't spit another one if there were. If not in           // phares
// compatibility mode, we remove the limit.                         // phares

  if (comp[comp_pain])  // killough 10/98: compatibility-optioned
    {
      // count total number of skulls currently on the level
      int count = 20;
      thinker_t *currentthinker;
      for (currentthinker = thinkercap.next;
           currentthinker != &thinkercap;
           currentthinker = currentthinker->next)
        if ((currentthinker->function.p1 == P_MobjThinker)
            && ((mobj_t *)currentthinker)->type == MT_SKULL)
	  if (--count < 0)         // killough 8/29/98: early exit
	    return;
    }

  // okay, there's room for another one

  an = angle >> ANGLETOFINESHIFT;

  prestep = 4*FRACUNIT + 3*(actor->info->radius + mobjinfo[MT_SKULL].radius)/2;

  x = actor->x + FixedMul(prestep, finecosine[an]);
  y = actor->y + FixedMul(prestep, finesine[an]);
  z = actor->z + 8*FRACUNIT;

  if (comp[comp_skull])   // killough 10/98: compatibility-optioned
    newmobj = P_SpawnMobj(x, y, z, MT_SKULL);                     // phares
  else                                                            //   V
    {
      // Check whether the Lost Soul is being fired through a 1-sided
      // wall or an impassible line, or a "monsters can't cross" line.
      // If it is, then we don't allow the spawn. This is a bug fix, but
      // it should be considered an enhancement, since it may disturb
      // existing demos, so don't do it in compatibility mode.

      if (Check_Sides(actor,x,y))
        return;

      newmobj = P_SpawnMobj(x, y, z, MT_SKULL);

      // Check to see if the new Lost Soul's z value is above the
      // ceiling of its new sector, or below the floor. If so, kill it.

      if ((newmobj->z >
           (newmobj->subsector->sector->ceilingheight - newmobj->height)) ||
          (newmobj->z < newmobj->subsector->sector->floorheight))
        {
          // kill it immediately
          P_DamageMobj(newmobj,actor,actor,10000);
          return;                                                 //   ^
        }                                                         //   |
     }                                                            // phares

  // killough 7/20/98: PEs shoot lost souls with the same friendliness
  newmobj->flags = (newmobj->flags & ~MF_FRIEND) | (actor->flags & MF_FRIEND);

  // killough 8/29/98: add to appropriate thread
  P_UpdateThinker(&newmobj->thinker);

  // Check for movements.
  // killough 3/15/98: don't jump over dropoffs:

  if (!P_TryMove(newmobj, newmobj->x, newmobj->y, false))
    {
      // kill it immediately
      P_DamageMobj(newmobj, actor, actor, 10000);
      return;
    }

  P_SetTarget(&newmobj->target, actor->target);
  A_SkullAttack(newmobj);
}

//
// A_PainAttack
// Spawn a lost soul and launch it at the target
//

void A_PainAttack(mobj_t *actor)
{
  if (!actor->target)
    return;
  A_FaceTarget(actor);
  A_PainShootSkull(actor, actor->angle);
}

void A_PainDie(mobj_t *actor)
{
  A_Fall(actor);
  A_PainShootSkull(actor, actor->angle+ANG90);
  A_PainShootSkull(actor, actor->angle+ANG180);
  A_PainShootSkull(actor, actor->angle+ANG270);
}

void A_Scream(mobj_t *actor)
{
  int sound;

  switch (actor->info->deathsound)
    {
    case 0:
      return;

    case sfx_podth1:
    case sfx_podth2:
    case sfx_podth3:
      sound = sfx_podth1 + P_Random(pr_scream)%3;
      break;

    case sfx_bgdth1:
    case sfx_bgdth2:
      sound = sfx_bgdth1 + P_Random(pr_scream)%2;
      break;

    default:
      sound = actor->info->deathsound;
      break;
    }

  // Check for bosses.
  if (actor->flags2 & (MF2_BOSS | MF2_FULLVOLSOUNDS))
    S_StartSound(NULL, sound); // full volume
  else
    S_StartSound(actor, sound);
}

void A_XScream(mobj_t *actor)
{
  S_StartSoundEx(actor, sfx_slop);
}

void A_Pain(mobj_t *actor)
{
  if (actor->info->painsound)
    S_StartSoundPain(actor, actor->info->painsound);
}

void A_Fall(mobj_t *actor)
{
  // actor is on ground, it can be walked over
  actor->flags &= ~MF_SOLID;
}

// killough 11/98: kill an object
void A_Die(mobj_t *actor)
{
  if (demo_version < DV_MBF)
    return;
  P_DamageMobj(actor, NULL, NULL, actor->health);
}

//
// A_Explode
//
void A_Explode(mobj_t *thingy)
{
  P_RadiusAttack(thingy, thingy->target, 128, 128);
}

//
// A_Detonate
// killough 8/9/98: same as A_Explode, except that the damage is variable
//

void A_Detonate(mobj_t *mo)
{
  if (demo_version < DV_MBF)
    return;
  P_RadiusAttack(mo, mo->target, mo->info->damage, mo->info->damage);
}

//
// killough 9/98: a mushroom explosion effect, sorta :)
// Original idea: Linguica
//

void A_Mushroom(mobj_t *actor)
{
  int i, j, n = actor->info->damage;

  // Mushroom parameters are part of code pointer's state
  fixed_t misc1 = actor->state->misc1 ? actor->state->misc1 : FRACUNIT*4;
  fixed_t misc2 = actor->state->misc2 ? actor->state->misc2 : FRACUNIT/2;

  if (demo_version < DV_MBF)
    return;
  A_Explode(actor);               // make normal explosion

  for (i = -n; i <= n; i += 8)    // launch mushroom cloud
    for (j = -n; j <= n; j += 8)
      {
	mobj_t target = *actor, *mo;
	target.x += i << FRACBITS;    // Aim in many directions from source
	target.y += j << FRACBITS;
	target.z += P_AproxDistance(i,j) * misc1;           // Aim fairly high
	mo = P_SpawnMissile(actor, &target, MT_FATSHOT);    // Launch fireball
	mo->momx = FixedMul(mo->momx, misc2);
	mo->momy = FixedMul(mo->momy, misc2);               // Slow down a bit
	mo->momz = FixedMul(mo->momz, misc2);
	mo->flags &= ~MF_NOGRAVITY;   // Make debris fall under gravity
      }
}

//
// A_BossDeath
// Possibly trigger special effects
// if on first boss level
//

void A_BossDeath(mobj_t *mo)
{
  thinker_t *th;
  line_t    junk;
  int       i;

  if (gamemapinfo && gamemapinfo->flags & MapInfo_BossActionClear)
  {
      return;
  }

  if (gamemapinfo && array_size(gamemapinfo->bossactions))
  {
      // make sure there is a player alive for victory
      for (i = 0; i < MAXPLAYERS; i++)
      {
          if (playeringame[i] && players[i].health > 0)
          {
              break;
          }
      }
      if (i == MAXPLAYERS)
      {
          return; // no one left alive, so do not end game
      }

      bossaction_t *bossaction;
      array_foreach(bossaction, gamemapinfo->bossactions)
      {
          if (bossaction->type == mo->type)
          {
              break;
          }
      }
      if (bossaction == array_end(gamemapinfo->bossactions))
      {
          return; // no matches found
      }

      // scan the remaining thinkers to see
      // if all bosses are dead
      for (th = thinkercap.next; th != &thinkercap; th = th->next)
      {
          if (th->function.p1 == P_MobjThinker)
          {
              mobj_t *mo2 = (mobj_t *)th;
              if (mo2 != mo && mo2->type == mo->type && mo2->health > 0)
              {
                  return; // other boss not dead
              }
          }
      }

      array_foreach(bossaction, gamemapinfo->bossactions)
      {
          if (bossaction->type == mo->type)
          {
              junk = *lines;
              junk.special = (short)bossaction->special;
              junk.args[0] = (short)bossaction->tag;
              // use special semantics for line activation to block problem
              // types.
              if (!P_UseSpecialLine(mo, &junk, 0, true))
              {
                  P_CrossSpecialLine(&junk, 0, mo, true);
              }
          }
      }

      return;
  }

  if (gamemode == commercial)
    {
      if (gamemap != 7)
        return;

      if (!(mo->flags2 & (MF2_MAP07BOSS1 | MF2_MAP07BOSS2)))
        return;
    }
  else
    {
      // [FG] game version specific differences
      if (demo_compatibility && gameversion < exe_ultimate)
      {
        if (gamemap != 8)
          return;

        if (mo->flags2 & MF2_E1M8BOSS && gameepisode != 1)
          return;
      }
      else
      switch(gameepisode)
        {
        case 1:
          if (gamemap != 8)
            return;

          if (!(mo->flags2 & MF2_E1M8BOSS))
            return;
          break;

        case 2:
          if (gamemap != 8)
            return;

          if (!(mo->flags2 & MF2_E2M8BOSS))
            return;
          break;

        case 3:
          if (gamemap != 8)
            return;

          if (!(mo->flags2 & MF2_E3M8BOSS))
            return;

          break;

        case 4:
          switch(gamemap)
            {
            case 6:
              if (!(mo->flags2 & MF2_E4M6BOSS))
                return;
              break;

            case 8:
              if (!(mo->flags2 & MF2_E4M8BOSS))
                return;
              break;

            default:
              return;
              break;
            }
          break;

        default:
          if (gamemap != 8)
            return;
          break;
        }

    }

  // make sure there is a player alive for victory
  for (i=0; i<MAXPLAYERS; i++)
    if (playeringame[i] && players[i].health > 0)
      break;

  if (i==MAXPLAYERS)
    return;     // no one left alive, so do not end game

  // scan the remaining thinkers to see
  // if all bosses are dead
  for (th = thinkercap.next ; th != &thinkercap ; th=th->next)
    if (th->function.p1 == P_MobjThinker)
      {
        mobj_t *mo2 = (mobj_t *) th;
        if (mo2 != mo && mo2->type == mo->type && mo2->health > 0)
          return;         // other boss not dead
      }

  // victory!
  if ( gamemode == commercial)
    {
      if (gamemap == 7)
        {
          if (mo->flags2 & MF2_MAP07BOSS1)
            {
              junk.args[0] = 666;
              EV_DoFloor(&junk,lowerFloorToLowest);
              return;
            }

          if (mo->flags2 & MF2_MAP07BOSS2)
            {
              junk.args[0] = 667;
              EV_DoFloor(&junk,raiseToTexture);
              return;
            }
        }
    }
  else
    {
      switch(gameepisode)
        {
        case 1:
          junk.args[0] = 666;
          EV_DoFloor(&junk, lowerFloorToLowest);
          return;
          break;

        case 4:
          switch(gamemap)
            {
            case 6:
              junk.args[0] = 666;
              EV_DoDoor(&junk, blazeOpen);
              return;
              break;

            case 8:
              junk.args[0] = 666;
              EV_DoFloor(&junk, lowerFloorToLowest);
              return;
              break;
            }
        }
    }
  G_ExitLevel();
}

void A_Hoof (mobj_t* mo)
{
  S_StartSound(mo, sfx_hoof);
  A_Chase(mo);
}

void A_Metal(mobj_t *mo)
{
  S_StartSound(mo, sfx_metal);
  A_Chase(mo);
}

void A_BabyMetal(mobj_t *mo)
{
  S_StartSound(mo, sfx_bspwlk);
  A_Chase(mo);
}

void A_OpenShotgun2(player_t *player, pspdef_t *psp)
{
  S_StartSound(player->mo, sfx_dbopn);
}

void A_LoadShotgun2(player_t *player, pspdef_t *psp)
{
  S_StartSound(player->mo, sfx_dbload);
}

void A_CloseShotgun2(player_t *player, pspdef_t *psp)
{
  S_StartSound(player->mo, sfx_dbcls);
  A_ReFire(player,psp);
}

// killough 2/7/98: Remove limit on icon landings:
mobj_t **braintargets;
int    numbraintargets_alloc;
int    numbraintargets;

struct brain_s brain;   // killough 3/26/98: global state of boss brain

// killough 3/26/98: initialize icon landings at level startup,
// rather than at boss wakeup, to prevent savegame-related crashes

void P_SpawnBrainTargets(void)  // killough 3/26/98: renamed old function
{
  thinker_t *thinker;

  // find all the target spots
  numbraintargets = 0;
  brain.targeton = 0;
  brain.easy = 0;           // killough 3/26/98: always init easy to 0

  for (thinker=thinkercap.next; thinker != &thinkercap; thinker=thinker->next)
    if (thinker->function.p1 == P_MobjThinker)
      {
        mobj_t *m = (mobj_t *) thinker;

        if (m->type == MT_BOSSTARGET )
          {   // killough 2/7/98: remove limit on icon landings:
            if (numbraintargets >= numbraintargets_alloc)
              braintargets = Z_Realloc(braintargets,
                      (numbraintargets_alloc = numbraintargets_alloc ?
                       numbraintargets_alloc*2 : 32) *sizeof *braintargets,
                      PU_STATIC, 0);
            braintargets[numbraintargets++] = m;
          }
      }
}

void A_BrainAwake(mobj_t *mo)
{
  S_StartSound(NULL,sfx_bossit); // killough 3/26/98: only generates sound now
}

void A_BrainPain(mobj_t *mo)
{
  S_StartSound(NULL,sfx_bospn);
}

void A_BrainScream(mobj_t *mo)
{
  int x;
  for (x=mo->x - 196*FRACUNIT ; x< mo->x + 320*FRACUNIT ; x+= FRACUNIT*8)
    {
      int y = mo->y - 320*FRACUNIT;
      int z = 128 + P_Random(pr_brainscream)*2*FRACUNIT;
      mobj_t *th = P_SpawnMobj (x,y,z, MT_ROCKET);
      th->momz = P_Random(pr_brainscream)*512;
      P_SetMobjState(th, S_BRAINEXPLODE1);
      th->tics -= P_Random(pr_brainscream)&7;
      if (th->tics < 1)
        th->tics = 1;
    }
  S_StartSound(NULL,sfx_bosdth);
}

void A_BrainExplode(mobj_t *mo)
{  // killough 5/5/98: remove dependence on order of evaluation:
  int t = P_Random(pr_brainexp);
  int x = mo->x + (t - P_Random(pr_brainexp))*2048;
  int y = mo->y;
  int z = 128 + P_Random(pr_brainexp)*2*FRACUNIT;
  mobj_t *th = P_SpawnMobj(x,y,z, MT_ROCKET);
  th->momz = P_Random(pr_brainexp)*512;
  P_SetMobjState(th, S_BRAINEXPLODE1);
  th->tics -= P_Random(pr_brainexp)&7;
  if (th->tics < 1)
    th->tics = 1;
}

void A_BrainDie(mobj_t *mo)
{
  G_ExitLevel();
}

void A_BrainSpit(mobj_t *mo)
{
  mobj_t *targ, *newmobj;

  if (!numbraintargets)     // killough 4/1/98: ignore if no targets
    return;

  brain.easy ^= 1;          // killough 3/26/98: use brain struct
  if (gameskill <= sk_easy && !brain.easy)
    return;

  // shoot a cube at current target
  targ = braintargets[brain.targeton++]; // killough 3/26/98:
  brain.targeton %= numbraintargets;     // Use brain struct for targets

  // spawn brain missile
  newmobj = P_SpawnMissile(mo, targ, MT_SPAWNSHOT);
  P_SetTarget(&newmobj->target, targ);
  newmobj->reactiontime = (short)(((targ->y-mo->y)/newmobj->momy)/newmobj->state->tics);

  // killough 7/18/98: brain friendliness is transferred
  newmobj->flags = (newmobj->flags & ~MF_FRIEND) | (mo->flags & MF_FRIEND);

  // killough 8/29/98: add to appropriate thread
  P_UpdateThinker(&newmobj->thinker);

  S_StartSound(NULL, sfx_bospit);
}

// travelling cube sound
void A_SpawnSound(mobj_t *mo)
{
  S_StartSound(mo,sfx_boscub);
  A_SpawnFly(mo);
}

static void WatchIconSpawn(mobj_t* spawned)
{
  spawned->intflags |= MIF_SPAWNED_BY_ICON;

  // We can't know inside P_SpawnMobj what the source is
  // This is less invasive than introducing a spawn source concept
  if (!((spawned->flags ^ MF_COUNTKILL) & (MF_FRIEND | MF_COUNTKILL)))
  {
    --max_kill_requirement;
  }
}

void A_SpawnFly(mobj_t *mo)
{
  mobj_t *newmobj;  // killough 8/9/98
  int    r;
  mobjtype_t type;

  mobj_t *fog;
  mobj_t *targ;

  if (--mo->reactiontime)
    return;     // still flying

  targ = P_SubstNullMobj(mo->target);

  // First spawn teleport fog.
  fog = P_SpawnMobj(targ->x, targ->y, targ->z, MT_SPAWNFIRE);

  S_StartSound(fog, sfx_telept);

  // Randomly select monster to spawn.
  r = P_Random(pr_spawnfly);

  // Probability distribution (kind of :), decreasing likelihood.
  if ( r<50 )
    type = MT_TROOP;
  else if (r<90)
    type = MT_SERGEANT;
  else if (r<120)
    type = MT_SHADOWS;
  else if (r<130)
    type = MT_PAIN;
  else if (r<160)
    type = MT_HEAD;
  else if (r<162)
    type = MT_VILE;
  else if (r<172)
    type = MT_UNDEAD;
  else if (r<192)
    type = MT_BABY;
  else if (r<222)
    type = MT_FATSO;
  else if (r<246)
    type = MT_KNIGHT;
  else
    type = MT_BRUISER;

  newmobj = P_SpawnMobj(targ->x, targ->y, targ->z, type);

  // killough 7/18/98: brain friendliness is transferred
  newmobj->flags = (newmobj->flags & ~MF_FRIEND) | (mo->flags & MF_FRIEND);

  WatchIconSpawn(newmobj);

  // killough 8/29/98: add to appropriate thread
  P_UpdateThinker(&newmobj->thinker);

  if (P_LookForTargets(newmobj,true))      // killough 9/4/98
    P_SetMobjState(newmobj, newmobj->info->seestate);

    // telefrag anything in this spot
  P_TeleportMove(newmobj, newmobj->x, newmobj->y, true); // killough 8/9/98

  // remove self (i.e., cube).
  P_RemoveMobj(mo);
}

void A_PlayerScream(mobj_t *mo)
{
  int sound = sfx_pldeth;  // Default death sound.
  if (gamemode != shareware && mo->health < -50) // killough 12/98
    sound = sfx_pdiehi;   // IF THE PLAYER DIES LESS THAN -50% WITHOUT GIBBING
  S_StartSoundEx(mo, sound);
}

//
// A_KeenDie
// DOOM II special, map 32.
// Uses special tag 666.
//
void A_KeenDie(mobj_t* mo)
{
  thinker_t *th;
  line_t   junk;

  A_Fall(mo);

  // scan the remaining thinkers to see if all Keens are dead

  for (th = thinkercap.next ; th != &thinkercap ; th=th->next)
    if (th->function.p1 == P_MobjThinker)
      {
        mobj_t *mo2 = (mobj_t *) th;
        if (mo2 != mo && mo2->type == mo->type && mo2->health > 0)
          return;                           // other Keen not dead
      }

  junk.args[0] = 666;
  EV_DoDoor(&junk,doorOpen);
}

//
// killough 11/98
//
// The following were inspired by Len Pitre
//
// A small set of highly-sought-after code pointers
//

void A_Spawn(mobj_t *mo)
{
  if (demo_version < DV_MBF)
    return;
  if (mo->state->misc1)
    {
      mobj_t *newmobj = P_SpawnMobj(mo->x, mo->y, 
				    (mo->state->misc2 << FRACBITS) + mo->z, 
				    mo->state->misc1 - 1);

      if (comp[comp_friendlyspawn])
      {
      newmobj->flags = (newmobj->flags & ~MF_FRIEND) | (mo->flags & MF_FRIEND);
      }
    }
}

void A_Turn(mobj_t *mo)
{
  if (demo_version < DV_MBF)
    return;
  mo->angle += (angle_t)(((uint64_t) mo->state->misc1 << 32) / 360);
}

void A_Face(mobj_t *mo)
{
  if (demo_version < DV_MBF)
    return;
  mo->angle = (angle_t)(((uint64_t) mo->state->misc1 << 32) / 360);
}

void A_Scratch(mobj_t *mo)
{
  if (demo_version < DV_MBF)
    return;
  mo->target && (A_FaceTarget(mo), P_CheckMeleeRange(mo)) ?
    mo->state->misc2 ? S_StartSound(mo, mo->state->misc2) : (void) 0,
    P_DamageMobjBy(mo->target, mo, mo, mo->state->misc1, MOD_Melee) : (void) 0;
}

void A_PlaySound(mobj_t *mo)
{
  if (demo_version < DV_MBF)
    return;
  S_StartSoundOrigin(mo, mo->state->misc2 ? NULL : mo, mo->state->misc1);
}

void A_RandomJump(mobj_t *mo)
{
  if (demo_version < DV_MBF)
    return;
  if (P_Random(pr_randomjump) < mo->state->misc2)
    P_SetMobjState(mo, mo->state->misc1);
}

//
// This allows linedef effects to be activated inside deh frames.
//

void A_LineEffect(mobj_t *mo)
{
  if (demo_version < DV_MBF)
    return;
  if (!(mo->intflags & MIF_LINEDONE))                // Unless already used up
    {
      line_t junk = *lines;                          // Fake linedef set to 1st
      if ((junk.special = (short)mo->state->misc1))  // Linedef type
	{
	  // [FG] made static
	  static player_t player;                    // Remember player status
	  player_t *oldplayer = mo->player;          // Remember player status
	  mo->player = &player;                      // Fake player
	  player.health = 100;                       // Alive player
	  junk.args[0] = (short)mo->state->misc2;    // Sector tag for linedef
	  if (!P_UseSpecialLine(mo, &junk, 0, false))// Try using it
	    P_CrossSpecialLine(&junk, 0, mo, false); // Try crossing it
	  if (!junk.special)                         // If type cleared,
	    mo->intflags |= MIF_LINEDONE;            // no more for this thing
	  mo->player = oldplayer;                    // Restore player status
	}
    }
}

//
// [XA] New mbf21 codepointers
//

//
// A_SpawnObject
// Basically just A_Spawn with better behavior and more args.
//   args[0]: Type of actor to spawn
//   args[1]: Angle (degrees, in fixed point), relative to calling actor's angle
//   args[2]: X spawn offset (fixed point), relative to calling actor
//   args[3]: Y spawn offset (fixed point), relative to calling actor
//   args[4]: Z spawn offset (fixed point), relative to calling actor
//   args[5]: X velocity (fixed point)
//   args[6]: Y velocity (fixed point)
//   args[7]: Z velocity (fixed point)
//
void A_SpawnObject(mobj_t *actor)
{
  int type, angle, ofs_x, ofs_y, ofs_z, vel_x, vel_y, vel_z;
  angle_t an;
  int fan, dx, dy;
  mobj_t *mo;

  if (!mbf21 || !actor->state->args[0])
    return;

  type  = actor->state->args[0] - 1;
  angle = actor->state->args[1];
  ofs_x = actor->state->args[2];
  ofs_y = actor->state->args[3];
  ofs_z = actor->state->args[4];
  vel_x = actor->state->args[5];
  vel_y = actor->state->args[6];
  vel_z = actor->state->args[7];

  // calculate position offsets
  an = actor->angle + (angle_t)(((int64_t)angle << 16) / 360);
  fan = an >> ANGLETOFINESHIFT;
  dx = FixedMul(ofs_x, finecosine[fan]) - FixedMul(ofs_y, finesine[fan]  );
  dy = FixedMul(ofs_x, finesine[fan]  ) + FixedMul(ofs_y, finecosine[fan]);

  // spawn it, yo
  mo = P_SpawnMobj(actor->x + dx, actor->y + dy, actor->z + ofs_z, type);
  if (!mo)
    return;

  // angle dangle
  mo->angle = an;

  // set velocity
  mo->momx = FixedMul(vel_x, finecosine[fan]) - FixedMul(vel_y, finesine[fan]  );
  mo->momy = FixedMul(vel_x, finesine[fan]  ) + FixedMul(vel_y, finecosine[fan]);
  mo->momz = vel_z;

  // if spawned object is a missile, set target+tracer
  if (mo->info->flags & (MF_MISSILE | MF_BOUNCES))
  {
    // if spawner is also a missile, copy 'em
    if (actor->info->flags & (MF_MISSILE | MF_BOUNCES))
    {
      P_SetTarget(&mo->target, actor->target);
      P_SetTarget(&mo->tracer, actor->tracer);
    }
    // otherwise, set 'em as if a monster fired 'em
    else
    {
      P_SetTarget(&mo->target, actor);
      P_SetTarget(&mo->tracer, actor->target);
    }
  }

  // [XA] don't bother with the dont-inherit-friendliness hack
  // that exists in A_Spawn, 'cause WTF is that about anyway?
}

//
// A_MonsterProjectile
// A parameterized monster projectile attack.
//   args[0]: Type of actor to spawn
//   args[1]: Angle (degrees, in fixed point), relative to calling actor's angle
//   args[2]: Pitch (degrees, in fixed point), relative to calling actor's pitch; approximated
//   args[3]: X/Y spawn offset, relative to calling actor's angle
//   args[4]: Z spawn offset, relative to actor's default projectile fire height
//
void A_MonsterProjectile(mobj_t *actor)
{
  int type, angle, pitch, spawnofs_xy, spawnofs_z;
  mobj_t *mo;
  int an;

  if (!mbf21 || !actor->target || !actor->state->args[0])
    return;

  type        = actor->state->args[0] - 1;
  angle       = actor->state->args[1];
  pitch       = actor->state->args[2];
  spawnofs_xy = actor->state->args[3];
  spawnofs_z  = actor->state->args[4];

  A_FaceTarget(actor);
  mo = P_SpawnMissile(actor, actor->target, type);
  if (!mo)
    return;

  // adjust angle
  mo->angle += (angle_t)(((int64_t)angle << 16) / 360);
  an = mo->angle >> ANGLETOFINESHIFT;
  mo->momx = FixedMul(mo->info->speed, finecosine[an]);
  mo->momy = FixedMul(mo->info->speed, finesine[an]);

  // adjust pitch (approximated, using Doom's ye olde
  // finetangent table; same method as monster aim)
  mo->momz += FixedMul(mo->info->speed, DegToSlope(pitch));

  // adjust position
  an = (actor->angle - ANG90) >> ANGLETOFINESHIFT;
  mo->x += FixedMul(spawnofs_xy, finecosine[an]);
  mo->y += FixedMul(spawnofs_xy, finesine[an]);
  mo->z += spawnofs_z;

  // always set the 'tracer' field, so this pointer
  // can be used to fire seeker missiles at will.
  P_SetTarget(&mo->tracer, actor->target);
}

//
// A_MonsterBulletAttack
// A parameterized monster bullet attack.
//   args[0]: Horizontal spread (degrees, in fixed point)
//   args[1]: Vertical spread (degrees, in fixed point)
//   args[2]: Number of bullets to fire; if not set, defaults to 1
//   args[3]: Base damage of attack (e.g. for 3d5, customize the 3); if not set, defaults to 3
//   args[4]: Attack damage modulus (e.g. for 3d5, customize the 5); if not set, defaults to 5
//
void A_MonsterBulletAttack(mobj_t *actor)
{
  int hspread, vspread, numbullets, damagebase, damagemod;
  int aimslope, i, damage, angle, slope;

  if (!mbf21 || !actor->target)
    return;

  hspread    = actor->state->args[0];
  vspread    = actor->state->args[1];
  numbullets = actor->state->args[2];
  damagebase = actor->state->args[3];
  damagemod  = actor->state->args[4];

  A_FaceTarget(actor);
  S_StartSound(actor, actor->info->attacksound);

  aimslope = P_AimLineAttack(actor, actor->angle, MISSILERANGE, 0);

  for (i = 0; i < numbullets; i++)
  {
    damage = (P_Random(pr_mbf21) % damagemod + 1) * damagebase;
    angle = (int)actor->angle + P_RandomHitscanAngle(pr_mbf21, hspread);
    slope = aimslope + P_RandomHitscanSlope(pr_mbf21, vspread);

    P_LineAttack(actor, angle, MISSILERANGE, slope, damage);
  }
}

//
// A_MonsterMeleeAttack
// A parameterized monster melee attack.
//   args[0]: Base damage of attack (e.g. for 3d8, customize the 3); if not set, defaults to 3
//   args[1]: Attack damage modulus (e.g. for 3d8, customize the 8); if not set, defaults to 8
//   args[2]: Sound to play if attack hits
//   args[3]: Range (fixed point); if not set, defaults to monster's melee range
//
void A_MonsterMeleeAttack(mobj_t *actor)
{
  int damagebase, damagemod, hitsound, range;
  int damage;

  if (!mbf21 || !actor->target)
    return;

  damagebase = actor->state->args[0];
  damagemod  = actor->state->args[1];
  hitsound   = actor->state->args[2];
  range      = actor->state->args[3];

  if (range == 0)
    range = actor->info->meleerange;

  range += actor->target->info->radius - 20 * FRACUNIT;

  A_FaceTarget(actor);
  if (!P_CheckRange(actor, range))
    return;

  S_StartSound(actor, hitsound);

  damage = (P_Random(pr_mbf21) % damagemod + 1) * damagebase;
  P_DamageMobjBy(actor->target, actor, actor, damage, MOD_Melee);
}

//
// A_RadiusDamage
// A parameterized version of A_Explode. Friggin' finally. :P
//   args[0]: Damage (int)
//   args[1]: Radius (also int; no real need for fractional precision here)
//
void A_RadiusDamage(mobj_t *actor)
{
  if (!mbf21 || !actor->state)
    return;

  P_RadiusAttack(actor, actor->target, actor->state->args[0], actor->state->args[1]);
}

//
// A_NoiseAlert
// Alerts nearby monsters (via sound) to the calling actor's target's presence.
//
void A_NoiseAlert(mobj_t *actor)
{
  if (!mbf21 || !actor->target)
    return;

  P_NoiseAlert(actor->target, actor);
}

//
// A_HealChase
// A parameterized version of A_VileChase.
//   args[0]: State to jump to on the calling actor when resurrecting a corpse
//   args[1]: Sound to play when resurrecting a corpse
//
void A_HealChase(mobj_t* actor)
{
  int state, sound;

  if (!mbf21 || !actor)
    return;

  state = actor->state->args[0];
  sound = actor->state->args[1];

  if (!P_HealCorpse(actor, actor->info->radius, state, sound))
    A_Chase(actor);
}

//
// A_SeekTracer
// A parameterized seeker missile function.
//   args[0]: direct-homing threshold angle (degrees, in fixed point)
//   args[1]: maximum turn angle (degrees, in fixed point)
//
void A_SeekTracer(mobj_t *actor)
{
  angle_t threshold, maxturnangle;

  if (!mbf21 || !actor)
    return;

  threshold    = FixedToAngle(actor->state->args[0]);
  maxturnangle = FixedToAngle(actor->state->args[1]);

  P_SeekerMissile(actor, &actor->tracer, threshold, maxturnangle, true);
}

//
// A_FindTracer
// Search for a valid tracer (seek target), if the calling actor doesn't already have one.
//   args[0]: field-of-view to search in (degrees, in fixed point); if zero, will search in all directions
//   args[1]: distance to search (map blocks, i.e. 128 units)
//
void A_FindTracer(mobj_t *actor)
{
  angle_t fov;
  int dist;

  if (!mbf21 || !actor || actor->tracer)
    return;

  fov  = FixedToAngle(actor->state->args[0]);
  dist =             (actor->state->args[1]);

  P_SetTarget(&actor->tracer, P_RoughTargetSearch(actor, fov, dist));
}

//
// A_ClearTracer
// Clear current tracer (seek target).
//
void A_ClearTracer(mobj_t *actor)
{
  if (!mbf21 || !actor)
    return;

  P_SetTarget(&actor->tracer, NULL);
}

//
// A_JumpIfHealthBelow
// Jumps to a state if caller's health is below the specified threshold.
//   args[0]: State to jump to
//   args[1]: Health threshold
//
void A_JumpIfHealthBelow(mobj_t* actor)
{
  int state, health;

  if (!mbf21 || !actor)
    return;

  state  = actor->state->args[0];
  health = actor->state->args[1];

  if (actor->health < health)
    P_SetMobjState(actor, state);
}

//
// A_JumpIfTargetInSight
// Jumps to a state if caller's target is in line-of-sight.
//   args[0]: State to jump to
//   args[1]: Field-of-view to check (degrees, in fixed point); if zero, will check in all directions
//
void A_JumpIfTargetInSight(mobj_t* actor)
{
  int state;
  angle_t fov;

  if (!mbf21 || !actor || !actor->target)
    return;

  state =             (actor->state->args[0]);
  fov   = FixedToAngle(actor->state->args[1]);

  // Check FOV first since it's faster
  if (fov > 0 && !P_CheckFov(actor, actor->target, fov))
    return;

  if (P_CheckSight(actor, actor->target))
    P_SetMobjState(actor, state);
}

//
// A_JumpIfTargetCloser
// Jumps to a state if caller's target is closer than the specified distance.
//   args[0]: State to jump to
//   args[1]: Distance threshold
//
void A_JumpIfTargetCloser(mobj_t* actor)
{
  int state, distance;

  if (!mbf21 || !actor || !actor->target)
    return;

  state    = actor->state->args[0];
  distance = actor->state->args[1];

  if (distance > P_AproxDistance(actor->x - actor->target->x,
                                 actor->y - actor->target->y))
    P_SetMobjState(actor, state);
}

//
// A_JumpIfTracerInSight
// Jumps to a state if caller's tracer (seek target) is in line-of-sight.
//   args[0]: State to jump to
//   args[1]: Field-of-view to check (degrees, in fixed point); if zero, will check in all directions
//
void A_JumpIfTracerInSight(mobj_t* actor)
{
  angle_t fov;
  int state;

  if (!mbf21 || !actor || !actor->tracer)
    return;

  state =             (actor->state->args[0]);
  fov   = FixedToAngle(actor->state->args[1]);

  // Check FOV first since it's faster
  if (fov > 0 && !P_CheckFov(actor, actor->tracer, fov))
    return;

  if (P_CheckSight(actor, actor->tracer))
    P_SetMobjState(actor, state);
}

//
// A_JumpIfTracerCloser
// Jumps to a state if caller's tracer (seek target) is closer than the specified distance.
//   args[0]: State to jump to
//   args[1]: Distance threshold (fixed point)
//
void A_JumpIfTracerCloser(mobj_t* actor)
{
  int state, distance;

  if (!mbf21 || !actor || !actor->tracer)
    return;

  state    = actor->state->args[0];
  distance = actor->state->args[1];

  if (distance > P_AproxDistance(actor->x - actor->tracer->x,
                                 actor->y - actor->tracer->y))
    P_SetMobjState(actor, state);
}

//
// A_JumpIfFlagsSet
// Jumps to a state if caller has the specified thing flags set.
//   args[0]: State to jump to
//   args[1]: Standard Flag(s) to check
//   args[2]: MBF21 Flag(s) to check
//
void A_JumpIfFlagsSet(mobj_t* actor)
{
  int state;
  unsigned int flags, flags2;

  if (!mbf21 || !actor)
    return;

  state  = actor->state->args[0];
  flags  = actor->state->args[1];
  flags2 = actor->state->args[2];

  if ((actor->flags & flags) == flags &&
      (actor->flags2 & flags2) == flags2)
    P_SetMobjState(actor, state);
}

//
// A_AddFlags
// Adds the specified thing flags to the caller.
//   args[0]: Standard Flag(s) to add
//   args[1]: MBF21 Flag(s) to add
//
void A_AddFlags(mobj_t* actor)
{
  unsigned int flags, flags2;
  boolean update_blockmap;

  if (!mbf21 || !actor)
    return;

  flags  = actor->state->args[0];
  flags2 = actor->state->args[1];

  // unlink/relink the thing from the blockmap if
  // the NOBLOCKMAP or NOSECTOR flags are added
  update_blockmap = ((flags & MF_NOBLOCKMAP) && !(actor->flags & MF_NOBLOCKMAP))
                    || ((flags & MF_NOSECTOR) && !(actor->flags & MF_NOSECTOR));

  if (update_blockmap)
    P_UnsetThingPosition(actor);

  actor->flags  |= flags;
  actor->flags2 |= flags2;

  if (update_blockmap)
    P_SetThingPosition(actor);
}

//
// A_RemoveFlags
// Removes the specified thing flags from the caller.
//   args[0]: Flag(s) to remove
//   args[1]: MBF21 Flag(s) to remove
//
void A_RemoveFlags(mobj_t* actor)
{
  unsigned int flags, flags2;
  boolean update_blockmap;

  if (!mbf21 || !actor)
    return;

  flags  = actor->state->args[0];
  flags2 = actor->state->args[1];

  // unlink/relink the thing from the blockmap if
  // the NOBLOCKMAP or NOSECTOR flags are removed
  update_blockmap = ((flags & MF_NOBLOCKMAP) && (actor->flags & MF_NOBLOCKMAP))
                    || ((flags & MF_NOSECTOR) && (actor->flags & MF_NOSECTOR));

  if (update_blockmap)
    P_UnsetThingPosition(actor);

  actor->flags  &= ~flags;
  actor->flags2 &= ~flags2;

  if (update_blockmap)
    P_SetThingPosition(actor);
}

//----------------------------------------------------------------------------
//
// $Log: p_enemy.c,v $
// Revision 1.22  1998/05/12  12:47:10  phares
// Removed OVER_UNDER code
//
// Revision 1.21  1998/05/07  00:50:55  killough
// beautification, remove dependence on evaluation order
//
// Revision 1.20  1998/05/03  22:28:02  killough
// beautification, move declarations and includes around
//
// Revision 1.19  1998/04/01  12:58:44  killough
// Disable boss brain if no targets
//
// Revision 1.18  1998/03/28  17:57:05  killough
// Fix boss spawn savegame bug
//
// Revision 1.17  1998/03/23  15:18:03  phares
// Repaired AV ghosts stuck together bug
//
// Revision 1.16  1998/03/16  12:33:12  killough
// Use new P_TryMove()
//
// Revision 1.15  1998/03/09  07:17:58  killough
// Fix revenant tracer bug
//
// Revision 1.14  1998/03/02  11:40:52  killough
// Use separate monsters_remember flag instead of bitmask
//
// Revision 1.13  1998/02/24  08:46:12  phares
// Pushers, recoil, new friction, and over/under work
//
// Revision 1.12  1998/02/23  04:43:44  killough
// Add revenant p_atracer, optioned monster ai_vengence
//
// Revision 1.11  1998/02/17  06:04:55  killough
// Change RNG calling sequences
// Fix minor icon landing bug
// Use lastenemy to make monsters remember former targets, and fix player look
//
// Revision 1.10  1998/02/09  03:05:22  killough
// Remove icon landing limit
//
// Revision 1.9  1998/02/05  12:15:39  phares
// tighten lost soul wall fix to compatibility
//
// Revision 1.8  1998/02/02  13:42:54  killough
// Relax lost soul wall fix to demo_compatibility
//
// Revision 1.7  1998/01/28  13:21:01  phares
// corrected Option3 in AV bug
//
// Revision 1.6  1998/01/28  12:22:17  phares
// AV bug fix and Lost Soul trajectory bug fix
//
// Revision 1.5  1998/01/26  19:24:00  phares
// First rev with no ^Ms
//
// Revision 1.4  1998/01/23  14:51:51  phares
// No content change. Put ^Ms back.
//
// Revision 1.3  1998/01/23  14:42:14  phares
// No content change. Removed ^Ms for experimental checkin.
//
// Revision 1.2  1998/01/19  14:45:01  rand
// Temporary line for checking checkins
//
// Revision 1.1.1.1  1998/01/19  14:02:59  rand
// Lee's Jan 19 sources
//
//----------------------------------------------------------------------------
