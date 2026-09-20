//
//  Copyright (C) 1999 by
//  id Software, Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman
//  Copyright (C) 2013 James Haley et al.
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
// DESCRIPTION:
//  Color range translation support
//  Functions to draw patches (by post) directly to screen.
//  Functions to blit a block to the screen.
//
//-----------------------------------------------------------------------------

#ifndef __V_VIDEO__
#define __V_VIDEO__

#include "doomtype.h"
#include "m_fixed.h"
#include "r_defs.h"

//
// VIDEO
//

extern pixel_t *I_VideoBuffer;

typedef struct
{
    int width;
    int height;
    int unscaledw; // unscaled width with correction for widecreen
    int deltaw;    // widescreen delta

    fixed_t xscale; // x-axis scaling multiplier
    fixed_t yscale; // y-axis scaling multiplier
    fixed_t xstep;  // x-axis scaling step
    fixed_t ystep;  // y-axis scaling step
} video_t;

extern video_t video;

typedef struct
{
    int x;   // original x coordinate for upper left corner
    int y;   // original y coordinate for upper left corner
    int w;   // original width
    int h;   // original height

    int cx1; // clipped x coordinate for left edge
    int cx2; // clipped x coordinate for right edge
    int cy1; // clipped y coordinate for upper edge
    int cy2; // clipped y coordinate for lower edge
    int cw;  // clipped width
    int ch;  // clipped height

    int sx;  // scaled x
    int sy;  // scaled y
    int sw;  // scaled width
    int sh;  // scaled height
} vrect_t;

void V_ScaleRect(vrect_t *rect);
int V_ScaleX(int x);
int V_ScaleY(int y);

// Allocates buffer screens, call before R_Init.
void V_Init(void);

void V_UseBuffer(pixel_t *buffer, int pitch);

void V_RestoreBuffer(void);

void V_CopyRect(int srcx, int srcy, const pixel_t *source, int width, int height,
                int pitch, int destx, int desty);

typedef struct
{
    int left;
    int top;
    boolean center;
    int width;
    int height;
} crop_t;
extern crop_t no_crop;

// On-screen patch drawing functions for specific purposes

void V_DrawPatch(
    int x,
    int y,
    const patch_t *patch
);

void V_DrawPatchCastCall(
    const patch_t *patch,
    const byte *tranmap,
    const byte *xlat,
    boolean flip
);

void V_DrawPatchCropped(
    int x,
    int y,
    const patch_t *patch,
    const crop_t crop
);

void V_DrawPatchGeneral(
    int x,
    int y,
    int xoffset,
    int yoffset,
    const byte *tranmap,
    const byte *xlat,
    const patch_t *patch,
    const crop_t crop
);

void V_DrawPatchTranslated(
    int x,
    int y,
    const patch_t *patch,
    const byte* xlat
);

void V_DrawPatchTranslatedTwice(
    int x,
    int y,
    const patch_t *patch,
    const byte* xlat,
    const byte* xlat2
);

void V_DrawPatchFullScreen(const patch_t *patch);

// Draw a linear block of pixels into the view buffer.

void V_DrawBlock(int x, int y, int width, int height, const pixel_t *src);

// Reads a linear block of pixels into the view buffer.

void V_GetBlock(int x, int y, int width, int height, pixel_t *dest);

// [FG] non hires-scaling variant of V_DrawBlock, used in disk icon drawing

void V_PutBlock(int x, int y, int width, int height, const pixel_t *src);

void V_FillRect(int x, int y, int width, int height, byte color);

void V_TileBlock64(int line, int width, int height, const byte *src);

void V_DrawBackground(const char *patchname);

void V_ShadeScreen(void);

void V_ShadeRect(int x, int y, int width, int height);

// [FG] colored blood and gibs

int V_BloodColor(int blood);

void V_ScreenShot(void);

#endif

//----------------------------------------------------------------------------
//
// $Log: v_video.h,v $
// Revision 1.9  1998/05/06  11:12:54  jim
// Formattted v_video.*
//
// Revision 1.8  1998/05/03  22:53:58  killough
// beautification
//
// Revision 1.7  1998/04/24  08:09:44  jim
// Make text translate tables lumps
//
// Revision 1.6  1998/03/02  11:43:06  killough
// Add cr_blue_status for blue statusbar numbers
//
// Revision 1.5  1998/02/27  19:22:11  jim
// Range checked hud/sound card variables
//
// Revision 1.4  1998/02/19  16:55:06  jim
// Optimized HUD and made more configurable
//
// Revision 1.3  1998/02/17  23:00:41  jim
// Added color translation machinery and data
//
// Revision 1.2  1998/01/26  19:27:59  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:03:05  rand
// Lee's Jan 19 sources
//
//
//----------------------------------------------------------------------------
