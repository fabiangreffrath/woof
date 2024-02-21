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
//   Related to f_finale.c, which is called at the end of a level
//    
//-----------------------------------------------------------------------------


#ifndef __F_FINALE__
#define __F_FINALE__

#include "doomtype.h"

struct event_s;

//
// FINALE
//

// Called by main loop.
boolean F_Responder(struct event_s *ev);

// Called by main loop.
void F_Ticker (void);

// Called by main loop.
void F_Drawer (void);

void F_StartFinale (void);

#endif

//----------------------------------------------------------------------------
//
// $Log: f_finale.h,v $
// Revision 1.3  1998/05/04  21:58:52  thldrmn
// commenting and reformatting
//
// Revision 1.2  1998/01/26  19:26:47  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:02:54  rand
// Lee's Jan 19 sources
//
//
//----------------------------------------------------------------------------
