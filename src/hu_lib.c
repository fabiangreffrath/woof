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

#include <ctype.h>
#include <stdlib.h>

#include "doomdef.h"
#include "doomkeys.h"
#include "doomstat.h"
#include "hu_lib.h"
#include "hu_stuff.h"
#include "m_swap.h"
#include "r_defs.h"
#include "r_draw.h"
#include "r_state.h"
#include "v_video.h"

// [FG] horizontal alignment

#define HU_GAPX 2
static int left_margin, right_margin;
int hud_widescreen_widgets;

void HUlib_set_margins (void)
{
  left_margin = HU_GAPX;

  if (hud_widescreen_widgets)
  {
    left_margin -= video.deltaw;
  }

  right_margin = SCREENWIDTH - left_margin;
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
  int bottom = SCREENHEIGHT - 1;

  if (scaledviewheight < SCREENHEIGHT ||
      draw_crispy_hud ||
      automap_on)
  {
    bottom -= 32; // ST_HEIGHT
  }

  align_offset[offset_topleft] = 0;
  align_offset[offset_topright] = 0;
  align_offset[offset_bottomleft] = bottom;
  align_offset[offset_bottomright] = bottom;
}

// [FG] clear line

void HUlib_clear_line (hu_line_t *const l)
{
  l->line[0] = '\0';
  l->len = 0;
  l->width = 0;
}

void HUlib_clear_cur_line (hu_multiline_t *const m)
{
  HUlib_clear_line(m->lines[m->curline]);
}

void HUlib_clear_all_lines (hu_multiline_t *const m)
{
  int i;

  for (i = 0; i < m->numlines; i++)
  {
    HUlib_clear_line(m->lines[i]);
  }
}

// [FG] add single char to line, increasing its length but not its width

static boolean add_char_to_line(hu_line_t *const t, const char ch)
{
  if (t->len == HU_MAXLINELENGTH - 1)
    return false;
  else
  {
    t->line[t->len++] = ch;
    t->line[t->len] = '\0';
    return true;
  }
}

static boolean del_char_from_line(hu_line_t* l)
{
  return l->len ? l->line[--l->len] = '\0', true : false;
}

// [FG] add printable char to line, handle Backspace and Enter (for w_chat)

boolean HUlib_add_key_to_line(hu_line_t *const l, unsigned char ch)
{
  if (ch >= ' ' && ch <= '_')
    add_char_to_line(l, (char) ch);
  else if (ch == KEY_BACKSPACE)                  // phares
    del_char_from_line(l);
  else if (ch != KEY_ENTER)                      // phares
    return false;                                // did not eat key

  return true;                                   // ate the key
}

boolean HUlib_add_key_to_cur_line(hu_multiline_t *const m, unsigned char ch)
{
  hu_line_t *const l = m->lines[m->curline];

  return HUlib_add_key_to_line(l, ch);
}

// [FG] point curline to the next line in a multiline if available

static inline void inc_cur_line (hu_multiline_t *const m)
{
  if (m->numlines > 1)
  {
    if (++m->curline >= m->numlines)
    {
      m->curline = 0;
    }
  }
}

// [FG] add string to line, increasing its (length and) width

static void add_string_to_line (hu_line_t *const l, const hu_font_t *const f, const char *s)
{
  int w = 0;
  unsigned char c;
  patch_t *const *const p = f->patches;

  if (!*s)
    return;

  while (*s)
  {
    c = toupper(*s++);

    if (c == '\x1b')
    {
      add_char_to_line(l, c);
      add_char_to_line(l, *s++);
      continue;
    }
    else if (c == '\t')
      w = (w + f->tab_width) & f->tab_mask;
    else if (c >= HU_FONTSTART && c <= HU_FONTEND + 6)
      w += SHORT(p[c - HU_FONTSTART]->width);
    else
      w += f->space_width;

    add_char_to_line(l, c);
  }

  while (*--s == ' ')
    w -= f->space_width;

  l->width += w;
}

// [FG] add string to current line, point to next line if available

void HUlib_add_strings_to_cur_line (hu_multiline_t *const m, const char *prefix, const char *s)
{
  hu_line_t *const l = m->lines[m->curline];

  HUlib_clear_line(l);

  if (prefix)
  {
    add_string_to_line(l, *m->font, prefix);
  }

  add_string_to_line(l, *m->font, s);

  inc_cur_line(m);
}

void HUlib_add_string_to_cur_line (hu_multiline_t *const m, const char *s)
{
  HUlib_add_strings_to_cur_line(m, NULL, s);
}

// [FG] horizontal and vertical alignment

static int horz_align_widget(const hu_widget_t *const w, const hu_line_t *const l, const align_t h_align)
{
  if (h_align == align_left)
  {
    return left_margin;
  }
  else if (h_align == align_right)
  {
    return right_margin - l->width;
  }
  else if (h_align == align_center)
  {
    return SCREENWIDTH/2 - l->width/2;
  }

  // [FG] align_direct
  if (hud_widescreen_widgets)
  {
    if (w->x < SCREENWIDTH/2)
    {
      return w->x - video.deltaw;
    }
    else
    {
      return w->x + video.deltaw;
    }
  }

  return w->x;
}

