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
//      Player related stuff.
//      Bobbing POV/weapon, movement.
//      Pending weapon.
//
//-----------------------------------------------------------------------------

#include <stdlib.h>

#include "d_event.h"
#include "d_player.h"
#include "d_ticcmd.h"
#include "doomdef.h"
#include "doomstat.h"
#include "doomtype.h"
#include "g_game.h"
#include "g_nextweapon.h"
#include "info.h"
#include "m_cheat.h"
#include "p_map.h"
#include "p_mobj.h"
#include "p_pspr.h"
#include "p_spec.h"
#include "p_user.h"
#include "r_defs.h"
#include "r_main.h"
#include "st_stuff.h"
#include "st_widgets.h"

static fixed_t PlayerSlope(player_t *player)
{
  const fixed_t pitch = player->pitch;
  return pitch ? -finetangent[(ANG90 - pitch) >> ANGLETOFINESHIFT] : 0;
}

//
// Movement.
//

// 16 pixels of bob

#define MAXBOB  0x100000

boolean onground; // whether player is on ground or in air

//
// P_Thrust
// Moves the given origin along a given angle.
//

void P_Thrust(player_t* player,angle_t angle,fixed_t move)
{
  player->mo->momx += FixedMul(move,finecosine[angle >>= ANGLETOFINESHIFT]);
  player->mo->momy += FixedMul(move,finesine[angle]);
}

//
// P_Bob
// Same as P_Thrust, but only affects bobbing.
//
// killough 10/98: We apply thrust separately between the real physical player
// and the part which affects bobbing. This way, bobbing only comes from player
// motion, nothing external, avoiding many problems, e.g. bobbing should not
// occur on conveyors, unless the player walks on one, and bobbing should be
// reduced at a regular rate, even on ice (where the player coasts).
//

void P_Bob(player_t *player, angle_t angle, fixed_t move)
{
  if (demo_version < DV_MBF)
    return;

  player->momx += FixedMul(move,finecosine[angle >>= ANGLETOFINESHIFT]);
  player->momy += FixedMul(move,finesine[angle]);
}

//
// P_CalcHeight
// Calculate the walking / running height adjustment
//

void P_CalcHeight (player_t* player)
{
  int     angle;
  fixed_t bob;

  // Regular movement bobbing
  // (needs to be calculated for gun swing
  // even if not on ground)
  // OPTIMIZE: tablify angle
  // Note: a LUT allows for effects
  //  like a ramp with low health.

  // killough 10/98: Make bobbing depend only on player-applied motion.
  //
  // Note: don't reduce bobbing here if on ice: if you reduce bobbing here,
  // it causes bobbing jerkiness when the player moves from ice to non-ice,
  // and vice-versa.

  // [FG] MBF player bobbing rewrite causes demo sync problems
  // http://prboom.sourceforge.net/mbf-bugs.html
  player->bob = (demo_version >= DV_MBF && player_bobbing) ?
      (FixedMul(player->momx,player->momx)
      + FixedMul(player->momy,player->momy))>>2 :
      (demo_compatibility || player_bobbing) ?
      (FixedMul (player->mo->momx, player->mo->momx)
      + FixedMul (player->mo->momy,player->mo->momy))>>2 : 0;

  if ((demo_version == DV_BOOM || demo_version == DV_MBF) &&
      player->mo->friction > ORIG_FRICTION) // ice?
  {
    if (player->bob > (MAXBOB>>2))
      player->bob = MAXBOB>>2;
  }
  else
  {
  if (player->bob > MAXBOB)                             
    player->bob = MAXBOB;
  }

  if (!onground || player->cheats & CF_NOMOMENTUM)
    {
      player->viewz = player->mo->z + VIEWHEIGHT;

      if (player->viewz > player->mo->ceilingz-4*FRACUNIT)
	player->viewz = player->mo->ceilingz-4*FRACUNIT;

      // phares 2/25/98:
      // The following line was in the Id source and appears
      // to be a bug. player->viewz is checked in a similar
      // manner at a different exit below.

      // player->viewz = player->mo->z + player->viewheight;

      return;
    }

  angle = (FINEANGLES/20*leveltime)&FINEMASK;

  bob = player->bob * view_bobbing_pct / 4;
  bob = FixedMul(bob/2,finesine[angle]);

  // move viewheight

  if (player->playerstate == PST_LIVE)
    {
      player->viewheight += player->deltaviewheight;

      if (player->viewheight > VIEWHEIGHT)
	{
	  player->viewheight = VIEWHEIGHT;
	  player->deltaviewheight = 0;
	}

      if (player->viewheight < VIEWHEIGHT/2)
	{
	  player->viewheight = VIEWHEIGHT/2;
	  if (player->deltaviewheight <= 0)
	    player->deltaviewheight = 1;
	}

      if (player->deltaviewheight)
	{
	  player->deltaviewheight += FRACUNIT/4;
	  if (!player->deltaviewheight)
	    player->deltaviewheight = 1;
	}
    }

  player->viewz = player->mo->z + player->viewheight + bob;

  if (player->viewz > player->mo->ceilingz-4*FRACUNIT)
    player->viewz = player->mo->ceilingz-4*FRACUNIT;
}

