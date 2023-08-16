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
// DESCRIPTION:  heads-up text and input code
//
//-----------------------------------------------------------------------------

#include "doomstat.h"
#include "doomkeys.h"
#include "m_swap.h"
#include "hu_lib.h"
#include "hu_stuff.h"

// [FG] horizontal alignment

#define TABWIDTH (16 - 1)

#define HU_GAPX 2
static int left_margin, right_margin;
int hud_widescreen_widgets;

void HUlib_set_margins (void)
{
  left_margin = HU_GAPX;

  if (hud_widescreen_widgets)
  {
    left_margin -= WIDESCREENDELTA;
  }

  right_margin = ORIGWIDTH - left_margin;
}

// [FG] vertical alignment

typedef enum {
    offset_topleft,
    offset_topright,

    offset_bottomleft,
    offset_bottomright,

    num_offsets,
} offset_t;

static int align_offset[num_offsets];

void HUlib_reset_align_offsets (void)
{
  int bottom = ORIGHEIGHT - 1;

  if (scaledviewheight < SCREENHEIGHT ||
      (crispy_hud && hud_active > 0) ||
      automap_on)
  {
    bottom -= 32; // ST_HEIGHT
  }

  align_offset[offset_topleft] = 0;
  align_offset[offset_topright] = 0;
  align_offset[offset_bottomleft] = bottom;
  align_offset[offset_bottomright] = bottom;
}

////////////////////////////////////////////////////////
//
// Basic text line widget
//
////////////////////////////////////////////////////////

//
// HUlib_clear_line()
//
// Blank the internal text line in a hu_line_t widget
//
// Passed a hu_line_t, returns nothing
//
void HUlib_clear_line(hu_line_t* t)
{
  t->line[0] = '\0';
  t->len = 0;
  t->width = 0;
}

void HUlib_clear_lines(hu_multiline_t* t)
{
  int i;

  for (i = 0; i < t->numlines; i++)
  {
    HUlib_clear_line(t->lines[i]);
  }
}
//
// HUlib_add_char_to_line()
//
// Adds a character at the end of the text line in a hu_line_t widget
//
// Passed the hu_line_t and the char to add
// Returns false if already at length limit, true if the character added
//

boolean HUlib_add_char_to_line(hu_line_t *t, char ch)
{
  if (t->len == HU_MAXLINELENGTH)
    return false;
  else
    {
      t->line[t->len++] = ch;
      t->line[t->len] = '\0';
      return true;
    }
}

static void HUlib_add_string_to_line(hu_line_t *l, char *s)
{
  int w = 0;
  unsigned char c;
  patch_t *const *const font = *l->multiline->font;

  if (!*s)
    return;

  while (*s)
  {
    c = toupper(*s++);

    if (c == '\x1b')
    {
      HUlib_add_char_to_line(l, c);
      HUlib_add_char_to_line(l, *s++);
      continue;
    }
    else if (c == '\t')
      w = (w + TABWIDTH) & ~TABWIDTH;
    else if (c != ' ' && c >= HU_FONTSTART && c <= HU_FONTEND + 6)
      w += SHORT(font[c - HU_FONTSTART]->width);
    else
      w += 4;

    HUlib_add_char_to_line(l, c);
  }

  while (*--s == ' ')
    w -= 4;

  l->width = w;
  l->multiline->built = true;
}

void HUlib_add_string_to_current_line(hu_multiline_t *l, char *s)
{
  HUlib_add_string_to_line(l->lines[l->curline], s);
}


//
// HUlib_del_char_from_line()
//
// Deletes a character at the end of the text line in a hu_line_t widget
//
// Passed the hu_line_t
// Returns false if already empty, true if the character deleted
//

static boolean HUlib_del_char_from_line(hu_line_t* t)
{
  return t->len ? t->line[--t->len] = '\0', true : false;
}

//
// HUlib_drawTextLine()
//
// Draws a hu_line_t widget
//
// Passed the hu_line_t and flag whether to draw a cursor
// Returns nothing
//

static int horz_align_widget(const hu_widget_t *widget, const hu_multiline_t *multiline, const hu_line_t *line, align_t h_align)
{
  if (h_align == align_left)
  {
    return left_margin;
  }
  else if (h_align == align_right)
  {
    return right_margin - line->width;
  }
  else if (h_align == align_center)
  {
    return ORIGWIDTH/2 - line->width/2;
  }

  return widget->x;
}

