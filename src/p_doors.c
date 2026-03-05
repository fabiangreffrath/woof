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
//   Door animation code (opening/closing)
//
//-----------------------------------------------------------------------------

#include "d_player.h"
#include "d_think.h"
#include "deh_strings.h"
#include "doomdata.h"
#include "doomdef.h"
#include "doomstat.h"
#include "i_printf.h"
#include "m_fixed.h"
#include "p_dirty.h"
#include "p_mobj.h"
#include "p_spec.h"
#include "p_tick.h"
#include "r_defs.h"
#include "r_state.h"
#include "s_sound.h"
#include "sounds.h"

///////////////////////////////////////////////////////////////
//
// Door action routines, called once per tick
//
///////////////////////////////////////////////////////////////

//
// T_VerticalDoor
//
// Passed a door structure containing all info about the door.
// See P_SPEC.H for fields.
// Returns nothing.
//
// jff 02/08/98 all cases with labels beginning with gen added to support
// generalized line type behaviors.

static void T_VerticalDoor(vldoor_t *door)
{
  result_e  res;

  // Is the door waiting, going up, or going down?
  switch(door->direction)
    {
    case 0:
      // Door is waiting
      if (!--door->topcountdown)  // downcount and check
        switch(door->type)
          {
          case blazeRaise:
          case genBlazeRaise:
            door->direction = -1; // time to go back down
            S_StartSound((mobj_t *)&door->sector->soundorg,sfx_bdcls);
            break;

          case doorNormal:
          case genRaise:
            door->direction = -1; // time to go back down
            S_StartSound((mobj_t *)&door->sector->soundorg,sfx_dorcls);
            break;

          case close30ThenOpen:
          case genCdO:
            door->direction = 1;  // time to go back up
            S_StartSound((mobj_t *)&door->sector->soundorg,sfx_doropn);
            break;

          case genBlazeCdO:
            door->direction = 1;  // time to go back up
            S_StartSound((mobj_t *)&door->sector->soundorg,sfx_bdopn);
            break;

          default:
            break;
          }
      break;

    case 2:
      // Special case for sector type door that opens in 5 mins
      if (!--door->topcountdown)  // 5 minutes up?
        switch(door->type)
          {
          case raiseIn5Mins:
            door->direction = 1;  // time to raise then
            door->type = doorNormal;  // door acts just like normal 1 DR door now
            S_StartSound((mobj_t *)&door->sector->soundorg,sfx_doropn);
            break;

          default:
            break;
          }
      break;

    case -1:
      // Door is moving down
      res = T_MovePlane(door->sector, door->speed,
                        door->sector->floorheight,
                        false, 1, door->direction);

      // killough 10/98: implement gradual lighting effects
      if (door->lighttag && door->topheight - door->sector->floorheight)
        EV_LightTurnOnPartway(door->line,
                              FixedDiv(door->sector->ceilingheight -
                                       door->sector->floorheight,
                                       door->topheight -
                                       door->sector->floorheight));

      // handle door reaching bottom
      if (res == pastdest)
        switch(door->type)
          {
            // regular open and close doors are all done, remove them
          case blazeRaise:
          case blazeClose:
          case genBlazeRaise:
          case genBlazeClose:
            door->sector->ceilingdata = NULL;  //jff 2/22/98
            P_RemoveDoorThinker(door);  // unlink and free
            // killough 4/15/98: remove double-closing sound of blazing doors
            if (STRICTMODE_COMP(comp_blazing))
              S_StartSound((mobj_t *)&door->sector->soundorg,sfx_bdcls);
            break;

          case doorNormal:
          case doorClose:
          case genRaise:
          case genClose:
            door->sector->ceilingdata = NULL; //jff 2/22/98
            P_RemoveDoorThinker(door);  // unlink and free
            break;

            // close then open doors start waiting
          case close30ThenOpen:
            door->direction = 0;
            door->topcountdown = TICRATE*30;
            break;

          case genCdO:
          case genBlazeCdO:
            door->direction = 0;
            door->topcountdown = door->topwait; // jff 5/8/98 insert delay
            break;

          default:
            break;
          }

      //jff 1/31/98 turn lighting off in tagged sectors of manual doors
      // killough 10/98: replaced with gradual lighting code

      else
        if (res == crushed) // handle door meeting obstruction on way down
          switch(door->type)
            {
            case genClose:
            case genBlazeClose:
            case blazeClose:
            case doorClose:          // Close types do not bounce, merely wait
              break;

            case blazeRaise:
            case genBlazeRaise:
              door->direction = 1;
              if (!STRICTMODE_COMP(comp_blazing))
              {
                S_StartSound((mobj_t *)&door->sector->soundorg,sfx_bdopn);
                break;
              }
              // fallthrough

            default:             // other types bounce off the obstruction
              door->direction = 1;
              S_StartSound((mobj_t *)&door->sector->soundorg,sfx_doropn);
              break;
            }
      break;

    case 1:
      // Door is moving up
      res = T_MovePlane(door->sector, door->speed,
                        door->topheight, false, 1,
                        door->direction);

      // killough 10/98: implement gradual lighting effects
      if (door->lighttag && door->topheight - door->sector->floorheight)
        EV_LightTurnOnPartway(door->line,
                              FixedDiv(door->sector->ceilingheight -
                                       door->sector->floorheight,
                                       door->topheight -
                                       door->sector->floorheight));

      // handle door reaching the top
      if (res == pastdest)
        switch(door->type)
          {
          case blazeRaise:       // regular open/close doors start waiting
          case doorNormal:
          case genRaise:
          case genBlazeRaise:
            door->direction = 0; // wait at top with delay
            door->topcountdown = door->topwait;
            break;

          case close30ThenOpen:  // close and close/open doors are done
          case blazeOpen:
          case doorOpen:
          case genBlazeOpen:
          case genOpen:
          case genCdO:
          case genBlazeCdO:
            door->sector->ceilingdata = NULL; //jff 2/22/98
            P_RemoveDoorThinker(door); // unlink and free
            break;

          default:
            break;
          }

      //jff 1/31/98 turn lighting on in tagged sectors of manual doors
      // killough 10/98: replaced with gradual lighting code
      break;
    }
}

