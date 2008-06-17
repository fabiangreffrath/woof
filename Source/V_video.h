// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id: v_video.h,v 1.9 1998/05/06 11:12:54 jim Exp $
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
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 
//  02111-1307, USA.
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
// Needed because we are refering to patches.
#include "r_data.h"

//
// VIDEO
//

#define CENTERY     (SCREENHEIGHT/2)

// Screen 0 is the screen updated by I_Update screen.
// Screen 1 is an extra buffer.

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
extern char *cr_blue_status; //killough 2/28/98
extern char *cr_orange;
extern char *cr_yellow;

// array of pointers to color translation tables
extern char *colrngs[];

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
  CR_BLUE,    //7
  CR_ORANGE,  //8
  CR_YELLOW,  //9
  CR_LIMIT    //10 //jff 2/27/98 added for range check
} crange_idx_e;
//jff 1/16/98 end palette color range additions

#define CR_DEFAULT CR_RED   /* default value for out of range colors */

extern byte *screens[5];
extern int  dirtybox[4];
extern byte gammatable[5][256];
extern int  usegamma, hires;        // killough 11/98

//jff 4/24/98 loads color translation lumps
void V_InitColorTranslation(void);

// Allocates buffer screens, call before R_Init.
void V_Init (void);

void V_CopyRect(int srcx,  int srcy,  int srcscrn, int width, int height,
		int destx, int desty, int destscrn);

// killough 11/98: Consolidated V_DrawPatch and V_DrawPatchFlipped

void V_DrawPatchGeneral(int x,int y,int scrn,patch_t *patch, boolean flipped);

#define V_DrawPatch(x,y,s,p)        V_DrawPatchGeneral(x,y,s,p,false)
#define V_DrawPatchFlipped(x,y,s,p) V_DrawPatchGeneral(x,y,s,p,true)

#define V_DrawPatchDirect V_DrawPatch       /* killough 5/2/98 */

void V_DrawPatchTranslated(int x, int y, int scrn, patch_t *patch, char *outr,
                           int cm);

// Draw a linear block of pixels into the view buffer.

void V_DrawBlock(int x, int y, int scrn, int width, int height, byte *src);

// Reads a linear block of pixels into the view buffer.

void V_GetBlock(int x, int y, int scrn, int width, int height, byte *dest);

#define V_MarkRect(x,y,width,height)  /* killough 11/98: unused */

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

