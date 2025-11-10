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
//   Setup a game, startup stuff.
//
//-----------------------------------------------------------------------------

#ifndef __P_SETUP__
#define __P_SETUP__

#include "doomdef.h"
#include "doomtype.h"
#include "m_fixed.h"
#include "r_defs.h"

void P_SetupLevel(int episode, int map, int playermask, skill_t skill);
void P_Init(void);               // Called by startup code.

extern byte     *rejectmatrix;   // for fast sight rejection

// killough 3/1/98: change blockmap from "short" to "long" offsets:
extern int32_t  *blockmaplump;   // offsets in blockmap are from here
extern int32_t  *blockmap;
extern int      bmapwidth;
extern int      bmapheight;      // in mapblocks
extern fixed_t  bmaporgx;
extern fixed_t  bmaporgy;        // origin of block map
extern struct mobj_s **blocklinks;    // for thing chains
extern int blocklinks_size;

extern boolean skipblstart; // MaxW: Skip initial blocklist short

struct sector_s *GetSectorAtNullAddress(void);
void P_DegenMobjThinker(struct mobj_s *mobj);
void P_SegLengths(void);

void P_CreateBlockMap(void);
void P_SetSkipBlockStart(void);
int P_GroupLines (void);
void P_SectorInit(sector_t * const sector);
void P_SidedefInit(side_t * const sidedef);
void P_LinedefInit(line_t * const linedef);
void P_ProcessSideDefs(side_t *side, int i, char *bottomtexture, char *midtexture, char *toptexture);

#endif

//----------------------------------------------------------------------------
//
// $Log: p_setup.h,v $
// Revision 1.3  1998/05/03  23:03:31  killough
// beautification, add external declarations for blockmap
//
// Revision 1.2  1998/01/26  19:27:28  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:03:08  rand
// Lee's Jan 19 sources
//
//----------------------------------------------------------------------------