void T_VerticalDoorAdapter(mobj_t *mobj)
{
    T_VerticalDoor((vldoor_t *)mobj);
}

///////////////////////////////////////////////////////////////
//
// Door linedef handlers
//
///////////////////////////////////////////////////////////////

//
// EV_DoLockedDoor
//
// Handle opening a tagged locked door
//
// Passed the line activating the door, the type of door,
// and the thing that activated the line
// Returns true if a thinker created
//
int EV_DoLockedDoor(line_t *line, vldoor_e type, mobj_t *thing)
{
  player_t *p = thing->player;

  if (!p)          // only players can open locked doors
    return 0;

  // check type of linedef, and if key is possessed to open it
  switch(line->special)
    {
    case 99:  // Blue Lock
    case 133:
      if (!p->cards[it_bluecard] && !p->cards[it_blueskull])
        {
          doomprintf(p, MESSAGES_NONE, DEH_StringColorized(PD_BLUEO));
          S_StartSound(p->mo,sfx_oof);                  // killough 3/20/98
          return 0;
        }
      break;

    case 134: // Red Lock
    case 135:
      if (!p->cards[it_redcard] && !p->cards[it_redskull])
        {
          doomprintf(p, MESSAGES_NONE, DEH_StringColorized(PD_REDO));
          S_StartSound(p->mo,sfx_oof);                // killough 3/20/98
          return 0;
        }
      break;

    case 136: // Yellow Lock
    case 137:
      if (!p->cards[it_yellowcard] && !p->cards[it_yellowskull])
        {
          doomprintf(p, MESSAGES_NONE, DEH_StringColorized(PD_YELLOWO));
          S_StartSound(p->mo,sfx_oof);                    // killough 3/20/98
          return 0;
        }
      break;
    }

  // got the key, so open the door
  return EV_DoDoor(line,type);
}

//
// EV_DoDoor
//
// Handle opening a tagged door
//
// Passed the line activating the door and the type of door
// Returns true if a thinker created
//

