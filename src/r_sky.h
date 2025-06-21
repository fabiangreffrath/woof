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
//      Sky rendering.
//
//-----------------------------------------------------------------------------

#ifndef __R_SKY__
#  define __R_SKY__

#  include "doomtype.h"
#  include "r_defs.h"
#  include "r_plane.h"
#  include "r_skydefs.h"

// SKY, store the number for name.
#  define SKYFLATNAME       "F_SKY1"

// The sky map is 256*128*4 maps.
#  define ANGLETOSKYSHIFT   22

// [FG] stretch short skies
#  define SKYSTRETCH_HEIGHT 228
extern boolean stretchsky;

// [FG] linear horizontal sky scrolling
extern boolean linearsky;

extern sky_t *levelskies;
void R_ClearLevelskies(void);
skyindex_t R_AddLevelsky(int texture);
skyindex_t R_AddLevelskyFromLine(line_t *line);
sky_t *R_GetLevelsky(skyindex_t index);
void R_UpdateStretchSkies(void);

// Called whenever the view size changes.
void R_InitSkyMap(void);

byte R_GetSkyColor(int texturenum);

void R_UpdateSkies(void);

#  define FIRE_WIDTH  128
#  define FIRE_HEIGHT 320

byte *R_GetFireColumn(int col, fire_t *fire);

#endif

//----------------------------------------------------------------------------
//
// $Log: r_sky.h,v $
// Revision 1.4  1998/05/03  22:56:25  killough
// Add m_fixed.h #include
//
// Revision 1.3  1998/05/01  14:15:29  killough
// beautification
//
// Revision 1.2  1998/01/26  19:27:46  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:03:09  rand
// Lee's Jan 19 sources
//
//
//----------------------------------------------------------------------------
