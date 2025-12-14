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
//  Switches, buttons. Two-state animation. Exits.
//
//-----------------------------------------------------------------------------

#include "d_player.h"
#include "doomdata.h"
#include "doomdef.h"
#include "doomstat.h"
#include "doomtype.h"
#include "g_game.h"
#include "i_printf.h"
#include "m_swap.h"
#include "p_dirty.h"
#include "p_mobj.h"
#include "p_spec.h"
#include "r_data.h"
#include "r_defs.h"
#include "r_state.h"
#include "s_sound.h"
#include "sounds.h"
#include "w_wad.h"
#include "z_zone.h"

// killough 2/8/98: Remove switch limit

static int *switchlist;                           // killough
static int max_numswitches;                       // killough
static int numswitches;                           // killough

button_t  buttonlist[MAXBUTTONS];

//
// P_InitSwitchList()
//
// Only called at game initialization in order to list the set of switches
// and buttons known to the engine. This enables their texture to change
// when activated, and in the case of buttons, change back after a timeout.
//
// This routine modified to read its data from a predefined lump or
// PWAD lump called SWITCHES rather than a static table in this module to
// allow wad designers to insert or modify switches.
//
// Lump format is an array of byte packed switchlist_t structures, terminated
// by a structure with episode == -0. The lump can be generated from a
// text source file using SWANTBLS.EXE, distributed with the BOOM utils.
// The standard list of switches and animations is contained in the example
// source text file DEFSWANI.DAT also in the BOOM util distribution.
//
// Rewritten by Lee Killough to remove limit 2/8/98
//
void P_InitSwitchList(void)
{
  int i, index = 0;
  int episode = (gamemode == registered || gamemode==retail) ?
                 2 : gamemode == commercial ? 3 : 1;
  switchlist_t *alphSwitchList;         //jff 3/23/98 pointer to switch table

  //jff 3/23/98 read the switch table from a predefined lump             
  alphSwitchList = (switchlist_t *)W_CacheLumpName("SWITCHES",PU_STATIC);

  for (i=0;;i++)
  {
    if (index+1 >= max_numswitches)
      switchlist = Z_Realloc(switchlist, sizeof *switchlist *
          (max_numswitches = max_numswitches ? max_numswitches*2 : 8),
          PU_STATIC, 0);
    if (SHORT(alphSwitchList[i].episode) <= episode) //jff 5/11/98 endianess
    {
      int texture1, texture2;

      if (!SHORT(alphSwitchList[i].episode))
        break;

      // Ignore switches referencing unknown texture names, instead of exiting.
      // Warn if either one is missing, but only add if both are valid.
      texture1 = R_CheckTextureNumForName(alphSwitchList[i].name1);

      if (texture1 == -1)
        I_Printf(VB_WARNING, "P_InitSwitchList: unknown texture %s",
            alphSwitchList[i].name1);

      texture2 = R_CheckTextureNumForName(alphSwitchList[i].name2);

      if (texture2 == -1)
        I_Printf(VB_WARNING, "P_InitSwitchList: unknown texture %s",
            alphSwitchList[i].name2);

      if (texture1 != -1 && texture2 != -1)
      {
        switchlist[index++] = texture1;
        switchlist[index++] = texture2;
      }
    }
  }

  numswitches = index/2;
  switchlist[index] = -1;
  Z_ChangeTag(alphSwitchList,PU_CACHE); //jff 3/23/98 allow table to be freed
}

//
// P_StartButton()
//
// Start a button (retriggerable switch) counting down till it turns off.
//
// Passed the linedef the button is on, which texture on the sidedef contains
// the button, the texture number of the button, and the time the button is
// to remain active in gametics.
// No return value.
//
void P_StartButton
( line_t*       line,
  bwhere_e      w,
  int           texture,
  int           time )
{
  int           i;
    
  // See if button is already pressed
  for (i = 0;i < MAXBUTTONS;i++)
    if (buttonlist[i].btimer && buttonlist[i].line == line)
      return;
    
  for (i = 0;i < MAXBUTTONS;i++)
    if (!buttonlist[i].btimer)    // use first unused element of list
    {
      buttonlist[i].line = line;
      buttonlist[i].where = w;
      buttonlist[i].btexture = texture;
      buttonlist[i].btimer = time;
      buttonlist[i].soundorg = (mobj_t *)&line->frontsector->soundorg;
      return;
    }
    
  I_Printf(VB_WARNING, "no button slots left!");
}