//
// P_MovePlayer
//
// Adds momentum if the player is not in the air
//
// killough 10/98: simplified

void P_MovePlayer (player_t* player)
{
  ticcmd_t *cmd = &player->cmd;
  mobj_t *mo = player->mo;

  mo->angle += cmd->angleturn << 16;
  onground = mo->z <= mo->floorz;
  player->ticangle += cmd->ticangleturn << FRACBITS;

  // killough 10/98:
  //
  // We must apply thrust to the player and bobbing separately, to avoid
  // anomalies. The thrust applied to bobbing is always the same strength on
  // ice, because the player still "works just as hard" to move, while the
  // thrust applied to the movement varies with 'movefactor'.

  if ((!demo_compatibility && demo_version < DV_MBF) ||
      cmd->forwardmove | cmd->sidemove) // killough 10/98
    {
      if (onground || mo->flags & MF_BOUNCES) // killough 8/9/98
	{
	  int friction, movefactor = P_GetMoveFactor(mo, &friction);

	  // killough 11/98:
	  // On sludge, make bobbing depend on efficiency.
	  // On ice, make it depend on effort.

	  int bobfactor =
	    friction < ORIG_FRICTION ? movefactor : ORIG_FRICTION_FACTOR;

	  if (cmd->forwardmove)
	    {
	      P_Bob(player,mo->angle,cmd->forwardmove*bobfactor);
	      P_Thrust(player,mo->angle,cmd->forwardmove*movefactor);
	    }

	  if (cmd->sidemove)
	    {
	      P_Bob(player,mo->angle-ANG90,cmd->sidemove*bobfactor);
	      P_Thrust(player,mo->angle-ANG90,cmd->sidemove*movefactor);
	    }
	}
      // Add (cmd-> forwardmove || cmd-> sidemove) check to prevent the players
      // always in S_PLAY_RUN1 animation in complevel Boom.
      if ((cmd->forwardmove || cmd->sidemove) &&
          (mo->state == states+S_PLAY))
	P_SetMobjState(mo,S_PLAY_RUN1);
    }

  if (!menuactive && !demoplayback && !player->centering)
  {
    player->pitch += cmd->pitch << FRACBITS;
    player->pitch = CLAMP(player->pitch, -max_pitch_angle, max_pitch_angle);
    player->slope = PlayerSlope(player);
  }
}

#define ANG5 (ANG90/18)

death_use_action_t death_use_action;
death_use_state_t death_use_state;

//
// P_DeathThink
// Fall on your face when dying.
// Decrease POV height to floor height.
//

