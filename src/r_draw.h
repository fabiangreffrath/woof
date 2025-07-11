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

#ifndef __R_DRAW__
#define __R_DRAW__

#include "doomtype.h"
#include "m_fixed.h"

typedef struct
{
    lighttable_t *colormap[2];
    int      x;
    int      yl;
    int      yh;
    fixed_t  iscale;
    fixed_t  texturemid;
    int      texheight; // killough
    byte     skycolor;
    byte     *source; // first pixel in a column
    const byte *brightmap;
    byte *translation;
} dc_t;
extern dc_t *const dc;

// The span blitting interface.
// Hook in assembler or system specific BLT here.

extern void (*R_DrawColumn)(void);
extern void (*R_DrawTLColumn)(void);      // drawing translucent textures // phares
extern void (*R_DrawFuzzColumn)(void);    // The Spectre/Invisibility effect.

// [crispy] draw fuzz effect independent of rendering frame rate
void R_SetFuzzPosTic(void);
void R_SetFuzzPosDraw(void);

// [FG] spectre drawing mode
typedef enum
{
    FUZZ_BLOCKY,
    FUZZ_REFRACTION,
    FUZZ_SHADOW,
    FUZZ_ORIGINAL
} fuzzmode_t;

extern fuzzmode_t fuzzmode;
void R_SetFuzzColumnMode(void);

void R_DrawSkyColumn(void);
void R_DrawSkyColumnMasked(void);

// Draw with color translation tables, for player sprite rendering,
//  Green/Red/Blue/Indigo shirts.

extern void (*R_DrawTranslatedColumn)(void);

typedef struct
{
    lighttable_t *colormap[2];
    int     y;
    int     x1;
    int     x2;
    fixed_t xfrac;
    fixed_t yfrac;
    fixed_t xstep;
    fixed_t ystep;
    byte *source; // start of a 64*64 tile image
    const byte *brightmap;
} ds_t;
extern ds_t *const ds;

// Span blitting for rows, floor/ceiling. No Spectre effect needed.
extern void (*R_DrawSpan)(void);

void R_InitBuffer(void);

// Initialize color translation tables, for player rendering etc.
void R_InitTranslationTables(void);
extern byte *translationtables;

// Rendering function.
void R_VideoErase(int x, int y, int w, int h);
void R_FillBackScreen(void);
void R_DrawBorder(int x, int y, int w, int h);

// If the view size is not full screen, draws a border around it.
void R_DrawViewBorder(void);

void R_InitBufferRes(void);

void R_InitDrawFunctions(void);

#endif

//----------------------------------------------------------------------------
//
// $Log: r_draw.h,v $
// Revision 1.5  1998/05/03  22:42:23  killough
// beautification, extra declarations
//
// Revision 1.4  1998/04/12  01:58:11  killough
// Add main_tranmap
//
// Revision 1.3  1998/03/02  11:51:55  killough
// Add translucency declarations
//
// Revision 1.2  1998/01/26  19:27:38  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:03:09  rand
// Lee's Jan 19 sources
//
//
//----------------------------------------------------------------------------