int EV_DoDoor(line_t *line, vldoor_e type)
{
  int secnum = -1, rtn = 0;
  sector_t *sec;
  vldoor_t *door;

  // open all doors with the same tag as the activating line
  while ((secnum = P_FindSectorFromLineTag(line,secnum)) >= 0)
    {
      sec = &sectors[secnum];
      // if the ceiling already moving, don't start the door action
      if (P_SectorActive(ceiling_special,sec)) //jff 2/22/98
        continue;

      // new door thinker
      rtn = 1;
      door = arena_alloc(thinkers_arena, vldoor_t);
      P_AddThinker(&door->thinker);
      sec->ceilingdata = door; //jff 2/22/98

      door->thinker.function.p1 = T_VerticalDoorAdapter;
      door->sector = sec;
      door->type = type;
      door->topwait = VDOORWAIT;
      door->speed = VDOORSPEED;
      door->line = line;  // jff 1/31/98 remember line that triggered us
      door->lighttag = 0; // killough 10/98: no light effects with tagged doors

      // setup door parameters according to type of door
      switch(type)
        {
        case blazeClose:
          door->topheight = P_FindLowestCeilingSurrounding(sec);
          door->topheight -= 4*FRACUNIT;
          door->direction = -1;
          door->speed = VDOORSPEED * 4;
          S_StartSound((mobj_t *)&door->sector->soundorg,sfx_bdcls);
          break;

        case doorClose:
          door->topheight = P_FindLowestCeilingSurrounding(sec);
          door->topheight -= 4*FRACUNIT;
          door->direction = -1;
          S_StartSound((mobj_t *)&door->sector->soundorg,sfx_dorcls);
          break;

        case close30ThenOpen:
          door->topheight = sec->ceilingheight;
          door->direction = -1;
          S_StartSound((mobj_t *)&door->sector->soundorg,sfx_dorcls);
          break;

        case blazeRaise:
        case blazeOpen:
          door->direction = 1;
          door->topheight = P_FindLowestCeilingSurrounding(sec);
          door->topheight -= 4*FRACUNIT;
          door->speed = VDOORSPEED * 4;
          if (door->topheight != sec->ceilingheight)
            S_StartSound((mobj_t *)&door->sector->soundorg,sfx_bdopn);
          break;

        case doorNormal:
        case doorOpen:
          door->direction = 1;
          door->topheight = P_FindLowestCeilingSurrounding(sec);
          door->topheight -= 4*FRACUNIT;
          if (door->topheight != sec->ceilingheight)
            S_StartSound((mobj_t *)&door->sector->soundorg,sfx_doropn);
          break;

        default:
          break;
        }
    }
  return rtn;
}


//
// EV_VerticalDoor
//
// Handle opening a door manually, no tag value
//
// Passed the line activating the door and the thing activating it
// Returns true if a thinker created
//
// jff 2/12/98 added int return value, fixed all returns
//

