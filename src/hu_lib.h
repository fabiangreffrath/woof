//
//  Copyright (C) 1999 by
//  id Software, Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman
//  Copyright (C) 2023 Fabian Greffrath
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

#include "doomtype.h"

struct patch_s;

// [FG] font stuff

#define HU_FONTSTART    '!'     /* the first font characters */
#define HU_FONTEND      (0x7f) /*jff 2/16/98 '_' the last font characters */

// Calculate # of glyphs in font.
#define HU_FONTSIZE     (HU_FONTEND - HU_FONTSTART + 1)

typedef struct
{
  struct patch_s *patches[HU_FONTSIZE+6];

  int line_height;

  const int space_width;

  const int tab_width;
  const int tab_mask;
} hu_font_t;

extern struct patch_s **hu_font;

// [FG] widget stuff

#define CR_ORIG (-1) // [FG] reset to original color

#define HU_MAXLINELENGTH 120

//jff 2/26/98 maximum number of messages allowed in refresh list
#define HU_MAXMESSAGES 8

typedef enum
{
  // [FG] h_align / v_align
  align_direct,

  // [FG] h_align
  align_left,
  align_right,
  align_center,

  // [FG] v_align
  align_top,
  align_bottom,
  align_secret,

  num_aligns,
} align_t;

// [FG] a single line of information

typedef struct
{
  char line[HU_MAXLINELENGTH];

  // [FG] length in chars
  int len;

  // [FG] width in pixels
  int width;

} hu_line_t;

// [FG] an array of lines with common properties

typedef struct hu_multiline_s
{
  hu_line_t *lines[HU_MAXMESSAGES]; // text lines to draw
  int numlines; // number of lines
  int curline; // current line number

  hu_font_t **font; // font
  byte *cr; //jff 2/16/52 output color range
  boolean drawcursor;

  // pointer to boolean stating whether to update window
  boolean *on;

  void (*builder)(void);
  boolean built;

  boolean exclusive;
  boolean bottomup;

} hu_multiline_t;

// [FG] configured alignment and coordinates for multilines

typedef struct hu_widget_s
{
  hu_multiline_t *multiline;

  align_t h_align, v_align;

  // [FG] align_direct
  int x, y;

  // [FG] back up for centered messages
  align_t h_align_orig;

} hu_widget_t;

void HUlib_set_margins (void);
void HUlib_reset_align_offsets (void);

void HUlib_clear_line (hu_line_t *const l);
void HUlib_clear_cur_line (hu_multiline_t *const m);
void HUlib_clear_all_lines (hu_multiline_t *const m);

void HUlib_add_string_to_cur_line (hu_multiline_t *const m, const char *s);
void HUlib_add_strings_to_cur_line (hu_multiline_t *const m, const char *prefix, const char *s);

void HUlib_draw_widget (const hu_widget_t *const w);

void HUlib_init_multiline (hu_multiline_t *const m, int nl, hu_font_t **f, byte *cr, boolean *on, void (*builder)(void));

boolean HUlib_add_key_to_line (hu_line_t *const l, unsigned char ch);
boolean HUlib_add_key_to_cur_line (hu_multiline_t *const m, unsigned char ch);

void HUlib_erase_widget (const hu_widget_t *const w);

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

