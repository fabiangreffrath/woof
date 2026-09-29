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
//      Optional Z collision between things ("over/under" monsters).
//
//-----------------------------------------------------------------------------

#ifndef __P_OVERUNDER__
#define __P_OVERUNDER__

#include "doomtype.h"

struct mobj_s;

typedef enum
{
  OVERUNDER_OFF,
  OVERUNDER_PLAYER,
  OVERUNDER_ALL,
} overunder_t;

extern int overunder;

// Things that set tmfloorz or tmceilingz during the last P_CheckPosition().
extern struct mobj_s *tmbelow, *tmabove;

boolean P_CanOverUnder(const struct mobj_s *a, const struct mobj_s *b);
void    P_SetOverUnderLinks(struct mobj_s *mo);
void    P_UnlinkOverUnder(struct mobj_s *mo);
void    P_UpdateOverUnder(struct mobj_s *mo);
boolean P_IsOverUnderVictim(const struct mobj_s *thing);
boolean P_CrushOverUnderLink(struct mobj_s *thing);
boolean P_IsOverUnderPlayer(const struct mobj_s *thing);

#endif // __P_OVERUNDER__