void P_DeathThink (player_t* player)
{
  angle_t angle;
  angle_t delta;

  P_MovePsprites (player);

  // fall to the ground

  if (player->viewheight > 6*FRACUNIT)
    player->viewheight -= FRACUNIT;

  if (player->viewheight < 6*FRACUNIT)
    player->viewheight = 6*FRACUNIT;

  player->deltaviewheight = 0;
  onground = (player->mo->z <= player->mo->floorz);
  P_CalcHeight (player);

  if (player->attacker && player->attacker != player->mo)
    {
      angle = R_PointToAngle2 (player->mo->x,
			       player->mo->y,
			       player->attacker->x,
			       player->attacker->y);

      delta = angle - player->mo->angle;

      if (delta < ANG5 || delta > (unsigned)-ANG5)
	{
	  // Looking at killer,
	  //  so fade damage flash down.

	  player->mo->angle = angle;

	  if (player->damagecount)
	    player->damagecount--;
	}
      else 
	if (delta < ANG180)
	  player->mo->angle += ANG5;
	else
	  player->mo->angle -= ANG5;
    }
  else 
    if (player->damagecount)
      player->damagecount--;

  if (player->cmd.buttons & BT_USE)
  {
    player->playerstate = PST_REBORN;
  }

  if (death_use_state == DEATH_USE_STATE_PENDING)
  {
    death_use_state = DEATH_USE_STATE_ACTIVE;

    if (!G_AutoSaveEnabled() || !G_LoadAutoSaveDeathUse())
    {
      if (savegameslot >= 0)
      {
        char *file = G_SaveGameName(savegameslot);
        G_LoadGame(file, savegameslot, false);
        free(file);
      }
      else
      {
        player->playerstate = PST_REBORN;
      }
    }
  }
}

//
// P_PlayerThink
//