//
// P_ChangeSwitchTexture()
//
// Function that changes switch wall texture on activation.
//
// Passed the line which the switch is on, and whether its retriggerable.
// If not retriggerable, this function clears the line special to insure that
//
// No return value
//
void P_ChangeSwitchTexture
( line_t*       line,
  int           useAgain )
{
  int     texTop;
  int     texMid;
  int     texBot;
  int     i;
  int     sound;

  if (!useAgain)
    dirty_line(line)->special = 0;

  texTop = sides[line->sidenum[0]].toptexture;
  texMid = sides[line->sidenum[0]].midtexture;
  texBot = sides[line->sidenum[0]].bottomtexture;

  sound = sfx_swtchn;

  // EXIT SWITCH?
  if (line->special == 11)                
    sound = sfx_swtchx;

  for (i = 0;i < numswitches*2;i++)
  {
    if (switchlist[i] == texTop)     // if an upper texture
    {
      S_StartSound(buttonlist->soundorg,sound);     // switch activation sound
      dirty_side(&sides[line->sidenum[0]])->toptexture = switchlist[i^1];       //chg texture

      if (useAgain)
        P_StartButton(line,top,switchlist[i],BUTTONTIME);         //start timer

      return;
    }
    else
    {
      if (switchlist[i] == texMid)   // if a normal texture
      {
        S_StartSound(buttonlist->soundorg,sound);   // switch activation sound
        dirty_side(&sides[line->sidenum[0]])->midtexture = switchlist[i^1];     //chg texture

        if (useAgain)
          P_StartButton(line, middle,switchlist[i],BUTTONTIME);   //start timer

        return;
      }
      else
      {
        if (switchlist[i] == texBot) // if a lower texture
        {
          S_StartSound(buttonlist->soundorg,sound); // switch activation sound
          dirty_side(&sides[line->sidenum[0]])->bottomtexture = switchlist[i^1];//chg texture

          if (useAgain)
            P_StartButton(line, bottom,switchlist[i],BUTTONTIME); //start timer

          return;
        }
      }
    }
  }
}


