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

#ifndef __P_TICK__
#define __P_TICK__

#include "d_think.h"

struct mobj_s;

// Called by C_Ticker, can call G_PlayerExited.
// Carries out all thinking of monsters and players.

void P_Ticker(void);

extern thinker_t thinkercap;  // Both the head and tail of the thinker list

void P_InitThinkers(void);
void P_AddThinker(thinker_t *thinker);
void P_RemoveThinker(thinker_t *thinker);
void P_RemoveThinkerDelayed(thinker_t *thinker);    // killough 4/25/98

void P_UpdateThinker(thinker_t *thinker);   // killough 8/29/98

void P_SetTarget(struct mobj_s **mo, struct mobj_s *target);   // killough 11/98

// killough 8/29/98: threads of thinkers, for more efficient searches
typedef enum {
   th_delete,  // haleyjd 11/09/06: giant bug fix
   th_misc,
   th_friends,
   th_enemies,
   NUMTHCLASS
} th_class;

extern thinker_t thinkerclasscap[];

#endif

//----------------------------------------------------------------------------
//
// $Log: p_tick.h,v $
// Revision 1.5  1998/05/15  00:36:22  killough
// Remove unnecessary crash hack
//
// Revision 1.4  1998/05/13  22:58:01  killough
// Restore Doom bug compatibility for demos
//
// Revision 1.3  1998/05/03  22:49:29  killough
// Add external declarations, formerly in p_local.h
//
// Revision 1.2  1998/01/26  19:27:31  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:03:08  rand
// Lee's Jan 19 sources
//
//----------------------------------------------------------------------------
