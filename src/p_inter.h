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
//
//
//-----------------------------------------------------------------------------

#ifndef __P_INTER__
#define __P_INTER__

#include "doomtype.h"
#include "hu_obituary.h"

struct player_s;
struct mobj_s;

// Ty 03/09/98 Moved to an int in p_inter.c for deh and externalization 
#define MAXHEALTH maxhealth

// follow a player exlusively for 3 seconds
#define BASETHRESHOLD   (100)

boolean P_GivePower(struct player_s *, int);
void P_TouchSpecialThing(struct mobj_s *special, struct mobj_s *toucher);
void P_DamageMobjBy(struct mobj_s *target, struct mobj_s *inflictor,
                    struct mobj_s *source, int damage, method_t method);
#define P_DamageMobj(a, b, c, d) P_DamageMobjBy(a, b, c, d, MOD_None)

// killough 5/2/98: moved from d_deh.c, g_game.c, m_misc.c, others:

extern int god_health;   // Ty 03/09/98 - deh support, see also p_inter.c
extern int idfa_armor;
extern int idfa_armor_class;
extern int idkfa_armor;
extern int idkfa_armor_class;  // Ty - end
// Ty 03/13/98 - externalized initial settings for respawned player
extern int initial_health;
extern int initial_bullets;
extern int maxhealth;
extern int maxhealthbonus;
extern int max_armor;
extern int green_armor_class;
extern int blue_armor_class;
extern int max_soul;
extern int soul_health;
extern int mega_health;
extern int bfgcells;
extern int deh_species_infighting;
extern int maxammo[], clipammo[];

#endif

//----------------------------------------------------------------------------
//
// $Log: p_inter.h,v $
// Revision 1.3  1998/05/03  23:08:57  killough
// beautification, add of the DEH parameter declarations
//
// Revision 1.2  1998/01/26  19:27:19  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:03:08  rand
// Lee's Jan 19 sources
//
//
//----------------------------------------------------------------------------