int EV_VerticalDoor(line_t *line, mobj_t *thing)
{
  player_t* player;
  sector_t* sec;
  vldoor_t* door;

  //  Check for locks
  player = thing->player;

  switch(line->special)
    {
    case 26: // Blue Lock
    case 32:
      if (!player)
        return 0;
      if (!player->cards[it_bluecard] && !player->cards[it_blueskull])
        {
          doomprintf(player, MESSAGES_NONE, DEH_StringColorized(PD_BLUEK));
          S_StartSound(player->mo,sfx_oof);             // killough 3/20/98
          return 0;
        }
      break;

    case 27: // Yellow Lock
    case 34:
      if ( !player )
        return 0;
      if (!player->cards[it_yellowcard] && !player->cards[it_yellowskull])
        {
          doomprintf(player, MESSAGES_NONE, DEH_StringColorized(PD_YELLOWK));
          S_StartSound(player->mo,sfx_oof);               // killough 3/20/98
          return 0;
        }
      break;

    case 28: // Red Lock
    case 33:
      if ( !player )
        return 0;
      if (!player->cards[it_redcard] && !player->cards[it_redskull])
        {
          doomprintf(player, MESSAGES_NONE, DEH_StringColorized(PD_REDK));
          S_StartSound(player->mo,sfx_oof);           // killough 3/20/98
          return 0;
        }
      break;

    default:
      break;
    }

  // if the wrong side of door is pushed, give oof sound
  if (line->sidenum[1]==NO_INDEX)                       // killough
    {
      S_StartSound(player->mo,sfx_oof);           // killough 3/20/98
      return 0;
    }

  // get the sector on the second side of activating linedef
  sec = sides[line->sidenum[1]].sector;

  // if door already has a thinker, use it
  door = sec->ceilingdata; //jff 2/22/98

  /* cph 2001/04/05 -
   * Ok, this is a disaster area. We're assuming that sec->ceilingdata
   *  is a vldoor_t! What if this door is controlled by both DR lines
   *  and by switches? I don't know how to fix that.
   * Secondly, original Doom didn't distinguish floor/lighting/ceiling
   *  actions, so we need to do the same in demo compatibility mode.
   */
  if (demo_compatibility)
  {
    if (!door)
    {
      door = sec->floordata;
    }
  }
  /* If this is a repeatable line, and the door is already moving, then we can
   * just reverse the current action. Note that in prboom 2.3.0 I erroneously
   * removed the if-this-is-repeatable check, hence the prboom_4_compatibility
   * clause below (foolishly assumed that already moving implies repeatable -
   * but it could be moving due to another switch, e.g. lv19-509) */
  if (door
      && ((line->special == 1) || (line->special == 117)
          || (line->special == 26) || (line->special == 27)
          || (line->special == 28)))
  {
    /* For old demos we have to emulate the old buggy behavior and
     * mess up non-T_VerticalDoor actions.
     */
    if (demo_version < DV_MBF21
        || door->thinker.function.p1 == T_VerticalDoorAdapter)
    {
      /* cph - we are writing outval to door->direction iff it is non-zero */
      signed int outval = 0;

      /* An already moving repeatable door which is being re-pressed, or a
       * monster is trying to open a closing door - so change direction
       * DEMOSYNC: we only read door->direction now if it really is a door.
       */
      if (door->thinker.function.p1 == T_VerticalDoorAdapter
          && door->direction == -1)
      {
        outval = 1; /* go back up */
      }
      else if (player)
      {
        outval = -1; /* go back down */
      }

      /* Write this to the thinker. In demo compatibility mode, we might be
       *  overwriting a field of a non-vldoor_t thinker - we need to add any
       *  other thinker types here if any demos depend on specific fields
       *  being corrupted by this.
       */
      if (outval)
      {
        if (door->thinker.function.p1 == T_VerticalDoorAdapter)
        {
          door->direction = outval;
        }
        else if (door->thinker.function.p1 == T_PlatRaiseAdapter)
        {
          plat_t *p = (plat_t *)door;
          p->wait = outval;
        }
        else
        {
          I_Printf(VB_WARNING, "EV_VerticalDoor: unknown thinker.function in "
                               "thinker corruption emulation");
        }

        return 1;
      }
    }
    /* It's a door but we're a monster and don't want to shut it;
     * exit with no action.
     */
    return 0;
  }

  // emit proper sound
  switch(line->special)
    {
    case 117: // blazing door raise
    case 118: // blazing door open
      S_StartSound((mobj_t *)&sec->soundorg,sfx_bdopn);
      break;

    case 1:   // normal door sound
    case 31:
      S_StartSound((mobj_t *)&sec->soundorg,sfx_doropn);
      break;

    default:  // locked door sound
      S_StartSound((mobj_t *)&sec->soundorg,sfx_doropn);
      break;
    }

  // new door thinker
  door = arena_alloc(thinkers_arena, vldoor_t);
  P_AddThinker (&door->thinker);
  sec->ceilingdata = door; //jff 2/22/98
  door->thinker.function.p1 = T_VerticalDoorAdapter;
  door->sector = sec;
  door->direction = 1;
  door->speed = VDOORSPEED;
  door->topwait = VDOORWAIT;
  door->line = line; // jff 1/31/98 remember line that triggered us

  // killough 10/98: use gradual lighting changes if nonzero tag given
  door->lighttag = STRICTMODE_COMP(comp_doorlight) ? 0 : line->args[0]; // killough 10/98

  // set the type of door from the activating linedef type
  switch(line->special)
    {
    case 1:
    case 26:
    case 27:
    case 28:
      door->type = doorNormal;
      break;

    case 31:
    case 32:
    case 33:
    case 34:
      door->type = doorOpen;
      dirty_line(line)->special = 0;
      break;

    case 117: // blazing door raise
      door->type = blazeRaise;
      door->speed = VDOORSPEED*4;
      break;

    case 118: // blazing door open
      door->type = blazeOpen;
      dirty_line(line)->special = 0;
      door->speed = VDOORSPEED*4;
      break;

    default:
      door->lighttag = 0;   // killough 10/98
      break;
    }

  // find the top and bottom of the movement range
  door->topheight = P_FindLowestCeilingSurrounding(sec);
  door->topheight -= 4*FRACUNIT;
  return 1;
}