//
// P_UseSpecialLine
//
//
// Called when a thing uses (pushes) a special line.
// Only the front sides of lines are usable.
// Dispatches to the appropriate linedef function handler.
//
// Passed the thing using the line, the line being used, and the side used
// Returns true if a thinker was created
//
boolean
P_UseSpecialLine
( mobj_t*       thing,
  line_t*       line,
  int           side,
  boolean       bossaction )
{

  if (side) //jff 6/1/98 fix inadvertent deletion of side test
    return false;

  //jff 02/04/98 add check here for generalized floor/ceil mover
  if (!demo_compatibility)
  {
    // pointer to line function is NULL by default, set non-null if
    // line special is push or switch generalized linedef type
    int (*linefunc)(line_t *line)=NULL;

    // check each range of generalized linedefs
    if ((unsigned)line->special >= GenFloorBase)
    {
      if (!thing->player && !bossaction)
        if ((line->special & FloorChange) || !(line->special & FloorModel))
          return false; // FloorModel is "Allow Monsters" if FloorChange is 0
      if (!line->args[0] && ((line->special&6)!=6)) //jff 2/27/98 all non-manual
        return false;                         // generalized types require tag
      linefunc = EV_DoGenFloor;
    }
    else if ((unsigned)line->special >= GenCeilingBase)
    {
      if (!thing->player && !bossaction)
        if ((line->special & CeilingChange) || !(line->special & CeilingModel))
          return false;   // CeilingModel is "Allow Monsters" if CeilingChange is 0
      if (!line->args[0] && ((line->special&6)!=6)) //jff 2/27/98 all non-manual
        return false;                         // generalized types require tag
      linefunc = EV_DoGenCeiling;
    }
    else if ((unsigned)line->special >= GenDoorBase)
    {
      if (!thing->player && !bossaction)
      {
        if (!(line->special & DoorMonster))
          return false;   // monsters disallowed from this door
        if (line->flags & ML_SECRET) // they can't open secret doors either
          return false;
      }
      if (!line->args[0] && ((line->special&6)!=6)) //jff 3/2/98 all non-manual
        return false;                         // generalized types require tag
      linefunc = EV_DoGenDoor;
    }
    else if ((unsigned)line->special >= GenLockedBase)
    {
      if (!thing->player || bossaction)
        return false;   // monsters disallowed from unlocking doors
      if (!P_CanUnlockGenDoor(line,thing->player))
        return false;
      if (!line->args[0] && ((line->special&6)!=6)) //jff 2/27/98 all non-manual
        return false;                         // generalized types require tag

      linefunc = EV_DoGenLockedDoor;
    }
    else if ((unsigned)line->special >= GenLiftBase)
    {
      if (!thing->player && !bossaction)
        if (!(line->special & LiftMonster))
          return false; // monsters disallowed
      if (!line->args[0] && ((line->special&6)!=6)) //jff 2/27/98 all non-manual
        return false;                         // generalized types require tag
      linefunc = EV_DoGenLift;
    }
    else if ((unsigned)line->special >= GenStairsBase)
    {
      if (!thing->player && !bossaction)
        if (!(line->special & StairMonster))
          return false; // monsters disallowed
      if (!line->args[0] && ((line->special&6)!=6)) //jff 2/27/98 all non-manual
        return false;                         // generalized types require tag
      linefunc = EV_DoGenStairs;
    }
    else if ((unsigned)line->special >= GenCrusherBase)
    {
      if (!thing->player && !bossaction)
        if (!(line->special & CrusherMonster))
          return false; // monsters disallowed
      if (!line->args[0] && ((line->special&6)!=6)) //jff 2/27/98 all non-manual
        return false;                         // generalized types require tag
      linefunc = EV_DoGenCrusher;
    }

    if (linefunc)
      switch((line->special & TriggerType) >> TriggerTypeShift)
      {
        case PushOnce:
          if (!side)
            if (linefunc(line))
              dirty_line(line)->special = 0;
          return true;
        case PushMany:
          if (!side)
            linefunc(line);
          return true;
        case SwitchOnce:
          if (linefunc(line))
            P_ChangeSwitchTexture(line,0);
          return true;
        case SwitchMany:
          if (linefunc(line))
            P_ChangeSwitchTexture(line,1);
          return true;
        default:  // if not a switch/push type, do nothing here
          return false;
      }
  }

  // Switches that other things can activate.
  if (!thing->player && !bossaction)
  {
    // never open secret doors
    if (line->flags & ML_SECRET)
      return false;

    switch(line->special)
    {
      case 1:         // MANUAL DOOR RAISE
      case 32:        // MANUAL BLUE
      case 33:        // MANUAL RED
      case 34:        // MANUAL YELLOW
      //jff 3/5/98 add ability to use teleporters for monsters
      case 195:       // switch teleporters
      case 174:
      case 210:       // silent switch teleporters
      case 209:
        break;

      default:
        return false;
        break;
    }
  }

  if (bossaction)
  {
    switch(line->special)
    {
      // 0-tag specials, locked switches and teleporters need to be blocked for boss actions.
      case 1:         // MANUAL DOOR RAISE
      case 32:        // MANUAL BLUE
      case 33:        // MANUAL RED
      case 34:        // MANUAL YELLOW
      case 117:       // Blazing door raise
      case 118:       // Blazing door open
      case 133:       // BlzOpenDoor BLUE
      case 135:       // BlzOpenDoor RED
      case 137:       // BlzOpenDoor YEL

      case 99:        // BlzOpenDoor BLUE
      case 134:       // BlzOpenDoor RED
      case 136:       // BlzOpenDoor YELLOW

      //jff 3/5/98 add ability to use teleporters for monsters
      case 195:       // switch teleporters
      case 174:
      case 210:       // silent switch teleporters
      case 209:
        return false;
        break;
    }
  }

  if (!P_CheckTag(line))  //jff 2/27/98 disallow zero tag on some types
    return false;

  // Dispatch to handler according to linedef type
  switch (line->special)
  {
    // Manual doors, push type with no tag
    case 1:             // Vertical Door
    case 26:            // Blue Door/Locked
    case 27:            // Yellow Door /Locked
    case 28:            // Red Door /Locked

    case 31:            // Manual door open
    case 32:            // Blue locked door open
    case 33:            // Red locked door open
    case 34:            // Yellow locked door open

    case 117:           // Blazing door raise
    case 118:           // Blazing door open
      EV_VerticalDoor (line, thing);
      return true;

    // Switches (non-retriggerable)
    case 7:
      // Build Stairs
      if (EV_BuildStairs(line,build8))
        P_ChangeSwitchTexture(line,0);
      return true;

    case 9:
      // Change Donut
      if (EV_DoDonut(line))
        P_ChangeSwitchTexture(line,0);
      return true;

    // S1 - Exit to the next map and reset inventory.
    case 2070:
      if (demo_version < DV_ID24)
        return false;
      reset_inventory = true;
      // fallthrough

    case 11:
      // Exit level

      // killough 10/98: prevent zombies from exiting levels
      if (!bossaction && thing->player && thing->player->health <= 0 && !comp[comp_zombie])
      {
        S_StartSound(thing, sfx_noway);
        return false;
      }

      P_ChangeSwitchTexture(line,0);
      G_ExitLevel();
      return true;

    case 14:
      // Raise Floor 32 and change texture
      if (EV_DoPlat(line,raiseAndChange,32))
        P_ChangeSwitchTexture(line,0);
      return true;

    case 15:
      // Raise Floor 24 and change texture
      if (EV_DoPlat(line,raiseAndChange,24))
        P_ChangeSwitchTexture(line,0);
      return true;

    case 18:
      // Raise Floor to next highest floor
      if (EV_DoFloor(line, raiseFloorToNearest))
        P_ChangeSwitchTexture(line,0);
      return true;

    case 20:
      // Raise Plat next highest floor and change texture
      if (EV_DoPlat(line,raiseToNearestAndChange,0))
        P_ChangeSwitchTexture(line,0);
      return true;

    case 21:
      // PlatDownWaitUpStay
      if (EV_DoPlat(line,downWaitUpStay,0))
        P_ChangeSwitchTexture(line,0);
      return true;

    case 23:
      // Lower Floor to Lowest
      if (EV_DoFloor(line,lowerFloorToLowest))
        P_ChangeSwitchTexture(line,0);
      return true;

    case 29:
      // Raise Door
      if (EV_DoDoor(line, doorNormal))
        P_ChangeSwitchTexture(line,0);
      return true;

    case 41:
      // Lower Ceiling to Floor
      if (EV_DoCeiling(line,lowerToFloor))
        P_ChangeSwitchTexture(line,0);
      return true;

    case 71:
      // Turbo Lower Floor
      if (EV_DoFloor(line,turboLower))
        P_ChangeSwitchTexture(line,0);
      return true;

    case 49:
      // Ceiling Crush And Raise
      if (EV_DoCeiling(line,crushAndRaise))
        P_ChangeSwitchTexture(line,0);
      return true;

    case 50:
      // Close Door
      if (EV_DoDoor(line,doorClose))
        P_ChangeSwitchTexture(line,0);
      return true;

    // SR - Exit to the secret map and reset inventory.
    case 2073:
      if (demo_version < DV_ID24)
        return false;
      reset_inventory = true;
      // fallthrough

    case 51:
      // Secret EXIT

      // killough 10/98: prevent zombies from exiting levels
      if (!bossaction && thing->player && thing->player->health <= 0 && !comp[comp_zombie])
      {
        S_StartSound(thing, sfx_noway);
        return false;
      }

      P_ChangeSwitchTexture(line,0);
      G_SecretExitLevel();
      return true;

    case 55:
      // Raise Floor Crush
      if (EV_DoFloor(line,raiseFloorCrush))
        P_ChangeSwitchTexture(line,0);
      return true;

    case 101:
      // Raise Floor
      if (EV_DoFloor(line,raiseFloor))
        P_ChangeSwitchTexture(line,0);
      return true;

    case 102:
      // Lower Floor to Surrounding floor height
      if (EV_DoFloor(line,lowerFloor))
        P_ChangeSwitchTexture(line,0);
      return true;

    case 103:
      // Open Door
      if (EV_DoDoor(line,doorOpen))
        P_ChangeSwitchTexture(line,0);
      return true;

    case 111:
      // Blazing Door Raise (faster than TURBO!)
      if (EV_DoDoor (line,blazeRaise))
        P_ChangeSwitchTexture(line,0);
      return true;

    case 112:
      // Blazing Door Open (faster than TURBO!)
      if (EV_DoDoor (line,blazeOpen))
        P_ChangeSwitchTexture(line,0);
      return true;

    case 113:
      // Blazing Door Close (faster than TURBO!)
      if (EV_DoDoor (line,blazeClose))
        P_ChangeSwitchTexture(line,0);
      return true;

    case 122:
      // Blazing PlatDownWaitUpStay
      if (EV_DoPlat(line,blazeDWUS,0))
        P_ChangeSwitchTexture(line,0);
      return true;

    case 127:
      // Build Stairs Turbo 16
      if (EV_BuildStairs(line,turbo16))
        P_ChangeSwitchTexture(line,0);
      return true;

    case 131:
      // Raise Floor Turbo
      if (EV_DoFloor(line,raiseFloorTurbo))
        P_ChangeSwitchTexture(line,0);
      return true;

    case 133:
      // BlzOpenDoor BLUE
    case 135:
      // BlzOpenDoor RED
    case 137:
      // BlzOpenDoor YELLOW
      if (EV_DoLockedDoor (line,blazeOpen,thing))
        P_ChangeSwitchTexture(line,0);
      return true;

    case 140:
      // Raise Floor 512
      if (EV_DoFloor(line,raiseFloor512))
        P_ChangeSwitchTexture(line,0);
      return true;

    // ID24 Music Changers
    case 2059: case 2065: case 2089: case 2095:
    case 2060: case 2066: case 2090: case 2096:
      EV_ChangeMusic(line, side);
      return true;

    case 2078:
      dirty_line(line)->special = 0;
      // fallthrough

    case 2079:
    {
      int colormap_index = side ? line->backtint : line->fronttint;
      for (int s = -1; (s = P_FindSectorFromLineTag(line, s)) >= 0;)
      {
        sectors[s].tint = colormap_index;
      }
      break;
    }

      // killough 1/31/98: factored out compatibility check;
      // added inner switch, relaxed check to demo_compatibility

    default:
      if (!demo_compatibility)
        switch (line->special)
        {
          //jff 1/29/98 added linedef types to fill all functions out so that
          // all possess SR, S1, WR, W1 types

          case 158:
            // Raise Floor to shortest lower texture
            // 158 S1  EV_DoFloor(raiseToTexture), CSW(0)
            if (EV_DoFloor(line,raiseToTexture))
              P_ChangeSwitchTexture(line,0);
            return true;

          case 159:
            // Raise Floor to shortest lower texture
            // 159 S1  EV_DoFloor(lowerAndChange)
            if (EV_DoFloor(line,lowerAndChange))
              P_ChangeSwitchTexture(line,0);
            return true;

          case 160:
            // Raise Floor 24 and change
            // 160 S1  EV_DoFloor(raiseFloor24AndChange)
            if (EV_DoFloor(line,raiseFloor24AndChange))
              P_ChangeSwitchTexture(line,0);
            return true;

          case 161:
            // Raise Floor 24
            // 161 S1  EV_DoFloor(raiseFloor24)
            if (EV_DoFloor(line,raiseFloor24))
              P_ChangeSwitchTexture(line,0);
            return true;

          case 162:
            // Moving floor min n to max n
            // 162 S1  EV_DoPlat(perpetualRaise,0)
            if (EV_DoPlat(line,perpetualRaise,0))
              P_ChangeSwitchTexture(line,0);
            return true;

          case 163:
            // Stop Moving floor
            // 163 S1  EV_DoPlat(perpetualRaise,0)
            EV_StopPlat(line);
            P_ChangeSwitchTexture(line,0);
            return true;

          case 164:
            // Start fast crusher
            // 164 S1  EV_DoCeiling(fastCrushAndRaise)
            if (EV_DoCeiling(line,fastCrushAndRaise))
              P_ChangeSwitchTexture(line,0);
            return true;

          case 165:
            // Start slow silent crusher
            // 165 S1  EV_DoCeiling(silentCrushAndRaise)
            if (EV_DoCeiling(line,silentCrushAndRaise))
              P_ChangeSwitchTexture(line,0);
            return true;

          case 166:
            // Raise ceiling, Lower floor
            // 166 S1 EV_DoCeiling(raiseToHighest), EV_DoFloor(lowerFloortoLowest)
            if (EV_DoCeiling(line, raiseToHighest) ||
                EV_DoFloor(line, lowerFloorToLowest))
              P_ChangeSwitchTexture(line,0);
            return true;

          case 167:
            // Lower floor and Crush
            // 167 S1 EV_DoCeiling(lowerAndCrush)
            if (EV_DoCeiling(line, lowerAndCrush))
              P_ChangeSwitchTexture(line,0);
            return true;

          case 168:
            // Stop crusher
            // 168 S1 EV_CeilingCrushStop()
            if (EV_CeilingCrushStop(line))
              P_ChangeSwitchTexture(line,0);
            return true;

          case 169:
            // Lights to brightest neighbor sector
            // 169 S1  EV_LightTurnOn(0)
            EV_LightTurnOn(line,0);
            P_ChangeSwitchTexture(line,0);
            return true;

          case 170:
            // Lights to near dark
            // 170 S1  EV_LightTurnOn(35)
            EV_LightTurnOn(line,35);
            P_ChangeSwitchTexture(line,0);
            return true;

          case 171:
            // Lights on full
            // 171 S1  EV_LightTurnOn(255)
            EV_LightTurnOn(line,255);
            P_ChangeSwitchTexture(line,0);
            return true;

          case 172:
            // Start Lights Strobing
            // 172 S1  EV_StartLightStrobing()
            EV_StartLightStrobing(line);
            P_ChangeSwitchTexture(line,0);
            return true;

          case 173:
            // Lights to Dimmest Near
            // 173 S1  EV_TurnTagLightsOff()
            EV_TurnTagLightsOff(line);
            P_ChangeSwitchTexture(line,0);
            return true;

          case 174:
            // Teleport
            // 174 S1  EV_Teleport(side,thing)
            if (EV_Teleport(line,side,thing))
              P_ChangeSwitchTexture(line,0);
            return true;

          case 175:
            // Close Door, Open in 30 secs
            // 175 S1  EV_DoDoor(close30ThenOpen)
            if (EV_DoDoor(line,close30ThenOpen))
              P_ChangeSwitchTexture(line,0);
            return true;

          case 189: //jff 3/15/98 create texture change no motion type
            // Texture Change Only (Trigger)
            // 189 S1 Change Texture/Type Only
            if (EV_DoChange(line,trigChangeOnly))
              P_ChangeSwitchTexture(line,0);
            return true;

          case 203:
            // Lower ceiling to lowest surrounding ceiling
            // 203 S1 EV_DoCeiling(lowerToLowest)
            if (EV_DoCeiling(line,lowerToLowest))
              P_ChangeSwitchTexture(line,0);
            return true;

          case 204:
            // Lower ceiling to highest surrounding floor
            // 204 S1 EV_DoCeiling(lowerToMaxFloor)
            if (EV_DoCeiling(line,lowerToMaxFloor))
              P_ChangeSwitchTexture(line,0);
            return true;

          case 209:
            // killough 1/31/98: silent teleporter
            //jff 209 S1 SilentTeleport
            if (EV_SilentTeleport(line, side, thing))
              P_ChangeSwitchTexture(line,0);
            return true;

          case 241: //jff 3/15/98 create texture change no motion type
            // Texture Change Only (Numeric)
            // 241 S1 Change Texture/Type Only
            if (EV_DoChange(line,numChangeOnly))
              P_ChangeSwitchTexture(line,0);
            return true;

          case 221:
            // Lower floor to next lowest floor
            // 221 S1 Lower Floor To Nearest Floor
            if (EV_DoFloor(line,lowerFloorToNearest))
              P_ChangeSwitchTexture(line,0);
            return true;

          case 229:
            // Raise elevator next floor
            // 229 S1 Raise Elevator next floor
            if (EV_DoElevator(line,elevateUp))
              P_ChangeSwitchTexture(line,0);
            return true;

          case 233:
            // Lower elevator next floor
            // 233 S1 Lower Elevator next floor
            if (EV_DoElevator(line,elevateDown))
              P_ChangeSwitchTexture(line,0);
            return true;

          case 237:
            // Elevator to current floor
            // 237 S1 Elevator to current floor
            if (EV_DoElevator(line,elevateCurrent))
              P_ChangeSwitchTexture(line,0);
            return true;


          // jff 1/29/98 end of added S1 linedef types

          //jff 1/29/98 added linedef types to fill all functions out so that
          // all possess SR, S1, WR, W1 types

          case 78: //jff 3/15/98 create texture change no motion type
            // Texture Change Only (Numeric)
            // 78 SR Change Texture/Type Only
            if (EV_DoChange(line,numChangeOnly))
              P_ChangeSwitchTexture(line,1);
            return true;

          case 176:
            // Raise Floor to shortest lower texture
            // 176 SR  EV_DoFloor(raiseToTexture), CSW(1)
            if (EV_DoFloor(line,raiseToTexture))
              P_ChangeSwitchTexture(line,1);
            return true;

          case 177:
            // Raise Floor to shortest lower texture
            // 177 SR  EV_DoFloor(lowerAndChange)
            if (EV_DoFloor(line,lowerAndChange))
              P_ChangeSwitchTexture(line,1);
            return true;

          case 178:
            // Raise Floor 512
            // 178 SR  EV_DoFloor(raiseFloor512)
            if (EV_DoFloor(line,raiseFloor512))
              P_ChangeSwitchTexture(line,1);
            return true;

          case 179:
            // Raise Floor 24 and change
            // 179 SR  EV_DoFloor(raiseFloor24AndChange)
            if (EV_DoFloor(line,raiseFloor24AndChange))
              P_ChangeSwitchTexture(line,1);
            return true;

          case 180:
            // Raise Floor 24
            // 180 SR  EV_DoFloor(raiseFloor24)
            if (EV_DoFloor(line,raiseFloor24))
              P_ChangeSwitchTexture(line,1);
            return true;

          case 181:
            // Moving floor min n to max n
            // 181 SR  EV_DoPlat(perpetualRaise,0)

            EV_DoPlat(line,perpetualRaise,0);
            P_ChangeSwitchTexture(line,1);
            return true;

          case 182:
            // Stop Moving floor
            // 182 SR  EV_DoPlat(perpetualRaise,0)
            EV_StopPlat(line);
            P_ChangeSwitchTexture(line,1);
            return true;

          case 183:
            // Start fast crusher
            // 183 SR  EV_DoCeiling(fastCrushAndRaise)
            if (EV_DoCeiling(line,fastCrushAndRaise))
              P_ChangeSwitchTexture(line,1);
            return true;

          case 184:
            // Start slow crusher
            // 184 SR  EV_DoCeiling(crushAndRaise)
            if (EV_DoCeiling(line,crushAndRaise))
              P_ChangeSwitchTexture(line,1);
            return true;

          case 185:
            // Start slow silent crusher
            // 185 SR  EV_DoCeiling(silentCrushAndRaise)
            if (EV_DoCeiling(line,silentCrushAndRaise))
              P_ChangeSwitchTexture(line,1);
            return true;

          case 186:
            // Raise ceiling, Lower floor
            // 186 SR EV_DoCeiling(raiseToHighest), EV_DoFloor(lowerFloortoLowest)
            if (EV_DoCeiling(line, raiseToHighest) ||
                EV_DoFloor(line, lowerFloorToLowest))
              P_ChangeSwitchTexture(line,1);
            return true;

          case 187:
            // Lower floor and Crush
            // 187 SR EV_DoCeiling(lowerAndCrush)
            if (EV_DoCeiling(line, lowerAndCrush))
              P_ChangeSwitchTexture(line,1);
            return true;

          case 188:
            // Stop crusher
            // 188 SR EV_CeilingCrushStop()
            if (EV_CeilingCrushStop(line))
              P_ChangeSwitchTexture(line,1);
            return true;

          case 190: //jff 3/15/98 create texture change no motion type
            // Texture Change Only (Trigger)
            // 190 SR Change Texture/Type Only
            if (EV_DoChange(line,trigChangeOnly))
              P_ChangeSwitchTexture(line,1);
            return true;

          case 191:
            // Lower Pillar, Raise Donut
            // 191 SR  EV_DoDonut()
            if (EV_DoDonut(line))
              P_ChangeSwitchTexture(line,1);
            return true;

          case 192:
            // Lights to brightest neighbor sector
            // 192 SR  EV_LightTurnOn(0)
            EV_LightTurnOn(line,0);
            P_ChangeSwitchTexture(line,1);
            return true;

          case 193:
            // Start Lights Strobing
            // 193 SR  EV_StartLightStrobing()
            EV_StartLightStrobing(line);
            P_ChangeSwitchTexture(line,1);
            return true;

          case 194:
            // Lights to Dimmest Near
            // 194 SR  EV_TurnTagLightsOff()
            EV_TurnTagLightsOff(line);
            P_ChangeSwitchTexture(line,1);
            return true;

          case 195:
            // Teleport
            // 195 SR  EV_Teleport(side,thing)
            if (EV_Teleport(line,side,thing))
              P_ChangeSwitchTexture(line,1);
            return true;

          case 196:
            // Close Door, Open in 30 secs
            // 196 SR  EV_DoDoor(close30ThenOpen)
            if (EV_DoDoor(line,close30ThenOpen))
              P_ChangeSwitchTexture(line,1);
            return true;

          case 205:
            // Lower ceiling to lowest surrounding ceiling
            // 205 SR EV_DoCeiling(lowerToLowest)
            if (EV_DoCeiling(line,lowerToLowest))
              P_ChangeSwitchTexture(line,1);
            return true;

          case 206:
            // Lower ceiling to highest surrounding floor
            // 206 SR EV_DoCeiling(lowerToMaxFloor)
            if (EV_DoCeiling(line,lowerToMaxFloor))
              P_ChangeSwitchTexture(line,1);
            return true;

          case 210:
            // killough 1/31/98: silent teleporter
            //jff 210 SR SilentTeleport
            if (EV_SilentTeleport(line, side, thing))
              P_ChangeSwitchTexture(line,1);
            return true;

          case 211: //jff 3/14/98 create instant toggle floor type
            // Toggle Floor Between C and F Instantly
            // 211 SR Toggle Floor Instant
            if (EV_DoPlat(line,toggleUpDn,0))
              P_ChangeSwitchTexture(line,1);
            return true;

          case 222:
            // Lower floor to next lowest floor
            // 222 SR Lower Floor To Nearest Floor
            if (EV_DoFloor(line,lowerFloorToNearest))
              P_ChangeSwitchTexture(line,1);
            return true;

          case 230:
            // Raise elevator next floor
            // 230 SR Raise Elevator next floor
            if (EV_DoElevator(line,elevateUp))
              P_ChangeSwitchTexture(line,1);
            return true;

          case 234:
            // Lower elevator next floor
            // 234 SR Lower Elevator next floor
            if (EV_DoElevator(line,elevateDown))
              P_ChangeSwitchTexture(line,1);
            return true;

          case 238:
            // Elevator to current floor
            // 238 SR Elevator to current floor
            if (EV_DoElevator(line,elevateCurrent))
              P_ChangeSwitchTexture(line,1);
            return true;

          case 258:
            // Build stairs, step 8
            // 258 SR EV_BuildStairs(build8)
            if (EV_BuildStairs(line,build8))
              P_ChangeSwitchTexture(line,1);
            return true;

          case 259:
            // Build stairs, step 16
            // 259 SR EV_BuildStairs(turbo16)
            if (EV_BuildStairs(line,turbo16))
              P_ChangeSwitchTexture(line,1);
            return true;

          // 1/29/98 jff end of added SR linedef types

        }
      break;

    // Buttons (retriggerable switches)
    case 42:
      // Close Door
      if (EV_DoDoor(line,doorClose))
        P_ChangeSwitchTexture(line,1);
      return true;

    case 43:
      // Lower Ceiling to Floor
      if (EV_DoCeiling(line,lowerToFloor))
        P_ChangeSwitchTexture(line,1);
      return true;

    case 45:
      // Lower Floor to Surrounding floor height
      if (EV_DoFloor(line,lowerFloor))
        P_ChangeSwitchTexture(line,1);
      return true;

    case 60:
      // Lower Floor to Lowest
      if (EV_DoFloor(line,lowerFloorToLowest))
        P_ChangeSwitchTexture(line,1);
      return true;

    case 61:
      // Open Door
      if (EV_DoDoor(line,doorOpen))
        P_ChangeSwitchTexture(line,1);
      return true;

    case 62:
      // PlatDownWaitUpStay
      if (EV_DoPlat(line,downWaitUpStay,1))
        P_ChangeSwitchTexture(line,1);
      return true;

    case 63:
      // Raise Door
      if (EV_DoDoor(line, doorNormal))
        P_ChangeSwitchTexture(line,1);
      return true;

    case 64:
      // Raise Floor to ceiling
      if (EV_DoFloor(line,raiseFloor))
        P_ChangeSwitchTexture(line,1);
      return true;

    case 66:
      // Raise Floor 24 and change texture
      if (EV_DoPlat(line,raiseAndChange,24))
        P_ChangeSwitchTexture(line,1);
      return true;

    case 67:
      // Raise Floor 32 and change texture
      if (EV_DoPlat(line,raiseAndChange,32))
        P_ChangeSwitchTexture(line,1);
      return true;

    case 65:
      // Raise Floor Crush
      if (EV_DoFloor(line,raiseFloorCrush))
        P_ChangeSwitchTexture(line,1);
      return true;

    case 68:
      // Raise Plat to next highest floor and change texture
      if (EV_DoPlat(line,raiseToNearestAndChange,0))
        P_ChangeSwitchTexture(line,1);
      return true;

    case 69:
      // Raise Floor to next highest floor
      if (EV_DoFloor(line, raiseFloorToNearest))
        P_ChangeSwitchTexture(line,1);
      return true;

    case 70:
      // Turbo Lower Floor
      if (EV_DoFloor(line,turboLower))
        P_ChangeSwitchTexture(line,1);
      return true;

    case 114:
      // Blazing Door Raise (faster than TURBO!)
      if (EV_DoDoor (line,blazeRaise))
        P_ChangeSwitchTexture(line,1);
      return true;

    case 115:
      // Blazing Door Open (faster than TURBO!)
      if (EV_DoDoor (line,blazeOpen))
        P_ChangeSwitchTexture(line,1);
      return true;

    case 116:
      // Blazing Door Close (faster than TURBO!)
      if (EV_DoDoor (line,blazeClose))
        P_ChangeSwitchTexture(line,1);
      return true;

    case 123:
      // Blazing PlatDownWaitUpStay
      if (EV_DoPlat(line,blazeDWUS,0))
        P_ChangeSwitchTexture(line,1);
      return true;

    case 132:
      // Raise Floor Turbo
      if (EV_DoFloor(line,raiseFloorTurbo))
        P_ChangeSwitchTexture(line,1);
      return true;

    case 99:
      // BlzOpenDoor BLUE
    case 134:
      // BlzOpenDoor RED
    case 136:
      // BlzOpenDoor YELLOW
      if (EV_DoLockedDoor (line,blazeOpen,thing))
        P_ChangeSwitchTexture(line,1);
      return true;

    case 138:
      // Light Turn On
      EV_LightTurnOn(line,255);
      P_ChangeSwitchTexture(line,1);
      return true;

    case 139:
      // Light Turn Off
      EV_LightTurnOn(line,35);
      P_ChangeSwitchTexture(line,1);
      return true;
  }
  return !bossaction;
}



