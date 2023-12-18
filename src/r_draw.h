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

#include "r_defs.h"

extern lighttable_t *dc_colormap[2];
extern int      dc_x;
extern int      dc_yl;
extern int      dc_yh;
extern fixed_t  dc_iscale;
extern fixed_t  dc_texturemid;
extern int      dc_texheight;    // killough
extern byte     dc_skycolor;

// first pixel in a column
extern byte     *dc_source;         
extern const byte *dc_brightmap;

// The span blitting interface.
// Hook in assembler or system specific BLT here.

void R_DrawColumn(void);
void R_DrawTLColumn(void);      // drawing translucent textures // phares
extern void (*R_DrawFuzzColumn)(void);    // The Spectre/Invisibility effect.

// [crispy] draw fuzz effect independent of rendering frame rate
void R_SetFuzzPosTic(void);
void R_SetFuzzPosDraw(void);

// [FG] spectre drawing mode
extern int fuzzcolumn_mode;
void R_SetFuzzColumnMode(void);

void R_DrawSkyColumn(void);

// Draw with color translation tables, for player sprite rendering,
//  Green/Red/Blue/Indigo shirts.

void R_DrawTranslatedColumn(void);

// [JN] translated and translucent column
void R_DrawTranslatedTLColumn(void);

extern lighttable_t *ds_colormap[2];

extern int     ds_y;
extern int     ds_x1;
extern int     ds_x2;
extern fixed_t ds_xfrac;
extern fixed_t ds_yfrac;
extern fixed_t ds_xstep;
extern fixed_t ds_ystep;

// start of a 64*64 tile image
extern byte *ds_source;              
extern byte *translationtables;
extern byte *dc_translation;
extern const byte *ds_brightmap;

// Span blitting for rows, floor/ceiling. No Spectre effect needed.
void R_DrawSpan(void);

void R_InitBuffer(void);

// Initialize color translation tables, for player rendering etc.
void R_InitTranslationTables(void);

// Rendering function.
void R_FillBackScreen(void);
void R_DrawBorder(int x, int y, int w, int h);

// If the view size is not full screen, draws a border around it.
void R_DrawViewBorder(void);

void R_InitBufferRes(void);

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
