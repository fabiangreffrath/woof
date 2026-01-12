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
//  AutoMap module.
//
//-----------------------------------------------------------------------------

#ifndef __AMMAP_H__
#define __AMMAP_H__

#include "doomtype.h"
#include "m_fixed.h"

struct event_s;

// Used by ST StatusBar stuff.
#define AM_MSGHEADER (('a'<<24)+('m'<<16))
#define AM_MSGENTERED (AM_MSGHEADER | ('e'<<8))
#define AM_MSGEXITED (AM_MSGHEADER | ('x'<<8))

// Called by main loop.
boolean AM_Responder(struct event_s *ev);

// Called by main loop.
void AM_Ticker (void);

// Called by main loop,
// called instead of view drawer if automap active.
void AM_Drawer (void);

// Called to force the automap to quit
// if the level is completed while it is up.
void AM_Stop (void);

// killough 2/22/98: for saving automap information in savegame:

extern void AM_Start(void);

//jff 4/16/98 make externally available

extern void AM_clearMarks(void);

extern void AM_initVariables(void);

void AM_SetMapCenter(fixed_t x, fixed_t y);

void AM_ResetScreenSize(void);

void AM_ColorPreset(void);
void AM_ApplyColors(boolean force);
void AM_ResetThickness(void);

void AM_BindAutomapVariables(void);

typedef struct
{
   int64_t x, y;
} mpoint_t;

typedef struct
{
    mpoint_t a, b;
} mline_t;

typedef struct
{
    mline_t *player;
    mline_t *player_cheat;
    mline_t *thing;
    mline_t *key;
} amdef_t;

amdef_t *AM_ParseAmDef(void);

extern mpoint_t *markpoints;
extern int markpointnum, markpointnum_max;
extern boolean followplayer;
extern boolean automap_grid;
extern int ddt_cheating;

extern boolean map_smooth_lines;

// end changes -- killough 2/22/98

#endif

//----------------------------------------------------------------------------
//
// $Log: am_map.h,v $
// Revision 1.7  1998/05/10  12:05:18  jim
// formatted/documented am_map
//
// Revision 1.6  1998/05/03  22:12:58  killough
// Add most external automap variable declarations
//
// Revision 1.5  1998/04/16  16:17:05  jim
// Fixed disappearing marks after new level
//
// Revision 1.4  1998/03/02  11:23:10  killough
// Add automap_grid decl for savegames
//
// Revision 1.3  1998/02/23  04:10:11  killough
// Remove limit on automap marks, save them in savegame
//
// Revision 1.2  1998/01/26  19:26:19  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:02:50  rand
// Lee's Jan 19 sources
//
//
//----------------------------------------------------------------------------
