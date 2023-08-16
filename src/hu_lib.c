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

#include "doomdef.h"
#include "doomstat.h"
#include "doomkeys.h"
#include "v_video.h"
#include "m_swap.h"
#include "hu_lib.h"
#include "hu_stuff.h"
#include "r_main.h"
#include "r_draw.h"

// boolean : whether the screen is always erased
#define noterased viewwindowx

#define TABWIDTH (16 - 1)

static int align_offset[num_aligns];

void HUlib_resetAlignOffsets (void)
{
  int bottom = ORIGHEIGHT - 1;

  if (scaledviewheight < SCREENHEIGHT ||
      (crispy_hud && hud_active > 0) ||
      automap_on)
  {
    bottom -= 32; // ST_HEIGHT
  }

  align_offset[align_topleft] = 0;
  align_offset[align_topright] = 0;
  align_offset[align_topcenter] = 0;
  align_offset[align_bottomleft] = bottom;
  align_offset[align_bottomright] = bottom;
  align_offset[align_bottomcenter] = bottom;
}

#define HU_GAPX 2
static int left_margin, right_margin;
int hud_widescreen_widgets;

void HUlib_setMargins (void)
{
  left_margin = HU_GAPX;

  if (hud_widescreen_widgets)
  {
    left_margin -= WIDESCREENDELTA;
  }

  right_margin = ORIGWIDTH - left_margin;
}

//
// not used currently
// code to initialize HUlib would go here if needed
//
void HUlib_init(void)
{
}

////////////////////////////////////////////////////////
//
// Basic text line widget
//
////////////////////////////////////////////////////////

//
// HUlib_clearTextLine()
//
// Blank the internal text line in a hu_textline_t widget
//
// Passed a hu_textline_t, returns nothing
//
void HUlib_clearTextLine(hu_textline_t* t)
{
  t->len = 0;
  t->l[0] = 0;
  t->width = 0;
}

void HUlib_clearMultiline(hu_mtext_t* t)
{
  int i;

  for (i = 0; i < t->nl; i++)
  {
    HUlib_clearTextLine(&t->l[i]);
  }
}
//
// HUlib_addCharToTextLine()
//
// Adds a character at the end of the text line in a hu_textline_t widget
//
// Passed the hu_textline_t and the char to add
// Returns false if already at length limit, true if the character added
//

boolean HUlib_addCharToTextLine(hu_textline_t *t, char ch)
{
  // killough 1/23/98 -- support multiple lines
  if (t->len == HU_MAXLINELENGTH)
    return false;
  else
    {
      t->l[t->len++] = ch;
      t->l[t->len] = 0;
      return true;
    }
}

static void HUlib_addStringToTextLine(hu_textline_t *l, char *s)
{
  int w = 0;
  unsigned char c;
  patch_t *const *const f = *l->mtext->f;

  if (!*s)
    return;

  while (*s)
  {
    c = toupper(*s++);

    if (c == '\x1b')
    {
      HUlib_addCharToTextLine(l, c);
      HUlib_addCharToTextLine(l, *s++);
      continue;
    }
    else if (c == '\t')
      w = (w + TABWIDTH) & ~TABWIDTH;
    else if (c != ' ' && c >= HU_FONTSTART && c <= HU_FONTEND + 6)
      w += SHORT(f[c - HU_FONTSTART]->width);
    else
      w += 4;

    HUlib_addCharToTextLine(l, c);
  }

  while (*--s == ' ')
    w -= 4;

  l->width = w;
  l->mtext->visible = true;
}

void HUlib_addStringToCurrentLine(hu_mtext_t *l, char *s)
{
  HUlib_addStringToTextLine(&l->l[l->cl], s);
}


//
// HUlib_delCharFromTextLine()
//
// Deletes a character at the end of the text line in a hu_textline_t widget
//
// Passed the hu_textline_t
// Returns false if already empty, true if the character deleted
//

static boolean HUlib_delCharFromTextLine(hu_textline_t* t)
{
  return t->len ? t->l[--t->len] = 0, t->needsupdate = 4, true : false;
}

//
// HUlib_drawTextLine()
//
// Draws a hu_textline_t widget
//
// Passed the hu_textline_t and flag whether to draw a cursor
// Returns nothing
//

static int HUlib_h_alignWidget(hu_textline_t *l, align_t align)
{
  switch (align)
  {
    case align_topleft:
    case align_topleft_exclusive:
    case align_bottomleft:
      return left_margin;

    case align_topright:
    case align_bottomright:
      return right_margin - l->width;

    case align_topcenter:
    case align_bottomcenter:
      return ORIGWIDTH / 2 - l->width / 2;

    case align_direct:
    default:
      return l->mtext->widget->x;
  }
}

