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
//      Status bar code.
//      Does the face/direction indicator animatin.
//      Does palette indicators as well (red pain/berserk, bright pickup)
//
//-----------------------------------------------------------------------------

#ifndef __STSTUFF_H__
#define __STSTUFF_H__

#include "doomdef.h"
#include "doomtype.h"

struct event_s;
struct patch_s;

// Size of statusbar.
// Now sensitive for scaling.

#define ST_HEIGHT 32
#define ST_WIDTH  SCREENWIDTH
#define ST_Y      (SCREENHEIGHT - ST_HEIGHT)

//
// STATUS BAR
//

// Called by main loop.
boolean ST_Responder(struct event_s *ev);

// Called by main loop.
void ST_Ticker(void);

// Called by main loop.
void ST_Drawer(void);

void ST_Erase(void);

// Called when the console player is spawned on each level.
void ST_Start(void);

// Called by startup code.
void ST_Init(void);

void ST_ResetPalette(void);

extern boolean st_refresh_background;

void ST_InitRes(void);

extern int health_red;    // health amount less than which status is red
extern int health_yellow; // health amount less than which status is yellow
extern int health_green;  // health amount above is blue, below is green

static inline boolean ST_PlayerInvulnerable(player_t *player)
{
    return (player->cheats & CF_GODMODE) ||
        (player->powers[pw_invulnerability] > 4 * 32) ||
        (player->powers[pw_invulnerability] & 8);
}

extern boolean palette_changes;

extern struct hudfont_s *stcfnt;
extern struct patch_s **hu_font;

void WI_UpdateWidgets(void);
void WI_DrawWidgets(void);

const char **ST_StatusbarList(void);

void ST_BindSTSVariables(void);

#endif

//----------------------------------------------------------------------------
//
// $Log: st_stuff.h,v $
// Revision 1.4  1998/05/03  22:50:55  killough
// beautification, move external declarations, remove cheats
//
// Revision 1.3  1998/04/19  01:10:39  killough
// Generalize cheat engine to add deh support
//
// Revision 1.2  1998/01/26  19:27:56  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:03:04  rand
// Lee's Jan 19 sources
//
//
//----------------------------------------------------------------------------
