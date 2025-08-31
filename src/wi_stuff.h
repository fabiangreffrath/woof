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
//  Intermission.
//
//-----------------------------------------------------------------------------

#ifndef __WI_STUFF__
#define __WI_STUFF__

#include "doomtype.h"

struct wbstartstruct_s;

// States for the intermission

typedef enum
{
  NoState = -1,
  StatCount,
  ShowNextLoc
} stateenum_t;

extern int acceleratestage;

// Called by main loop, animate the intermission.
void WI_Ticker (void);

// Called by main loop,
// draws the intermission directly into the screen buffer.
void WI_Drawer (void);

// Setup for an intermission screen.
void WI_Start(struct wbstartstruct_s *wbstartstruct);

void WI_checkForAccelerate(void);      // killough 11/98

void WI_slamBackground(void);          // killough 11/98

extern boolean wi_overlay;
void WI_drawOverlayStats(void);

#endif

//----------------------------------------------------------------------------
//
// $Log: wi_stuff.h,v $
// Revision 1.3  1998/05/04  21:36:12  thldrmn
// commenting and reformatting
//
// Revision 1.2  1998/01/26  19:28:03  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:03:05  rand
// Lee's Jan 19 sources
//
//
//----------------------------------------------------------------------------