static int HUlib_v_alignWidget(hu_textline_t *l, align_t align)
{
  patch_t *const *const f = *l->mtext->f;
  const int font_height = SHORT(f['A'-HU_FONTSTART]->height) + 1;
  int y;

  switch (align)
  {
    case align_topleft:
    case align_topleft_exclusive:
      y = align_offset[align_topleft];
      align_offset[align_topleft] += font_height;
      if (align == align_topleft_exclusive)
        align_offset[align_topright] = align_offset[align_topleft];
      break;
    case align_topright:
      y = align_offset[align_topright];
      align_offset[align_topright] += font_height;
      break;
    case align_bottomleft:
      align_offset[align_bottomleft] -= font_height;
      y = align_offset[align_bottomleft];
      break;
    case align_bottomright:
      align_offset[align_bottomright] -= font_height;
      y = align_offset[align_bottomright];
      break;
    case align_topcenter:
      align_offset[align_topcenter] = MAX(align_offset[align_topleft], align_offset[align_topright]);
      y = align_offset[align_topcenter];
      align_offset[align_topcenter] += font_height;
      align_offset[align_topleft] = align_offset[align_topright] = align_offset[align_topcenter];
      break;
    case align_bottomcenter:
      align_offset[align_bottomcenter] = MIN(align_offset[align_bottomleft], align_offset[align_bottomright]);
      align_offset[align_bottomcenter] -= font_height;
      y = align_offset[align_bottomcenter];
      align_offset[align_bottomleft] = align_offset[align_bottomright] = align_offset[align_bottomcenter];
      break;
    default:
    case align_direct:
      y = l->mtext->widget->y;
      break;
  }
  return y;
}

static void HUlib_drawTextLineAligned(hu_textline_t *l, int x, int y, boolean drawcursor)
{
  int i;
  unsigned char c;
  char *const oc = l->mtext->cr;       //jff 2/17/98 remember default color
  char *cr = oc;
  patch_t *const *const f = *l->mtext->f;

  // draw the new stuff
  for (i = 0; i < l->len; i++)
    {
      c = toupper(l->l[i]); //jff insure were not getting a cheap toupper conv.

        if (c=='\t')    // killough 1/23/98 -- support tab stops
          x = (x + TABWIDTH) & ~TABWIDTH;
        else
          if (c == '\x1b')  //jff 2/17/98 escape code for color change
            {               //jff 3/26/98 changed to actual escape char
              if (++i < l->len)
              {
                if (l->l[i] >= '0' && l->l[i] <= '0'+CR_NONE)
                  cr = colrngs[l->l[i]-'0'];
                else if (l->l[i] == '0'+CR_ORIG) // [FG] reset to original color
                  cr = oc;
              }
            }
          else
            if (c != ' ' && c >= HU_FONTSTART && c <= HU_FONTEND + 6)
              {
                int w = SHORT(f[c - HU_FONTSTART]->width);
                if (x+w > SCREENWIDTH)
                  break;
                // killough 1/18/98 -- support multiple lines:
                V_DrawPatchTranslated(x, y, FG, f[c - HU_FONTSTART], cr);
                x += w;
              }
            else
              if ((x+=4) >= SCREENWIDTH)
                break;
    }

  cr = oc; //jff 2/17/98 restore original color

  // draw the cursor if requested
  // killough 1/18/98 -- support multiple lines
  if (drawcursor &&
      x + SHORT(f['_' - HU_FONTSTART]->width) <= SCREENWIDTH)
  {
    V_DrawPatchTranslated(x, y, FG, f['_' - HU_FONTSTART], cr);
  }
}

void HUlib_drawTextLine(hu_mtext_t *l, align_t align, boolean drawcursor)
{
  int i;

  if (l->visible || (l->on && *l->on))
  {
    for (i = 0; i < l->nl; i++)
    {
      int x, y;
      int idx = l->cl - i;

      if (idx < 0)
        idx += l->nl; // handle queue of lines

      x = HUlib_h_alignWidget(&l->l[idx], align);
      y = HUlib_v_alignWidget(&l->l[idx], align);
      HUlib_drawTextLineAligned(&l->l[idx], x, y, drawcursor);
    }
  }
}

//
// HUlib_eraseTextLine()
//
// Erases a hu_textline_t widget when screen border is behind text
// Sorta called by HU_Erase and just better darn get things straight
//
// Passed the hu_textline_t
// Returns nothing
//

