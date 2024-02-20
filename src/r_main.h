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
//      System specific interface stuff.
//
//-----------------------------------------------------------------------------

#ifndef __R_MAIN__
#define __R_MAIN__

#include "d_player.h"
#include "r_data.h"

//
// POV related.
//

#define MAX_PITCH_ANGLE (32 * ANG1)

extern fixed_t  viewcos;
extern fixed_t  viewsin;
extern int      viewwindowx;
extern int      viewwindowy;
extern int      centerx;
extern int      centery;
extern fixed_t  centerxfrac;
extern fixed_t  centeryfrac;
extern fixed_t  projection;
extern fixed_t  skyiscale;
extern int      validcount;
extern int      linecount;
extern int      loopcount;
extern fixed_t  viewheightfrac; // [FG] sprite clipping optimizations

//
// Rendering stats
//

extern int rendered_visplanes, rendered_segs, rendered_vissprites, rendered_voxels;

//
// Lighting LUT.
// Used for z-depth cuing per column/row,
//  and other lighting effects (sector ambient, flash).
//

// Lighting constants.

extern int LIGHTLEVELS;
extern int LIGHTSEGSHIFT;
extern int LIGHTBRIGHT;
extern int MAXLIGHTSCALE;
extern int LIGHTSCALESHIFT;
extern int MAXLIGHTZ;
extern int LIGHTZSHIFT;

// killough 3/20/98: Allow colormaps to be dynamic (e.g. underwater)
extern lighttable_t **(*scalelight);
extern lighttable_t **(*zlight);
extern int numcolormaps;    // killough 4/4/98: dynamic number of maps
// killough 3/20/98, 4/4/98: end dynamic colormaps

extern boolean setsmoothlight;
void R_SmoothLight(void);

extern int          extralight;
extern lighttable_t *fixedcolormap;

// Number of diminishing brightness levels.
// There a 0-31, i.e. 32 LUT in the COLORMAP lump.

#define NUMCOLORMAPS 32

//
// Function pointer to switch refresh/drawing functions.
//

extern void (*colfunc)(void);

//
// Utility functions.
//

int R_PointOnSide(fixed_t x, fixed_t y, node_t *node);
int R_PointOnSegSide(fixed_t x, fixed_t y, seg_t *line);
angle_t R_PointToAngle(fixed_t x, fixed_t y);
angle_t R_PointToAngle2(fixed_t x1, fixed_t y1, fixed_t x2, fixed_t y2);
angle_t R_PointToAngleCrispy(fixed_t x, fixed_t y);
subsector_t *R_PointInSubsector(fixed_t x, fixed_t y);

//
// REFRESH - the actual rendering functions.
//

void R_RenderPlayerView(player_t *player);   // Called by G_Drawer.
void R_Init(void);                           // Called by startup code.
void R_SetViewSize(int blocks);              // Called by M_Responder.

void R_InitLightTables(void);                // killough 8/9/98
int R_GetLightIndex(fixed_t scale);

extern boolean setsizeneeded;
void R_ExecuteSetViewSize(void);

void R_InitAnyRes(void);

// [AM] Fractional part of the current tic, in the half-open
//      range of [0.0, 1.0).  Used for interpolation.
extern fixed_t fractionaltic;

inline static fixed_t LerpFixed(fixed_t oldvalue, fixed_t newvalue)
{
    return (oldvalue + FixedMul(newvalue - oldvalue, fractionaltic));
}

// [AM] Interpolate between two angles.
inline static angle_t LerpAngle(angle_t oangle, angle_t nangle)
{
    if (nangle == oangle)
        return nangle;
    else if (nangle > oangle)
    {
        if (nangle - oangle < ANG270)
            return oangle + (angle_t)((nangle - oangle) * FIXED2DOUBLE(fractionaltic));
        else // Wrapped around
            return oangle - (angle_t)((oangle - nangle) * FIXED2DOUBLE(fractionaltic));
    }
    else // nangle < oangle
    {
        if (oangle - nangle < ANG270)
            return oangle - (angle_t)((oangle - nangle) * FIXED2DOUBLE(fractionaltic));
        else // Wrapped around
            return oangle + (angle_t)((nangle - oangle) * FIXED2DOUBLE(fractionaltic));
    }
}

extern double deltatics;
extern boolean raw_input;

#endif

//----------------------------------------------------------------------------
//
// $Log: r_main.h,v $
// Revision 1.7  1998/05/03  23:00:42  killough
// beautification
//
// Revision 1.6  1998/04/06  04:43:17  killough
// Make colormaps fully dynamic
//
// Revision 1.5  1998/03/23  03:37:44  killough
// Add support for arbitrary number of colormaps
//
// Revision 1.4  1998/03/09  07:27:23  killough
// Avoid using FP for point/line queries
//
// Revision 1.3  1998/02/02  13:29:10  killough
// performance tuning
//
// Revision 1.2  1998/01/26  19:27:41  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:03:08  rand
// Lee's Jan 19 sources
//
//
//----------------------------------------------------------------------------
