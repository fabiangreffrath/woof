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
// DESCRIPTION:  none
//
//-----------------------------------------------------------------------------

#ifndef __HULIB__
#define __HULIB__

// We are referring to patches.
#include "r_defs.h"
#include "v_video.h"  //jff 2/16/52 include color range defs

#define CR_ORIG (-1) // [FG] reset to original color

// background and foreground screen numbers
// different from other modules.
#define BG      1
#define FG      0

#define HU_MAXLINELENGTH  80

//jff 2/26/98 maximum number of messages allowed in refresh list
#define HU_MAXMESSAGES 8

typedef enum {
    // [FG] h_align / v_align
    align_direct,

    // [FG] h_align
    align_left,
    align_right,
    align_center,

    // [FG] v_align
    align_top,
    align_bottom,

    num_aligns,
} align_t;

// a single line of information

typedef struct
{
  struct hu_multiline_s *multiline;

  char  l[HU_MAXLINELENGTH];

  // length in chars
  int   len;

  // length in chars
  int   width;

} hu_textline_t;

// an array of textlines

typedef struct hu_multiline_s
{
  struct hu_widget_s *widget;

  hu_textline_t *l[HU_MAXMESSAGES]; // text lines to draw
  int     nl;                          // number of lines
  int     cl;                          // current line number

  patch_t ***f;                         // font
  char *cr;                         //jff 2/16/52 output color range
  boolean drawcursor;

  // pointer to boolean stating whether to update window
  boolean*    on;
  boolean   laston;             // last value of *->on.

  void (*builder)(void);
  boolean built;

} hu_multiline_t;

// alignment and placement information for a multiline

typedef struct hu_widget_s {
  struct hu_multiline_s *multiline;

  align_t h_align, v_align;

  // [FG] align_direct
  int x, y;

} hu_widget_t;

//
// Widget creation, access, and update routines
//

// initializes heads-up widget library
void HUlib_init(void);

//
// textline code
//

// clear a line of text
void HUlib_clearTextLine(hu_textline_t *t);
void HUlib_clearMultiline(hu_multiline_t* t);

// returns success
//boolean HUlib_addCharToTextLine(hu_textline_t *t, char ch);
//void HUlib_addStringToTextLine(hu_textline_t *t, char *s);
void HUlib_addStringToCurrentLine(hu_multiline_t *t, char *s);

// draws tline
void HUlib_drawWidget(hu_widget_t *w);
void HUlib_resetAlignOffsets();
void HUlib_setMargins (void);

//
// Scrolling Text window widget routines
//

// add a text message to an stext widget
void HUlib_addMessageToSText
( hu_multiline_t* s,
  char*   prefix,
  char*   msg );

//jff 2/26/98 message refresh widget
// initialize refresh text widget
void HUlib_initMText(hu_multiline_t *m,
                     int ml,
                     patch_t ***f,
                     char *cr,
                     boolean *on,
                     void (*builder)(void))
;

//jff 2/26/98 message refresh widget
// add a text message to refresh text widget
void HUlib_addMessageToMText
( hu_multiline_t* m,
  char*   prefix,
  char*   msg );

//jff 2/26/98 message refresh widget
// draws mtext
void HUlib_drawMText(hu_multiline_t* m, align_t align);

// resets line and left margin
void HUlib_resetIText(hu_multiline_t* it);

// whether eaten
boolean HUlib_keyInIText
( hu_multiline_t* it,
  unsigned char ch );

void HUlib_drawIText(hu_multiline_t* it, align_t align);

#endif


//----------------------------------------------------------------------------
//
// $Log: hu_lib.h,v $
// Revision 1.9  1998/05/11  10:13:31  jim
// formatted/documented hu_lib
//
// Revision 1.8  1998/04/28  15:53:53  jim
// Fix message list bug in small screen mode
//
// Revision 1.7  1998/02/26  22:58:44  jim
// Added message review display to HUD
//
// Revision 1.6  1998/02/19  16:55:19  jim
// Optimized HUD and made more configurable
//
// Revision 1.5  1998/02/18  00:58:58  jim
// Addition of HUD
//
// Revision 1.4  1998/02/15  02:48:09  phares
// User-defined keys
//
// Revision 1.3  1998/01/26  19:26:52  phares
// First rev with no ^Ms
//
// Revision 1.2  1998/01/26  05:50:24  killough
// Support more lines, and tab stops, in messages
//
// Revision 1.1.1.1  1998/01/19  14:02:55  rand
// Lee's Jan 19 sources
//
//
//----------------------------------------------------------------------------

