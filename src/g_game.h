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
//-----------------------------------------------------------------------------

#ifndef __G_GAME__
#define __G_GAME__

#include "doomdef.h"
#include "d_event.h"
#include "d_ticcmd.h"
#include "m_fixed.h"

//
// GAME
//

// killough 5/2/98: number of bytes reserved for saving options
#define GAME_OPTION_SIZE 64

#define MBF21_GAME_OPTION_SIZE (21 + MBF21_COMP_TOTAL)

void G_PrepTiccmd(void);
boolean G_MovementResponder(event_t *ev);
boolean G_Responder(event_t *ev);
boolean G_CheckDemoStatus(void);
void G_DeathMatchSpawnPlayer(int playernum);
void G_InitNew(skill_t skill, int episode, int map);
void G_DeferedInitNew(skill_t skill, int episode, int map);
void G_DeferedPlayDemo(char *demo);
void G_LoadGame(char *name, int slot, boolean is_command); // killough 5/15/98
void G_ForcedLoadGame(void);           // killough 5/15/98: forced loadgames
void G_SaveGame(int slot, char *description); // Called by M_Responder.
void G_RecordDemo(char *name);              // Only called by startup code.
void G_BeginRecording(void);
void G_PlayDemo(char *name);
void G_ExitLevel(void);
void G_SecretExitLevel(void);
void G_WorldDone(void);
void G_Ticker(void);
void G_ScreenShot(void);
void G_UpdateSideMove(void);
void G_ReloadDefaults(boolean keep_demover); // killough 3/1/98: loads game defaults
char *G_SaveGameName(int); // killough 3/22/98: sets savegame filename
char *G_MBFSaveGameName(int); // MBF savegame filename
void G_SetFastParms(int);        // killough 4/10/98: sets -fast parameters
void G_DoNewGame(void);
void G_DoReborn(int playernum);
byte *G_ReadOptions(byte *demo_p);         // killough 3/1/98
byte *G_ReadOptionsMBF21(byte *demo_p);
byte *G_WriteOptions(byte *demo_p);        // killough 3/1/98
void G_PlayerReborn(int player);
void G_DoVictory(void);

int G_ValidateMapName(const char *mapname, int *pEpi, int *pMap);

void G_EnableWarp(boolean warp);

int G_GetNamedComplevel (const char *arg);
const char *G_GetCurrentComplevelName(void);

typedef enum
{
  CL_VANILLA,
  CL_BOOM,
  CL_MBF,
  CL_MBF21,
} complevel_t;

extern complevel_t default_complevel;
extern boolean force_complevel;

// killough 5/2/98: moved from m_misc.c:
extern int  key_escape;
extern int  key_enter;
extern int  key_help;
extern int  autorun;           // always running?                   // phares
extern boolean autostrafe50;
extern int  novert;
extern boolean mouselook;
extern boolean padlook;

extern fixed_t *forwardmove;
extern fixed_t *sidemove;

extern int  defaultskill;      //jff 3/24/98 default skill
extern boolean haswolflevels;  //jff 4/18/98 wolf levels present

extern int  bodyquesize, default_bodyquesize; // killough 2/8/98, 10/98

// killough 5/2/98: moved from d_deh.c:
// Par times (new item with BOOM) - from g_game.c
extern int pars[][10];  // hardcoded array size
extern int cpars[];     // hardcoded array size

#endif

//----------------------------------------------------------------------------
//
// $Log: g_game.h,v $
// Revision 1.10  1998/05/16  09:17:02  killough
// Make loadgame checksum friendlier
//
// Revision 1.9  1998/05/06  15:15:59  jim
// Documented IWAD routines
//
// Revision 1.8  1998/05/03  22:15:50  killough
// Add all external declarations in g_game.c
//
// Revision 1.7  1998/04/27  02:00:53  killough
// Add gcc __attribute__ to check dprintf() format string
//
// Revision 1.6  1998/04/10  06:34:35  killough
// Fix -fast parameter bugs
//
// Revision 1.5  1998/03/23  03:15:02  killough
// Add G_SaveGameName()
//
// Revision 1.4  1998/03/16  12:29:53  killough
// Remember savegame slot when loading
//
// Revision 1.3  1998/03/02  11:28:46  killough
// Add G_ReloadDefaults() prototype
//
// Revision 1.2  1998/01/26  19:26:51  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:02:55  rand
// Lee's Jan 19 sources
//
//----------------------------------------------------------------------------
