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
//  Gamma correction LUT.
//  Color range translation support
//  Functions to draw patches (by post) directly to screen.
//  Functions to blit a block to the screen.
//
//-----------------------------------------------------------------------------

#ifndef __V_VIDEO__
#define __V_VIDEO__

#include "doomtype.h"
#include "doomdef.h"
#include "i_video.h"
// Needed because we are refering to patches.
#include "r_data.h"

//
// VIDEO
//

#define CENTERY     (SCREENHEIGHT/2)

extern int v_lightest_color, v_darkest_color;

//jff 2/16/98 palette color ranges for translation
//jff 2/18/98 conversion to palette lookups for speed
//jff 4/24/98 now pointers to lumps loaded 
extern char *cr_brick;
extern char *cr_tan;
extern char *cr_gray;
extern char *cr_green;
extern char *cr_brown;
extern char *cr_gold;
extern char *cr_red;
extern char *cr_blue;
extern char *cr_blue2;
extern char *cr_orange;
extern char *cr_yellow;
extern char *cr_black;
extern char *cr_purple;
extern char *cr_white;
// [FG] dark/shaded color translation table
extern char *cr_dark;

// array of pointers to color translation tables
extern char *colrngs[];
extern char *red2col[];

// symbolic indices into color translation table pointer array
typedef enum
{
  CR_BRICK,   //0
  CR_TAN,     //1
  CR_GRAY,    //2
  CR_GREEN,   //3
  CR_BROWN,   //4
  CR_GOLD,    //5
  CR_RED,     //6
  CR_BLUE1,   //7
  CR_ORANGE,  //8
  CR_YELLOW,  //9
  CR_BLUE2,   //10
  CR_BLACK,   //11
  CR_PURPLE,  //12
  CR_WHITE,   //13
  CR_NONE,    //14 // [FG] dummy
  CR_LIMIT    //15 //jff 2/27/98 added for range check
} crange_idx_e;
//jff 1/16/98 end palette color range additions

extern pixel_t *I_VideoBuffer;

extern byte gammatable[5][256];

//jff 4/24/98 loads color translation lumps
void V_InitColorTranslation(void);

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

// Allocates buffer screens, call before R_Init.
void V_Init (void);

void V_UseBuffer(pixel_t *buffer);

void V_RestoreBuffer(void);

void V_CopyRect(int srcx, int srcy, pixel_t *source,
                int width, int height,
                int destx, int desty);

// killough 11/98: Consolidated V_DrawPatch and V_DrawPatchFlipped

void V_DrawPatchGeneral(int x, int y, patch_t *patch, boolean flipped);

#define V_DrawPatch(x, y, p)        V_DrawPatchGeneral(x, y, p, false)

#define V_DrawPatchFlipped(x, y, p) V_DrawPatchGeneral(x, y, p, true)

#define V_DrawPatchDirect V_DrawPatch       /* killough 5/2/98 */

void V_DrawPatchTranslated(int x, int y, patch_t *patch, char *outr);

void V_DrawPatchFullScreen(patch_t *patch);

// Draw a linear block of pixels into the view buffer.

void V_DrawBlock(int x, int y, int width, int height, pixel_t *src);

// Reads a linear block of pixels into the view buffer.

void V_GetBlock(int x, int y, int width, int height, pixel_t *dest);

// [FG] non hires-scaling variant of V_DrawBlock, used in disk icon drawing

void V_PutBlock(int x, int y, int width, int height, pixel_t *src);

void V_FillRect(int x, int y, int width, int height, byte color);

void V_ShadeScreen(void);

void V_TileBlock64(int line, int width, int height, const byte *src);

void V_DrawBackground(const char *patchname);

// [FG] colored blood and gibs

int V_BloodColor(int blood);

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