static int vert_align_widget(const hu_widget_t *widget, const hu_multiline_t *multiline, const hu_line_t *line, align_t h_align, align_t v_align)
{
  patch_t *const *const font = *multiline->font;
  const int font_height = SHORT(font['A'-HU_FONTSTART]->height) + 1;

  int y = 0;

  if (v_align == align_direct)
  {
    return widget->y;
  }
  // [FG] centered and Vanilla widgets are always exclusive
  else if (h_align == align_center || multiline->on)
  {
    if (v_align == align_top)
    {
      y = MAX(align_offset[offset_topleft],
              align_offset[offset_topright]);

      align_offset[offset_topleft] =
      align_offset[offset_topright] = y + font_height;
    }
    else if (v_align == align_bottom)
    {
      y = MIN(align_offset[offset_bottomleft],
              align_offset[offset_bottomright]) - font_height;

      align_offset[offset_bottomleft] =
      align_offset[offset_bottomright] = y;
    }
  }
  else if (v_align == align_top)
  {
    if (h_align == align_left)
    {
      y = align_offset[offset_topleft];
      align_offset[offset_topleft] += font_height;
    }
    else if (h_align == align_right) 
    {
      y = align_offset[offset_topright];
      align_offset[offset_topright] += font_height;
    }
  }
  else if (v_align == align_bottom)
  {
    if (h_align == align_left)
    {
      align_offset[offset_bottomleft] -= font_height;
      y = align_offset[offset_bottomleft];
    }
    else if (h_align == align_right) 
    {
      align_offset[offset_bottomright] -= font_height;
      y = align_offset[offset_bottomright];
    }
  }

  return y;
}

static void HUlib_draw_line_aligned(const hu_multiline_t *multiline, const hu_line_t *l, int x, int y)
{
  int i;
  unsigned char c;
  char *cr = multiline->cr;
  patch_t *const *const font = *multiline->font;

  // draw the new stuff
  for (i = 0; i < l->len; i++)
    {
      c = toupper(l->line[i]); //jff insure were not getting a cheap toupper conv.

        if (c=='\t')    // killough 1/23/98 -- support tab stops
          x = (x + TABWIDTH) & ~TABWIDTH;
        else
          if (c == '\x1b')  //jff 2/17/98 escape code for color change
            {               //jff 3/26/98 changed to actual escape char
              if (++i < l->len)
              {
                if (l->line[i] >= '0' && l->line[i] <= '0'+CR_NONE)
                  cr = colrngs[l->line[i]-'0'];
                else if (l->line[i] == '0'+CR_ORIG) // [FG] reset to original color
                  cr = multiline->cr;
              }
            }
          else
            if (c != ' ' && c >= HU_FONTSTART && c <= HU_FONTEND + 6)
              {
                int w = SHORT(font[c - HU_FONTSTART]->width);
                if (x+w > SCREENWIDTH)
                  break;
                // killough 1/18/98 -- support multiple lines:
                V_DrawPatchTranslated(x, y, FG, font[c - HU_FONTSTART], cr);
                x += w;
              }
            else
              if ((x+=4) >= SCREENWIDTH)
                break;
    }

  // draw the cursor if requested
  // killough 1/18/98 -- support multiple lines
  if (multiline->drawcursor && (leveltime & 16))
  {
    cr = multiline->cr; //jff 2/17/98 restore original color
    V_DrawPatchTranslated(x, y, FG, font['_' - HU_FONTSTART], cr);
  }
}

void HUlib_draw_widget(hu_widget_t *w)
{
  const hu_multiline_t *multiline = w->multiline;
  const int h_align = w->h_align, v_align = w->v_align;

  const int nl = multiline->numlines;
  int cl = multiline->curline;

  int i, x, y;

  for (i = 0; i < nl; i++, cl++)
  {
    const hu_line_t *line;

    if (cl >= nl)
      cl = 0;

    line = multiline->lines[cl];

    if (line->width || multiline->drawcursor)
    {
      x = horz_align_widget(w, multiline, line, h_align);
      y = vert_align_widget(w, multiline, line, h_align, v_align);
      HUlib_draw_line_aligned(multiline, line, x, y);
    }
  }
}

////////////////////////////////////////////////////////
//
// Player message widget (up to 4 lines of text)
//
////////////////////////////////////////////////////////

//
// HUlib_clear_next_line()
//
// Adds a blank line to a hu_stext_t widget
//
// Passed a hu_stext_t
// Returns nothing
//

static void HUlib_clear_next_line(hu_multiline_t* s)
{
  if (++s->curline >= s->numlines)                  // add a clear line
    s->curline = 0;
  HUlib_clear_line(s->lines[s->curline]);
}

//
// HUlib_add_strings_to_next_line()
//
// Adds a message line with prefix to a hu_stext_t widget
//
// Passed a hu_stext_t, the prefix string, and a message string
// Returns nothing
//