//----------------------------------------------------------------------------
//
// $Log: p_switch.c,v $
// Revision 1.25  1998/06/01  14:48:19  jim
// Fix switch use from back side
//
// Revision 1.24  1998/05/11  14:04:46  jim
// Fix endianess of speed field in SWITCH predefined lump
//
// Revision 1.23  1998/05/07  21:30:40  jim
// formatted/documented p_switch
//
// Revision 1.22  1998/05/03  23:17:44  killough
// Fix #includes at the top, nothing else
//
// Revision 1.21  1998/03/24  22:10:32  jim
// Fixed switch won't light problem in UDOOM
//
// Revision 1.20  1998/03/23  18:39:45  jim
// Switch and animation tables now lumps
//
// Revision 1.19  1998/03/16  15:47:18  killough
// Merge Jim's linedef changes
//
// Revision 1.18  1998/03/15  14:39:58  jim
// added pure texture change linedefs & generalized sector types
//
// Revision 1.17  1998/03/14  17:19:15  jim
// Added instant toggle floor type
//
// Revision 1.16  1998/03/12  21:54:23  jim
// Freed up 12 linedefs for use as vectors
//
// Revision 1.15  1998/03/05  16:59:22  jim
// Fixed inability of monsters/barrels to use new teleports
//
// Revision 1.14  1998/03/02  15:33:10  jim
// fixed errors in numeric model sector search and 0 tag trigger defeats
//
// Revision 1.13  1998/02/28  01:25:06  jim
// Fixed error in 0 tag trigger fix
//
// Revision 1.11  1998/02/23  00:42:17  jim
// Implemented elevators
//
// Revision 1.10  1998/02/13  03:28:49  jim
// Fixed W1,G1 linedefs clearing untriggered special, cosmetic changes
//
// Revision 1.9  1998/02/09  03:11:03  killough
// Remove limit on switches
//
// Revision 1.8  1998/02/08  05:35:54  jim
// Added generalized linedef types
//
// Revision 1.6  1998/02/02  13:39:27  killough
// Program beautification
//
// Revision 1.5  1998/01/30  14:44:20  jim
// Added gun exits, right scrolling walls and ceiling mover specials
//
// Revision 1.2  1998/01/26  19:24:28  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:03:01  rand
// Lee's Jan 19 sources
//
//
//----------------------------------------------------------------------------