///////////////////////////////////////////////////////////////
//
// Sector type door spawners
//
///////////////////////////////////////////////////////////////

//
// P_SpawnDoorCloseIn30()
//
// Spawn a door that closes after 30 seconds (called at level init)
//
// Passed the sector of the door, whose type specified the door action
// Returns nothing

void P_SpawnDoorCloseIn30 (sector_t* sec)
{
  vldoor_t *door = arena_alloc(thinkers_arena, vldoor_t);

  P_AddThinker (&door->thinker);

  sec->ceilingdata = door; //jff 2/22/98
  sec->special = 0;

  door->thinker.function.p1 = T_VerticalDoorAdapter;
  door->sector = sec;
  door->direction = 0;
  door->type = doorNormal;
  door->speed = VDOORSPEED;
  door->topcountdown = 30 * 35;
  door->line = NULL; // jff 1/31/98 remember line that triggered us
  door->lighttag = 0;  // killough 10/98: no lighting changes
}

//
// P_SpawnDoorRaiseIn5Mins()
//
// Spawn a door that opens after 5 minutes (called at level init)
//
// Passed the sector of the door, whose type specified the door action
// Returns nothing
//

void P_SpawnDoorRaiseIn5Mins(sector_t *sec, int secnum)
{
  vldoor_t* door;

  door = arena_alloc(thinkers_arena, vldoor_t);

  P_AddThinker (&door->thinker);

  sec->ceilingdata = door; //jff 2/22/98
  sec->special = 0;

  door->thinker.function.p1 = T_VerticalDoorAdapter;
  door->sector = sec;
  door->direction = 2;
  door->type = raiseIn5Mins;
  door->speed = VDOORSPEED;
  door->topheight = P_FindLowestCeilingSurrounding(sec);
  door->topheight -= 4*FRACUNIT;
  door->topwait = VDOORWAIT;
  door->topcountdown = 5 * 60 * 35;
  door->line = NULL; // jff 1/31/98 remember line that triggered us
  door->lighttag = 0;  // killough 10/98: no lighting changes
}

//----------------------------------------------------------------------------
//
// $Log: p_doors.c,v $
// Revision 1.13  1998/05/09  12:16:29  jim
// formatted/documented p_doors
//
// Revision 1.12  1998/05/03  23:07:16  killough
// Fix #includes at the top, remove #if 0, nothing else
//
// Revision 1.11  1998/04/16  06:28:34  killough
// Remove double-closing sound of blazing doors
//
// Revision 1.10  1998/03/28  05:32:36  jim
// Text enabling changes for DEH
//
// Revision 1.9  1998/03/23  03:24:53  killough
// Make door-opening 'oof' sound have true source
//
// Revision 1.8  1998/03/10  07:08:16  jim
// Extended manual door lighting to generalized doors
//
// Revision 1.7  1998/02/23  23:46:40  jim
// Compatibility flagged multiple thinker support
//
// Revision 1.6  1998/02/23  00:41:36  jim
// Implemented elevators
//
// Revision 1.5  1998/02/13  03:28:25  jim
// Fixed W1,G1 linedefs clearing untriggered special, cosmetic changes
//
// Revision 1.4  1998/02/08  05:35:23  jim
// Added generalized linedef types
//
// Revision 1.2  1998/01/26  19:23:58  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:02:59  rand
// Lee's Jan 19 sources
//
//----------------------------------------------------------------------------