void HUlib_add_strings_to_next_line(hu_multiline_t *s, char *prefix, char *msg)
{
  HUlib_clear_next_line(s);
  if (prefix)
    HUlib_add_string_to_line(s->lines[s->curline], prefix);
  HUlib_add_string_to_line(s->lines[s->curline], msg);
}

////////////////////////////////////////////////////////
//
// Scrolling message review widget
//
// jff added 2/26/98
//
////////////////////////////////////////////////////////

//
// HUlib_init_multiline()
//
// Initialize a hu_multiline_t widget. Set the position, width, number of lines,
// font, start char of the font, color range, background font, and whether
// enabled.
//
// Passed a hu_multiline_t, and the values used to initialize
// Returns nothing
//

void HUlib_init_multiline(hu_multiline_t *m,
                     int nl,
                     patch_t ***font,
                     char *cr,
                     boolean *on,
                     void (*builder)(void))
{
  int i;

  if (m->numlines != nl)
  {
    for (i = 0; i < m->numlines; i++)
    {
      free(m->lines[i]);
      m->lines[i] = NULL;
    }
  }

  m->numlines = nl;
  m->curline = 0;

  for (i = 0; i < m->numlines; i++)
  {
    if (m->lines[i] == NULL)
    {
      m->lines[i] = malloc(sizeof(hu_line_t));
      HUlib_clear_line(m->lines[i]);
    }
    m->lines[i]->multiline = m;
  }

  m->font = font;
  m->cr = cr;
  m->drawcursor = false;

  m->on = on;
  m->laston = true;

  m->builder = builder;
  m->built = false;
}

//
// HUlib_add_strings_to_current_line()
//
// Adds a message line with prefix to a hu_multiline_t widget
//
// Passed a hu_multiline_t, the prefix string, and a message string
// Returns nothing
//

void HUlib_add_strings_to_current_line(hu_multiline_t *m, char *prefix, char *msg)
{
  HUlib_clear_current_line(m);

  if (prefix)
    HUlib_add_string_to_line(m->lines[m->curline], prefix);

  HUlib_add_string_to_line(m->lines[m->curline], msg);
}

////////////////////////////////////////////////////////
//
// Interactive text entry widget
//
////////////////////////////////////////////////////////

// The following deletion routines adhere to the left margin restriction

//
// HUlib_clear_current_line()
//
// Deletes all characters from a hu_itext_t widget
// Resets left margin as well
//
// Passed the hu_itext_t
// Returns nothing
//

void HUlib_clear_current_line(hu_multiline_t *it)
{
  HUlib_clear_line(it->lines[it->curline]);
}

//
// HUlib_keyInIText()
//
// Wrapper function for handling general keyed input.
//
// Passed the hu_itext_t and the char input
// Returns true if it ate the key
//

boolean HUlib_key_in_line(hu_line_t *it, unsigned char ch)
{
  if (ch >= ' ' && ch <= '_')
    HUlib_add_char_to_line(it, (char) ch);
  else
    if (ch == KEY_BACKSPACE)                  // phares
      HUlib_del_char_from_line(it);
  else
    if (ch != KEY_ENTER)                      // phares
      return false;                            // did not eat key
  return true;                                 // ate the key
}

boolean HUlib_key_in_current_line(hu_multiline_t *it, unsigned char ch)
{
  return HUlib_key_in_line(it->lines[it->curline], ch);
}

//----------------------------------------------------------------------------
//
// $Log: hu_lib.c,v $
// Revision 1.13  1998/05/11  10:13:26  jim
// formatted/documented hu_lib
//
// Revision 1.12  1998/05/03  22:24:13  killough
// Provide minimal headers at top; nothing else
//
// Revision 1.11  1998/04/29  09:24:33  jim
// Fix compiler warning
//
// Revision 1.10  1998/04/28  15:53:46  jim
// Fix message list bug in small screen mode
//
// Revision 1.9  1998/03/27  21:25:41  jim
// Commented change of \ to ESC
//
// Revision 1.8  1998/03/26  20:06:24  jim
// Fixed escape confusion in HU text drawer
//
// Revision 1.7  1998/02/26  22:58:33  jim
// Added message review display to HUD
//
// Revision 1.6  1998/02/19  16:55:15  jim
// Optimized HUD and made more configurable
//
// Revision 1.5  1998/02/18  00:59:01  jim
// Addition of HUD
//
// Revision 1.4  1998/02/15  02:47:44  phares
// User-defined keys
//
// Revision 1.3  1998/01/26  19:23:20  phares
// First rev with no ^Ms
//
// Revision 1.2  1998/01/26  05:50:22  killough
// Support more lines, and tab stops, in messages
//
// Revision 1.1.1.1  1998/01/19  14:02:55  rand
// Lee's Jan 19 sources
//
//----------------------------------------------------------------------------