void HUlib_eraseTextLine(hu_textline_t* l)
{
#if 0
  // killough 11/98: trick to shadow variables
  int x = viewwindowx, y = viewwindowy; 
  int viewwindowx = x >> hires, viewwindowy = y >> hires;  // killough 11/98
  patch_t *const *const f = *l->mtext->f;

  // Only erases when NOT in automap and the screen is reduced,
  // and the text must either need updating or refreshing
  // (because of a recent change back from the automap)

  if (!automapactive && viewwindowx && l->needsupdate)
    {
      int yoffset, lh = SHORT(f['A'-HU_FONTSTART]->height) + 1;
      for (y=l->y,yoffset=y*SCREENWIDTH ; y<l->y+lh ; y++,yoffset+=SCREENWIDTH)
        if (y < viewwindowy || y >= viewwindowy + scaledviewheight) // killough 11/98:
          R_VideoErase(yoffset, SCREENWIDTH); // erase entire line
        else
          {
            // erase left border
            R_VideoErase(yoffset, viewwindowx);
            // erase right border
            R_VideoErase(yoffset + viewwindowx + scaledviewwidth, viewwindowx); // killough 11/98
          }
    }

  if (l->needsupdate)
    l->needsupdate--;
#endif
}

////////////////////////////////////////////////////////
//
// Player message widget (up to 4 lines of text)
//
////////////////////////////////////////////////////////

//
// HUlib_addLineToSText()
//
// Adds a blank line to a hu_stext_t widget
//
// Passed a hu_stext_t
// Returns nothing
//

static void HUlib_addLineToSText(hu_mtext_t* s)
{
  int i;
  if (++s->cl >= s->nl)                  // add a clear line
    s->cl = 0;
  HUlib_clearTextLine(s->l + s->cl);
  for (i=0 ; i<s->nl ; i++)              // everything needs updating
    s->l[i].needsupdate = 4;
}

//
// HUlib_addMessageToSText()
//
// Adds a message line with prefix to a hu_stext_t widget
//
// Passed a hu_stext_t, the prefix string, and a message string
// Returns nothing
//

void HUlib_addMessageToSText(hu_mtext_t *s, char *prefix, char *msg)
{
  HUlib_addLineToSText(s);
  if (prefix)
    HUlib_addStringToTextLine(&s->l[s->cl], prefix);
  HUlib_addStringToTextLine(&s->l[s->cl], msg);
}

//
// HUlib_drawSText()
//
// Displays a hu_stext_t widget
//
// Passed a hu_stext_t
// Returns nothing
//

void HUlib_drawSText(hu_mtext_t* s, align_t align)
{
#if 0
  int i;
  if (*s->on)
    for (i=0; i<s->nl; i++)
      {
	int idx = s->cl - i;
	if (idx < 0)
	  idx += s->nl; // handle queue of lines
	// need a decision made here on whether to skip the draw
	HUlib_drawTextLine(&s->l[idx], align, false); // no cursor, please
      }
#endif
}

//
// HUlib_eraseSText()
//
// Erases a hu_stext_t widget, when the screen is not fullsize
//
// Passed a hu_stext_t
// Returns nothing
//

void HUlib_eraseSText(hu_mtext_t* s)
{
/*
  int i;

  for (i=0 ; i<s->nl ; i++)
    {
      if (s->laston && !*s->on)
        s->l[i].needsupdate = 4;
      HUlib_eraseTextLine(&s->l[i]);
    }
  s->laston = *s->on;
*/
}

////////////////////////////////////////////////////////
//
// Scrolling message review widget
//
// jff added 2/26/98
//
////////////////////////////////////////////////////////

//
// HUlib_initMText()
//
// Initialize a hu_mtext_t widget. Set the position, width, number of lines,
// font, start char of the font, color range, background font, and whether
// enabled.
//
// Passed a hu_mtext_t, and the values used to initialize
// Returns nothing
//

void HUlib_initMText(hu_mtext_t *m,
                     int ml,
                     patch_t ***f,
                     char *cr,
                     boolean *on,
                     void (*builder)(void))
{
  int i;

/*
  if (m->ml != ml)
  {
    for (i = 0; i < m->ml; i++)
    {
      free(m->l[i]);
      m->l[i] = NULL;
    }
  }
*/

  m->ml = ml;
  m->nl = 0;
  m->cl = -1; //jff 4/28/98 prepare for pre-increment

  for (i = 0; i < m->ml; i++)
  {
//    if (m->l[i] == NULL)
    {
/*
      m->l[i] = malloc(sizeof(hu_textline_t));
*/
      HUlib_clearTextLine(&m->l[i]);
    }
    m->l[i].mtext = m;
  }

  m->f = f;
  m->cr = cr;

  m->on = on;
  m->laston = true;

  m->builder = builder;
  m->visible = false;
}

