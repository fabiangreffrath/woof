//
//  Copyright (C) 2026 Fabian Greffrath
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

// Returns true if mo is solid and shootable and either the active player or not
// a player at all.
static boolean passable(const mobj_t *const mo)
{
    return (mo->flags & (MF_SOLID | MF_SHOOTABLE)) == (MF_SOLID | MF_SHOOTABLE)
           && (!mo->player || mo->player->mo == mo);
}

// Returns true if a and b may pass over or under each other, depending on the
// option.
boolean P_CanOverUnder(const mobj_t *const a, const mobj_t *const b)
{
    const boolean a_player = a->player != NULL;
    const boolean b_player = b->player != NULL;
    const int mode = CRITICAL(overunder);

    return mode != OVERUNDER_OFF && passable(a) && passable(b)
           && !(a_player && b_player)
           && (mode == OVERUNDER_ALL || a_player || b_player);
}

// Returns true if mo is an enemy monster, including lost souls but not friends,
// barrels or the boss brain.
static boolean monster(const mobj_t *const mo)
{
    return !(mo->flags & MF_FRIEND)
           && ((mo->flags & MF_COUNTKILL) || mo->type == MT_SKULL);
}

// Kills mo with crusher damage.
static void crush(mobj_t *const mo)
{
    P_DamageMobjBy(mo, NULL, NULL, 10000, MOD_Crush);
}

// Returns the thing that set tmfloorz in the last P_CheckPosition(), or NULL.
static mobj_t *floor_thing(void)
{
    return tmbelow && tmfloorz == tmbelow->z + tmbelow->height ? tmbelow : NULL;
}

// Returns the thing that set tmceilingz in the last P_CheckPosition(), or NULL.
static mobj_t *ceiling_thing(void)
{
    return tmabove && tmceilingz == tmabove->z ? tmabove : NULL;
}

// Links lower and upper to each other.
static void join(mobj_t *const lower, mobj_t *const upper)
{
    P_SetTarget(&lower->above_thing, upper);
    P_SetTarget(&upper->below_thing, lower);
}

// Links mo to the things it touches that determined its floor and ceiling in
// the last P_CheckPosition(). Stale links of other things to mo are left alone,
// they drop them themselves in P_UpdateOverUnder().
void P_SetOverUnderLinks(mobj_t *mo)
{
    mobj_t *const below = floor_thing();
    mobj_t *const above = ceiling_thing();

    if (below && mo->z == tmfloorz)
    {
        join(below, mo);
    }
    else
    {
        P_SetTarget(&mo->below_thing, NULL);
    }

    if (above && mo->z + mo->height == tmceilingz)
    {
        join(mo, above);
    }
    else
    {
        P_SetTarget(&mo->above_thing, NULL);
    }
}

// Returns true if upper still touches or intersects lower from above.
static boolean linked(const mobj_t *const lower, const mobj_t *const upper)
{
    const fixed_t blockdist = lower->radius + upper->radius;

    return lower->thinker.function.p1 == P_MobjThinker
           && upper->thinker.function.p1 == P_MobjThinker
           && P_CanOverUnder(lower, upper)
           && abs(lower->x - upper->x) < blockdist
           && abs(lower->y - upper->y) < blockdist && upper->z >= lower->z
           && upper->z <= lower->z + lower->height;
}

// Removes the links of mo itself, e.g. before it is removed from the game.
void P_UnlinkOverUnder(mobj_t *mo)
{
    P_SetTarget(&mo->above_thing, NULL);
    P_SetTarget(&mo->below_thing, NULL);
}

// Returns the thing above mo if the link to it still holds, or NULL.
static const mobj_t *linked_above(const mobj_t *const mo)
{
    return mo->above_thing && linked(mo, mo->above_thing) ? mo->above_thing
                                                          : NULL;
}

// Returns the thing below mo if the link to it still holds, or NULL.
static const mobj_t *linked_below(const mobj_t *const mo)
{
    return mo->below_thing && linked(mo->below_thing, mo) ? mo->below_thing
                                                          : NULL;
}

// Returns true if thing is a player standing over or under a monster.
boolean P_IsOverUnderPlayer(const mobj_t *const thing)
{
    return thing->player && (linked_above(thing) || linked_below(thing));
}

// Returns false if a and b may pass over or under each other but their heights
// are more than a step apart, so that they cannot hit each other in melee.
boolean P_CheckOverUnderHeight(const mobj_t *const a, const mobj_t *const b)
{
    const fixed_t reach = 24 * FRACUNIT;

    return !P_CanOverUnder(a, b)
           || (a->z <= b->z + b->height + reach
               && b->z <= a->z + a->height + reach);
}

// Kills a living monster that stands over another thing or that the player
// stands over or under, even if it would still fit. Returns true if the monster
// was killed.
boolean P_CrushOverUnderLink(mobj_t *thing)
{
    const mobj_t *const above = linked_above(thing);
    const mobj_t *const below = linked_below(thing);

    if (thing->health <= 0 || thing->player || !monster(thing)
        || !(below || (above && above->player)))
    {
        return false;
    }

    crush(thing);
    return true;
}

// Kills the monsters that set the floor and ceiling of a player in the last
// P_CheckPosition(). Returns true if any of them was killed.
boolean P_CrushOverUnderBlockers(mobj_t *thing)
{
    mobj_t *const below = floor_thing();
    mobj_t *const above = ceiling_thing();
    const boolean kill_below = below && below->health > 0 && monster(below);
    const boolean kill_above = above && above->health > 0 && monster(above);

    if (!thing->player || thing->player->mo != thing)
    {
        return false;
    }

    if (kill_below)
    {
        crush(below);
    }

    if (kill_above)
    {
        crush(above);
    }

    return kill_below || kill_above;
}

// Drops links that no longer hold and recomputes floor and ceiling so that mo
// can fall. Called once per tic for things that have links.
void P_UpdateOverUnder(mobj_t *mo)
{
    if (linked_below(mo) == mo->below_thing
        && linked_above(mo) == mo->above_thing)
    {
        return;
    }

    P_CheckPosition(mo, mo->x, mo->y);
    mo->floorz = tmfloorz;
    mo->ceilingz = tmceilingz;
    mo->dropoffz = tmdropoffz;
    P_SetOverUnderLinks(mo);
}
