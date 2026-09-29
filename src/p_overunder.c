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
//      Optional Z collision between things ("over/under" monsters).
//      Uses the above_thing/below_thing pointers Boom reserved in mobj_t
//      but never made use of.
//
//-----------------------------------------------------------------------------

#include <stdlib.h>

#include "doomstat.h"
#include "hu_obituary.h"
#include "p_inter.h"
#include "p_map.h"
#include "p_mobj.h"
#include "p_overunder.h"
#include "p_tick.h"

int overunder;

mobj_t *tmbelow, *tmabove;

// Returns true if a player and a solid shootable non-player thing may pass over or under each other.
boolean P_CanOverUnder(const mobj_t *const a, const mobj_t *const b)
{
  const boolean a_player = a->player && a->player->mo == a;
  const boolean b_player = b->player && b->player->mo == b;
  const mobj_t *const other = a_player ? b : a;

  return CRITICAL(overunder) == OVERUNDER_PLAYER && a_player != b_player
         && !other->player
         && (other->flags & (MF_SOLID | MF_SHOOTABLE)) == (MF_SOLID | MF_SHOOTABLE);
}

// Links mo to the things that determined its floor and ceiling in the last P_CheckPosition().
// Stale links of other things to mo are left alone, they drop them themselves in P_UpdateOverUnder().
void P_SetOverUnderLinks(mobj_t *mo)
{
  if (tmbelow && tmfloorz == tmbelow->z + tmbelow->height)
  {
    P_SetTarget(&mo->below_thing, tmbelow);
    P_SetTarget(&tmbelow->above_thing, mo);
  }
  else
    P_SetTarget(&mo->below_thing, NULL);

  if (tmabove && tmceilingz == tmabove->z)
  {
    P_SetTarget(&mo->above_thing, tmabove);
    P_SetTarget(&tmabove->below_thing, mo);
  }
  else
    P_SetTarget(&mo->above_thing, NULL);
}

// Returns true if the link between lower and upper still holds.
static boolean linked(const mobj_t *const lower, const mobj_t *const upper)
{
  const fixed_t blockdist = lower->radius + upper->radius;

  return lower->thinker.function.p1 == P_MobjThinker
         && upper->thinker.function.p1 == P_MobjThinker
         && P_CanOverUnder(lower, upper)
         && abs(lower->x - upper->x) < blockdist
         && abs(lower->y - upper->y) < blockdist
         && (upper->floorz == lower->z + lower->height
             || lower->ceilingz == upper->z);
}

// Removes the links of mo itself, e.g. before it is removed from the game.
void P_UnlinkOverUnder(mobj_t *mo)
{
  P_SetTarget(&mo->above_thing, NULL);
  P_SetTarget(&mo->below_thing, NULL);
}

// Returns the thing mo is still linked to, or NULL.
static const mobj_t *partner(const mobj_t *const mo)
{
  if (mo->above_thing && linked(mo, mo->above_thing))
    return mo->above_thing;

  if (mo->below_thing && linked(mo->below_thing, mo))
    return mo->below_thing;

  return NULL;
}

// Returns true if thing is a living monster the player stands over or under.
boolean P_IsOverUnderMonster(const mobj_t *const thing)
{
  const mobj_t *const other = partner(thing);

  return thing->health > 0 && !thing->player && other && other->player;
}

// Returns true if thing is a player standing over or under a monster.
boolean P_IsOverUnderPlayer(const mobj_t *const thing)
{
  return thing->player && partner(thing) != NULL;
}

// Kills a living monster the player stands over or under, even if it would still fit.
// Returns true if the monster was killed.
boolean P_CrushOverUnderLink(mobj_t *thing)
{
  if (!P_IsOverUnderMonster(thing))
    return false;

  P_DamageMobjBy(thing, NULL, NULL, 10000, MOD_Crush);
  return true;
}

// Drops links that no longer hold and recomputes floor and ceiling so that mo can fall.
// Called once per tic for things that have links.
void P_UpdateOverUnder(mobj_t *mo)
{
  if ((!mo->below_thing || linked(mo->below_thing, mo))
      && (!mo->above_thing || linked(mo, mo->above_thing)))
    return;

  P_CheckPosition(mo, mo->x, mo->y);
  mo->floorz = tmfloorz;
  mo->ceilingz = tmceilingz;
  mo->dropoffz = tmdropoffz;
  P_SetOverUnderLinks(mo);
}
