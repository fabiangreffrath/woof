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
//      Refresh, visplane stuff (floor, ceilings).
//
//-----------------------------------------------------------------------------

#ifndef __R_PLANE__
#define __R_PLANE__

#include "m_fixed.h"

struct visplane_s;

// killough 10/98: special mask indicates sky flat comes from sidedef
#define PL_SKYFLAT (0x80000000)

// Visplane related.
extern  int *lastopening; // [FG] 32-bit integer math

extern int *floorclip, *ceilingclip; // [FG] 32-bit integer math
extern fixed_t *yslope, *distscale;

void R_InitPlanes(void);
void R_ClearPlanes(void);
void R_DrawPlanes (void);

struct visplane_s *R_FindPlane(fixed_t height, int picnum, int lightlevel,
                               fixed_t xoffs,  // killough 2/28/98: add x-y offsets
                               fixed_t yoffs);

struct visplane_s *R_CheckPlane(struct visplane_s *pl, int start, int stop);

// cph 2003/04/18 - create duplicate of existing visplane and set initial range
struct visplane_s *R_DupPlane(const struct visplane_s *pl, int start, int stop);

void R_InitPlanesRes(void);

void R_InitVisplanesRes(void);

#endif

//----------------------------------------------------------------------------
//
// $Log: r_plane.h,v $
// Revision 1.6  1998/04/27  01:48:34  killough
// Program beautification
//
// Revision 1.5  1998/03/02  11:47:16  killough
// Add support for general flats xy offsets
//
// Revision 1.4  1998/02/09  03:16:06  killough
// Change arrays to use MAX height/width
//
// Revision 1.3  1998/02/02  14:20:45  killough
// Made some functions static
//
// Revision 1.2  1998/01/26  19:27:42  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:03:03  rand
// Lee's Jan 19 sources
//
//
//----------------------------------------------------------------------------