//
// HUlib_addLineToMText()
//
// Adds a blank line to a hu_mtext_t widget
//
// Passed a hu_mtext_t
// Returns nothing
//

static void HUlib_addLineToMText(hu_mtext_t *m)
{
  // add a clear line
  if (++m->cl >= m->nl)
    m->cl = 0;

  HUlib_clearTextLine(&m->l[m->cl]);

  if (m->nl < m->ml)
    m->nl++;
}

//
// HUlib_addMessageToMText()
//
// Adds a message line with prefix to a hu_mtext_t widget
//
// Passed a hu_mtext_t, the prefix string, and a message string
// Returns nothing
//

void HUlib_addMessageToMText(hu_mtext_t *m, char *prefix, char *msg)
{
  HUlib_addLineToMText(m);

  if (prefix)
    HUlib_addStringToTextLine(&m->l[m->cl], prefix);

  HUlib_addStringToTextLine(&m->l[m->cl], msg);
}

//
// HUlib_drawMText()
//
// Displays a hu_mtext_t widget
//
// Passed a hu_mtext_t
// Returns nothing
//
// killough 11/98: Simplified, allowed text to scroll in either direction

void HUlib_drawMText(hu_mtext_t* m, align_t align)
{
/*
  int i;

  if (!*m->on)
    return; // if not on, don't draw

  for (i=0 ; i<m->nl ; i++)
    {
      int idx = m->cl - i;

      if (idx < 0)
	idx += m->nl; // handle queue of lines

      m->l[idx].y = i * HU_REFRESHSPACING;

      HUlib_drawTextLine(&m->l[idx], align, false); // no cursor, please
    }
*/
}

//
// HUlib_eraseMText()
//
// Erases a hu_mtext_t widget, when the screen is not fullsize
//
// Passed a hu_mtext_t
// Returns nothing
//

void HUlib_eraseMText(hu_mtext_t *m)
{
  int i;

  for (i=0 ; i< m->nl ; i++)
    {
      m->l[i].needsupdate = 4;
      HUlib_eraseTextLine(&m->l[i]);
    }
}

////////////////////////////////////////////////////////
//
// Interactive text entry widget
//
////////////////////////////////////////////////////////

// The following deletion routines adhere to the left margin restriction

//
// HUlib_delCharFromIText()
//
// Deletes a character at the end of the text line in a hu_itext_t widget
//
// Passed the hu_itext_t
// Returns nothing
//

static void HUlib_delCharFromIText(hu_mtext_t *it)
{
  if (it->l[0].len > 0)
    HUlib_delCharFromTextLine(&it->l[0]);
}

//
// HUlib_resetIText()
//
// Deletes all characters from a hu_itext_t widget
// Resets left margin as well
//
// Passed the hu_itext_t
// Returns nothing
//

void HUlib_resetIText(hu_mtext_t *it)
{
  HUlib_clearTextLine(&it->l[0]);
}

//
// HUlib_keyInIText()
//
// Wrapper function for handling general keyed input.
//
// Passed the hu_itext_t and the char input
// Returns true if it ate the key
//

boolean HUlib_keyInIText(hu_mtext_t *it, unsigned char ch)
{
  if (ch >= ' ' && ch <= '_')
    HUlib_addCharToTextLine(&it->l[0], (char) ch);
  else
    if (ch == KEY_BACKSPACE)                  // phares
      HUlib_delCharFromIText(it);
  else
    if (ch != KEY_ENTER)                      // phares
      return false;                            // did not eat key
  return true;                                 // ate the key
}

//
// HUlib_drawIText()
//
// Displays a hu_itext_t widget
//
// Passed the hu_itext_t
// Returns nothing
//

void HUlib_drawIText(hu_mtext_t *it, align_t align)
{
/*
  hu_textline_t *l = &it->l[0];
  if ((l->visible = *it->on))
    HUlib_drawTextLine(l, align, true); // draw the line w/ cursor
*/
}

//
// HUlib_eraseIText()
//
// Erases a hu_itext_t widget when the screen is not fullsize
//
// Passed the hu_itext_t
// Returns nothing
//

void HUlib_eraseIText(hu_mtext_t* it)
{
  if (it->laston && !*it->on)
    it->l[0].needsupdate = 4;
  HUlib_eraseTextLine(&it->l[0]);
  it->laston = *it->on;
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