static int vert_align_widget(const hu_widget_t *const w, const hu_multiline_t *const m, const hu_font_t *const f, const align_t h_align, const align_t v_align)
{
  const int font_height = f->line_height;

  int y = 0;

  if (v_align == align_direct)
  {
    return w->y;
  }
  // [FG] centered and Vanilla widgets are always exclusive,
  //      i.e. they don't allow any other widget on the same line
  else if (h_align == align_center || m->on)
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

// [FG] draw a line to a given screen coordinates using the given font

static void draw_line_aligned (const hu_multiline_t *m, const hu_line_t *l, const hu_font_t *const f, int x, int y)
{
  const int x0 = x;
  int i;
  unsigned char c;
  byte *cr = m->cr;
  patch_t *const *const p = f->patches;

  // draw the new stuff
  for (i = 0; i < l->len; i++)
  {
    c = toupper(l->line[i]); //jff insure were not getting a cheap toupper conv.

#if 0
    if (c == '\n')
    {
      // [FG] TODO line breaks!
    }
    else
#endif
    if (c == '\t')    // killough 1/23/98 -- support tab stops
    {
      x = x0 + (((x - x0) + f->tab_width) & f->tab_mask);
    }
    else if (c == '\x1b')  //jff 2/17/98 escape code for color change
    {               //jff 3/26/98 changed to actual escape char
      if (++i < l->len)
      {
        if (l->line[i] >= '0' && l->line[i] <= '0'+CR_NONE)
          cr = colrngs[l->line[i]-'0'];
        else if (l->line[i] == '0'+CR_ORIG) // [FG] reset to original color
          cr = m->cr;
      }
    }
    else if (c >= HU_FONTSTART && c <= HU_FONTEND + 6)
    {
      int w = SHORT(p[c-HU_FONTSTART]->width);

      if (x+w > right_margin + HU_GAPX)
        break;

      // killough 1/18/98 -- support multiple lines:
      V_DrawPatchTranslated(x, y, p[c-HU_FONTSTART], cr);
      x += w;
    }
    else if ((x += f->space_width) >= right_margin + HU_GAPX)
      break;
  }

  // draw the cursor if requested
  // killough 1/18/98 -- support multiple lines
  if (m->drawcursor &&
      x + SHORT(p['_'-HU_FONTSTART]->width) <= right_margin + HU_GAPX &&
      leveltime & 16)
  {
    cr = m->cr; //jff 2/17/98 restore original color
    V_DrawPatchTranslated(x, y, p['_' - HU_FONTSTART], cr);
  }
}

// [FG] shortcut for single-lined wigets

static void draw_widget_single (const hu_widget_t *const w, const hu_font_t *const f)
{
  const hu_multiline_t *const m = w->multiline;
  const int h_align = w->h_align, v_align = w->v_align;

  const int cl = m->curline;
  const hu_line_t *const l = m->lines[cl];

  if (l->width || m->drawcursor)
  {
    int x, y;

    x = horz_align_widget(w, l, h_align);
    y = vert_align_widget(w, m, f, h_align, v_align);
    draw_line_aligned(m, l, f, x, y);
  }
}

// [FG] the w_messages widget is drawn bottom-up if v_align == align_top,
//      i.e. the last message is drawn first, same for all other widgets
//      if v_align == align_bottom

static void draw_widget_bottomup (const hu_widget_t *const w, const hu_font_t *const f)
{
  const hu_multiline_t *const m = w->multiline;
  const int h_align = w->h_align, v_align = w->v_align;

  const int nl = m->numlines;
  int cl = m->curline - 1;

  int i, x, y;

  for (i = 0; i < nl; i++, cl--)
  {
    const hu_line_t *l;

    if (cl < 0)
      cl = nl - 1;

    l = m->lines[cl];

    if (l->width)
    {
      x = horz_align_widget(w, l, h_align);
      y = vert_align_widget(w, m, f, h_align, v_align);
      draw_line_aligned(m, l, f, x, y);
    }
  }
}

// [FG] standard behavior, the first line is drawn first

static void draw_widget_topdown (const hu_widget_t *const w, const hu_font_t *const f)
{
  const hu_multiline_t *const m = w->multiline;
  const int h_align = w->h_align, v_align = w->v_align;

  const int nl = m->numlines;
  int cl = m->curline;

  int i, x, y;

  for (i = 0; i < nl; i++, cl++)
  {
    const hu_line_t *l;

    if (cl >= nl)
      cl = 0;

    l = m->lines[cl];

    if (l->width)
    {
      x = horz_align_widget(w, l, h_align);
      y = vert_align_widget(w, m, f, h_align, v_align);
      draw_line_aligned(m, l, f, x, y);
    }
  }
}

void HUlib_draw_widget (const hu_widget_t *const w)
{
  const hu_multiline_t *const m = w->multiline;
  const hu_font_t *const f = *m->font;

  if (m->numlines == 1)
    draw_widget_single(w, f);
  // [FG] Vanilla widget with top alignment,
  //      or Boom widget with bottom alignment
  else if ((m->on != NULL) ^ (w->v_align == align_bottom))
    draw_widget_bottomup(w, f);
  else
    draw_widget_topdown(w, f);
}

void HUlib_init_multiline(hu_multiline_t *m,
                          int nl,
                          hu_font_t **f,
                          byte *cr,
                          boolean *on,
                          void (*builder)(void))
{
  int i;

  // [FG] dynamically allocate lines array
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
    }
    HUlib_clear_line(m->lines[i]);
  }

  m->font = f;
  m->cr = cr;
  m->drawcursor = false;

  m->on = on;

  m->builder = builder;
  m->built = false;
}

void HUlib_erase_widget (const hu_widget_t *const w)
{
  const hu_multiline_t *const m = w->multiline;
  const hu_font_t *const f = *m->font;

  const int height = m->numlines * f->line_height;
  const int y = vert_align_widget(w, m, f, w->h_align, w->v_align);

  if (y > scaledviewy && y < scaledviewy + scaledviewheight - height)
  {
    R_VideoErase(0, y, scaledviewx, height);
    R_VideoErase(scaledviewx + scaledviewwidth, y, scaledviewx, height);
  }
  else
  {
    R_VideoErase(0, y, video.unscaledw, height);
  }
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