void P_PlayerThink (player_t* player)
{
  ticcmd_t*    cmd;
  weapontype_t newweapon;

  // [AM] Assume we can interpolate at the beginning
  //      of the tic.
  player->mo->interp = true;

  // [AM] Store starting position for player interpolation.
  player->mo->oldx = player->mo->x;
  player->mo->oldy = player->mo->y;
  player->mo->oldz = player->mo->z;
  player->mo->oldangle = player->mo->angle;
  player->oldviewz = player->viewz;
  player->oldpitch = player->pitch;
  player->oldrecoilpitch = player->recoilpitch;
  player->oldticangle = player->ticangle;

  // killough 2/8/98, 3/21/98:
  // (this code is necessary despite questions raised elsewhere in a comment)

  if (player->cheats & CF_NOCLIP)
    player->mo->flags |= MF_NOCLIP;
  else
    player->mo->flags &= ~MF_NOCLIP;

  // chain saw run forward

  cmd = &player->cmd;
  if (player->mo->flags & MF_JUSTATTACKED)
    {
      cmd->angleturn = 0;
      cmd->ticangleturn = 0;
      cmd->forwardmove = 0xc800/512;
      cmd->sidemove = 0;
      player->mo->flags &= ~MF_JUSTATTACKED;
    }

  // [crispy] center view
  if (player->centering)
  {
    player->pitch /= 2;

    if (abs(player->pitch) < ANG1)
    {
      player->pitch = 0;

      if (player->oldpitch == 0)
      {
        player->centering = false;
      }
    }

    player->slope = PlayerSlope(player);
  }

  // [crispy] weapon recoil pitch
  if (player->recoilpitch)
  {
    if (player->recoilpitch > 0)
    {
      player->recoilpitch -= ANG1;
    }
    else if (player->recoilpitch < 0)
    {
      player->recoilpitch += ANG1;
    }
  }

  if (player->playerstate == PST_DEAD)
    {
      P_DeathThink (player);
      return;
    }

  // Move around.
  // Reactiontime is used to prevent movement
  //  for a bit after a teleport.

  if (player->mo->reactiontime)
    player->mo->reactiontime--;
  else
    P_MovePlayer (player);

  P_CalcHeight (player); // Determines view height and bobbing

  // Determine if there's anything about the sector you're in that's
  // going to affect you, like painful floors.

  if (player->mo->subsector->sector->special)
    P_PlayerInSpecialSector (player);

  // Sprite Height problem...                                         // phares
  // Future code:                                                     //  |
  // It's possible that at this point the player is standing on top   //  V
  // of a Thing that could cause him some damage, like a torch or
  // burning barrel. We need a way to generalize Thing damage by
  // grabbing a bit in the Thing's options to indicate damage. Since
  // this is competing with other attributes we may want to add,
  // we'll put this off for future consideration when more is
  // known.

  // Future Code:                                                     //  ^
  // Check to see if the object you've been standing on has moved     //  |
  // out from underneath you.                                         // phares

  // Check for weapon change.

  // A special event has no other buttons.

  if (cmd->buttons & BT_SPECIAL)
    cmd->buttons = 0;

  if (cmd->buttons & BT_CHANGE)
    {
      // The actual changing of the weapon is done
      //  when the weapon psprite can do it
      //  (read: not in the middle of an attack).

      newweapon = (cmd->buttons & BT_WEAPONMASK)>>BT_WEAPONSHIFT;

      // killough 3/22/98: For demo compatibility we must perform the fist
      // and SSG weapons switches here, rather than in G_BuildTiccmd(). For
      // other games which rely on user preferences, we must use the latter.

      if (demo_compatibility)
	{ // compatibility mode -- required for old demos -- killough
	  newweapon = (cmd->buttons & BT_WEAPONMASK_OLD) >> BT_WEAPONSHIFT;
	  if (newweapon == wp_fist && player->weaponowned[wp_chainsaw] &&
	      (player->readyweapon != wp_chainsaw ||
	       !player->powers[pw_strength]))
	    newweapon = wp_chainsaw;
	  if (ALLOW_SSG &&
	      newweapon == wp_shotgun &&
	      player->weaponowned[wp_supershotgun] &&
	      player->readyweapon != wp_supershotgun)
	    newweapon = wp_supershotgun;
	}

      // killough 2/8/98, 3/22/98 -- end of weapon selection changes

      if (player->weaponowned[newweapon] && newweapon != player->readyweapon)

	// Do not go to plasma or BFG in shareware,
	//  even if cheated.

	if ((newweapon != wp_plasma && newweapon != wp_bfg)
	    || (gamemode != shareware) )
	  player->pendingweapon = newweapon;
    }

  // check for use

  if (cmd->buttons & BT_USE)
    {
      if (!player->usedown)
	{
	  P_UseLines (player);
	  player->usedown = true;

	  // [crispy] "use" button timer
	  if (hud_time_use)
	  {
	    // to match the timer, we use the leveltime value at the end of the frame
	    player->btuse = leveltime + 1;
	    player->btuse_tics = 5*TICRATE/2 + 1; // [crispy] 2.5 seconds
	  }
	}
    }
  else
    player->usedown = false;

  // cycle psprites

  P_MovePsprites (player);

  // Counters, time dependent power ups.

  if (player->btuse_tics > 0)
    player->btuse_tics--;

  // Strength counts up to diminish fade.

  if (player->powers[pw_strength])
    player->powers[pw_strength]++;

  // killough 1/98: Make idbeholdx toggle:

  if (player->powers[pw_invulnerability] > 0) // killough
    player->powers[pw_invulnerability]--;

  if (player->powers[pw_invisibility] > 0)    // killough
    if (! --player->powers[pw_invisibility] )
      player->mo->flags &= ~MF_SHADOW;

  if (player->powers[pw_infrared] > 0)        // killough
    player->powers[pw_infrared]--;

  if (player->powers[pw_ironfeet] > 0)        // killough
    player->powers[pw_ironfeet]--;

  if (player->cheats & CF_MAPCOORDS)
  {
    cheat_mypos_print();
  }

  if (player->damagecount)
    player->damagecount--;

  if (player->bonuscount)
    player->bonuscount--;

  // Handling colormaps.
  // killough 3/20/98: reformat to terse C syntax

  // killough 7/11/98: beta version had invisibility, instead of
  // invulernability, and the light amp visor used the last colormap.
  // But white flashes occurred when invulnerability wore off.

  if (STRICTMODE(palette_changes == PAL_CHANGE_OFF))
  {
    if (player->powers[pw_invulnerability] || player->powers[pw_infrared])
      player->fixedcolormap = 1;
    else
      player->fixedcolormap = 0;
  }
  else
  player->fixedcolormap = 

    beta_emulation ?    /* Beta Emulation */
    player->powers[pw_infrared] > 4*32 ||
    player->powers[pw_infrared] & 8 ? 32 :
    player->powers[pw_invisibility] > 4*32 ||
    player->powers[pw_invisibility] & 8 ||
    (player->powers[pw_invulnerability] < 4*32 &&
     player->powers[pw_invulnerability] > 0 &&
     player->powers[pw_invulnerability] & 8) ? 33 : 0 :

    player->powers[pw_invulnerability] > 4*32 ||    /* Regular Doom */
    player->powers[pw_invulnerability] & 8 ? INVERSECOLORMAP :
    player->powers[pw_infrared] > 4*32 || player->powers[pw_infrared] & 8;
}

