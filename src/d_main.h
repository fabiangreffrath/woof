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
//
// DESCRIPTION:
//      System specific interface stuff.
//
//-----------------------------------------------------------------------------

#ifndef __D_MAIN__
#define __D_MAIN__

#include "doomtype.h"

struct event_s;

void D_AddFile(const char *file);

const char *D_DoomExeName(void); // killough 10/98: executable's name
extern char *basesavegame;     // killough 2/16/98: savegame path
extern char *screenshotdir; // [FG] screenshot path
extern char *savegamename;

void D_SetSavegameDirectory(void);

extern const char *gamedescription;

//jff 1/24/98 make command line copies of play modes available
extern boolean clnomonsters; // checkparm of -nomonsters
extern boolean clrespawnparm;  // checkparm of -respawn
extern boolean clfastparm; // checkparm of -fast
extern boolean clpistolstart; // checkparm of -pistolstart
extern boolean clcoopspawns; // checkparm of -coop_spawns
//jff end of external declaration of command line playmode

void D_SetMaxHealth(void);
void D_SetBloodColor(void);

extern boolean quit_prompt;
extern boolean quit_sound;
extern boolean fast_exit;
boolean D_AllowEndDoom(void);

// Called by IO functions when input is detected.
void D_PostEvent(struct event_s *ev);

void D_BindMiscVariables(void);

//
// BASE LEVEL
//

void D_PageTicker(void);
void D_PageDrawer(void);
void D_AdvanceDemo(void);
void D_StartTitle(void);

extern boolean advancedemo;

#endif

//----------------------------------------------------------------------------
//
// $Log: d_main.h,v $
// Revision 1.7  1998/05/06  15:32:19  jim
// document g_game.c, change externals
//
// Revision 1.5  1998/05/03  22:27:08  killough
// Add external declarations
//
// Revision 1.4  1998/02/23  04:15:01  killough
// Remove obsolete function prototype
//
// Revision 1.3  1998/02/17  06:10:39  killough
// Add D_DoomExeDir prototype, basesavegame decl.
//
// Revision 1.2  1998/01/26  19:26:28  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:03:09  rand
// Lee's Jan 19 sources
//
//----------------------------------------------------------------------------
