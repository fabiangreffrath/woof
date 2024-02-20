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
//      Cheat code checking.
//
//-----------------------------------------------------------------------------

#ifndef __M_CHEAT__
#define __M_CHEAT__

#include "doomtype.h"

struct event_s;

typedef void (*cheatf_v)();
typedef void (*cheatf_i)(int i);
typedef void (*cheatf_s)(char *s);

typedef union
{
  cheatf_v v;
  cheatf_i i;
  cheatf_s s;
} cheatf_t;

// killough 4/16/98: Cheat table structure

typedef enum {
  always   = 0,
  not_dm   = 1,
  not_coop = 2,
  not_demo = 4, 
  not_menu = 8,
  not_deh  = 16,
  beta_only = 32,                  // killough 7/24/98
  not_net = not_dm | not_coop
} cheat_when_t;

extern struct cheat_s {
  const char *cheat; // [FG] char!
  const char *const deh_cheat;
  const cheat_when_t when;
  const cheatf_t func;
  const int arg;
  uint64_t code, mask;
  boolean deh_modified;                // killough 9/12/98
} cheat[];

boolean M_CheatResponder(struct event_s *ev);

#endif

//----------------------------------------------------------------------------
//
// $Log: m_cheat.h,v $
// Revision 1.5  1998/05/03  22:10:56  killough
// Cheat engine, moved from st_stuff
//
// Revision 1.4  1998/05/01  14:38:08  killough
// beautification
//
// Revision 1.3  1998/02/09  03:03:07  killough
// Rendered obsolete by st_stuff.c
//
// Revision 1.2  1998/01/26  19:27:08  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:02:58  rand
// Lee's Jan 19 sources
//
//
//----------------------------------------------------------------------------
