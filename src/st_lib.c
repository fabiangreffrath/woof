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
//
// DESCRIPTION:
//      The status bar widget code.
//
//-----------------------------------------------------------------------------

#include "st_lib.h"

#include "m_swap.h"
#include "r_defs.h"
#include "v_video.h"
#include "w_wad.h"
#include "z_zone.h"

int sts_colored_numbers; //jff 2/18/98 control to disable status color changes
int sts_pct_always_gray; // killough 2/21/98: always gray %'s? bug or feature?

extern boolean st_crispyhud;

patch_t*    sttminus;

//
// STlib_init()
//
// Hack display negative frags. Loads and store the stminus lump.
//
// Passed nothing, returns nothing
//
void STlib_init(void)
{
  // [FG] allow playing with the Doom v1.2 IWAD which is missing the STTMINUS lump
  if (W_CheckNumForName("STTMINUS") >= 0)
  sttminus = (patch_t *) W_CacheLumpName("STTMINUS", PU_STATIC);
  else
    sttminus = NULL;
}

//
// STlib_initNum()
//
// Initializes an st_number_t widget
//
// Passed the widget, its position, the patches for the digits, a pointer
// to the value displayed, a pointer to the on/off control, and the width
// Returns nothing
//
void STlib_initNum
( st_number_t* n,
  int x,
  int y,
  patch_t** pl,
  int* num,
  boolean* on,
  int     width )
{
  n->x  = x;
  n->y  = y;
  n->oldnum = 0;
  n->width  = width;
  n->num  = num;
  n->on = on;
  n->p  = pl;
}

//
// STlib_drawNum()
// 
// A fairly efficient way to draw a number based on differences from the 
// old number.
//
// Passed a st_number_t widget and a color range for output.
// Returns nothing
//
static void STlib_drawNum
( st_number_t*  n,
  byte *outrng )       //jff 2/16/98 add color translation to digit output
{
  int   numdigits = n->width;
  int   num = *n->num;

  int   w = SHORT(n->p[0]->width);
  int   x = n->x;

  int   neg;

  n->oldnum = *n->num;

  neg = num < 0;

  if (neg)
  {
    if (numdigits == 2 && num < -9)
      num = -9;
    else if (numdigits == 3 && num < -99)
      num = -99;

    num = -num;
  }

  // if non-number, do not draw it
  if (num == LARGENUMBER)
    return;

  //jff 2/16/98 add color translation to digit output
  // in the special case of 0, you draw 0
  if (!num)
  {
    if (outrng && sts_colored_numbers)
      V_DrawPatchTranslated(x - w, n->y, n->p[ 0 ],outrng);
    else //jff 2/18/98 allow use of faster draw routine from config
      V_DrawPatch(x - w, n->y, n->p[ 0 ]);
  }

  // draw the new number
  //jff 2/16/98 add color translation to digit output
  while (num && numdigits--)
  {
    x -= w;
    if (outrng && sts_colored_numbers)
      V_DrawPatchTranslated(x, n->y, n->p[ num % 10 ],outrng);
    else //jff 2/18/98 allow use of faster draw routine from config
      V_DrawPatch(x, n->y, n->p[ num % 10 ]);
    num /= 10;
  }

  // draw a minus sign if necessary
  //jff 2/16/98 add color translation to digit output
  if (neg && sttminus)
  {
    w = SHORT(sttminus->width);
    if (outrng && sts_colored_numbers)
      V_DrawPatchTranslated(x - w, n->y, sttminus,outrng);
    else //jff 2/18/98 allow use of faster draw routine from config
      V_DrawPatch(x - w, n->y, sttminus);
  }
}

//
// STlib_updateNum()
//
// Draws a number conditionally based on the widget's enable
//
// Passed a number widget and the output color range
// Returns nothing
//
void STlib_updateNum
( st_number_t*    n,
  byte *outrng ) //jff 2/16/98 add color translation to digit output
{
  if (*n->on) STlib_drawNum(n, outrng);
}

//
// STlib_initPercent()
//
// Initialize a st_percent_t number with percent sign widget
//
// Passed a st_percent_t widget, the position, the digit patches, a pointer
// to the number to display, a pointer to the enable flag, and patch
// for the percent sign.
// Returns nothing.
//
void STlib_initPercent
( st_percent_t* p,
  int x,
  int y,
  patch_t** pl,
  int* num,
  boolean* on,
  patch_t* percent )
{
  STlib_initNum(&p->n, x, y, pl, num, on, 3);
  p->p = percent;
}

//
// STlib_updatePercent()
//
// Draws a number/percent conditionally based on the widget's enable
//
// Passed a precent widget and the output color range
// Returns nothing
//
void STlib_updatePercent
( st_percent_t*   per,
  byte *outrng )            //jff 2/16/98 add color translation to digit output
{
  // Remove the check for 'refresh' because this causes percent symbols to always appear
  // in automap overlay mode.
  if (*per->n.on) // killough 2/21/98: fix percents not updated;
  {
    V_DrawPatchTranslated
    (
      per->n.x,
      per->n.y,
      per->p,
      // [FG] fix always gray percent / always red mismatch
      sts_pct_always_gray ? cr_gray :
      !sts_colored_numbers ? NULL :
      outrng
    );
  }

  STlib_updateNum(&per->n, outrng);
}

//
// STlib_initMultIcon()
//
// Initialize a st_multicon_t widget, used for a multigraphic display
// like the status bar's keys.
//
// Passed a st_multicon_t widget, the position, the graphic patches, a pointer
// to the numbers representing what to display, and pointer to the enable flag
// Returns nothing.
//
void STlib_initMultIcon
( st_multicon_t* i,
  int x,
  int y,
  patch_t** il,
  int* inum,
  boolean* on )
{
  i->x  = x;
  i->y  = y;
  i->oldinum  = -1;
  i->inum = inum;
  i->on = on;
  i->p  = il;
}

//
// STlib_updateMultIcon()
//
// Draw a st_multicon_t widget, used for a multigraphic display
// like the status bar's keys. Displays each when the control
// numbers change
//
// Passed a st_multicon_t widget
// Returns nothing.
//
void STlib_updateMultIcon
( st_multicon_t*  mi )
{
  if (*mi->on)
  {
    if (*mi->inum != -1)  // killough 2/16/98: redraw only if != -1
      V_DrawPatch(mi->x, mi->y, mi->p[*mi->inum]);
  }
}

//----------------------------------------------------------------------------
//
// $Log: st_lib.c,v $
// Revision 1.8  1998/05/11  10:44:42  jim
// formatted/documented st_lib
//
// Revision 1.7  1998/05/03  22:58:17  killough
// Fix header #includes at top, nothing else
//
// Revision 1.6  1998/02/23  04:56:34  killough
// Fix percent sign problems
//
// Revision 1.5  1998/02/19  16:55:09  jim
// Optimized HUD and made more configurable
//
// Revision 1.4  1998/02/18  00:59:13  jim
// Addition of HUD
//
// Revision 1.3  1998/02/17  06:17:03  killough
// Add support for erasing keys in status bar
//
// Revision 1.2  1998/01/26  19:24:56  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:03:03  rand
// Lee's Jan 19 sources
//
//
//----------------------------------------------------------------------------