boolean P_EvaluateItemOwned(itemtype_t item, player_t *player)
{
    switch (item)
    {
        case item_bluecard:
        case item_yellowcard:
        case item_redcard:
        case item_blueskull:
        case item_yellowskull:
        case item_redskull:
            return player->cards[item - item_bluecard] != 0;

        case item_backpack:
            return player->backpack;

        case item_greenarmor:
        case item_bluearmor:
            return player->armortype == (item - item_greenarmor + 1);

        case item_areamap:
            return player->powers[pw_allmap] != 0;

        case item_lightamp:
            return player->powers[pw_infrared] != 0;

        case item_berserk:
            return player->powers[pw_strength] != 0;

        case item_invisibility:
            return player->powers[pw_invisibility] != 0;

        case item_radsuit:
            return player->powers[pw_ironfeet] != 0;

        case item_invulnerability:
            return player->powers[pw_invulnerability]
                   || (player->cheats & CF_GODMODE);

        case item_healthbonus:
        case item_stimpack:
        case item_medikit:
        case item_soulsphere:
        case item_megasphere:
        case item_armorbonus:
        case item_noitem:
        case item_messageonly:
        default:
            break;
    }

    return false;
}

//----------------------------------------------------------------------------
//
// $Log: p_user.c,v $
// Revision 1.14  1998/05/12  12:47:25  phares
// Removed OVER_UNDER code
//
// Revision 1.13  1998/05/10  23:38:04  killough
// Add #include p_user.h to ensure consistent prototypes
//
// Revision 1.12  1998/05/05  15:35:20  phares
// Documentation and Reformatting changes
//
// Revision 1.11  1998/05/03  23:21:04  killough
// Fix #includes and remove unnecessary decls at the top, nothing else
//
// Revision 1.10  1998/03/23  15:24:50  phares
// Changed pushers to linedef control
//
// Revision 1.9  1998/03/23  03:35:24  killough
// Move weapons changes to G_BuildTiccmd, fix idclip
//
// Revision 1.8  1998/03/12  14:28:50  phares
// friction and IDCLIP changes
//
// Revision 1.7  1998/03/09  18:26:55  phares
// Fixed bug in neighboring variable friction sectors
//
// Revision 1.6  1998/02/27  08:10:08  phares
// Added optional player bobbing
//
// Revision 1.5  1998/02/24  08:46:42  phares
// Pushers, recoil, new friction, and over/under work
//
// Revision 1.4  1998/02/15  02:47:57  phares
// User-defined keys
//
// Revision 1.3  1998/02/09  03:13:20  killough
// Improve weapon control and add preferences
//
// Revision 1.2  1998/01/26  19:24:34  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:03:01  rand
// Lee's Jan 19 sources
//
//----------------------------------------------------------------------------
