// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// $Id: hu_lib.c,v 1.13 1998/05/11 10:13:26 jim Exp $
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
// DESCRIPTION:  heads-up text and input code
//
//-----------------------------------------------------------------------------

static const char
rcsid[] = "$Id: hu_lib.c,v 1.13 1998/05/11 10:13:26 jim Exp $";

#include "doomdef.h"
#include "doomstat.h"
#include "v_video.h"
#include "m_swap.h"
#include "hu_lib.h"
#include "hu_stuff.h"
#include "r_main.h"
#include "r_draw.h"

// boolean : whether the screen is always erased
#define noterased viewwindowx

extern boolean  automapactive;  // in AM_map.c

extern int  key_backspace;                                          // phares
extern int  key_enter;                                              // phares

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
  t->linelen = 0;        // killough 1/23/98: support multiple lines
  t->len = 0;
  t->l[0] = 0;
  t->needsupdate = true;
}

//
// HUlib_initTextLine()
//
// Initialize a hu_textline_t widget. Set the position, font, start char
// of the font, and color range to be used.
//
// Passed a hu_textline_t, and the values used to initialize
// Returns nothing
//

void HUlib_initTextLine(hu_textline_t *t, int x, int y, patch_t **f, int sc,
                        char *cr) //jff 2/16/98 add color range parameter
{
  t->x = x;
  t->y = y;
  t->f = f;
  t->sc = sc;
  t->cr = cr;
  HUlib_clearTextLine(t);
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
  if (t->linelen == HU_MAXLINELENGTH)
    return false;
  else
    {
      t->linelen++;
      if (ch == '\n')
        t->linelen=0;
      t->l[t->len++] = ch;
      t->l[t->len] = 0;
      t->needsupdate = 4;
      return true;
    }
}

//
// HUlib_delCharFromTextLine()
//
// Deletes a character at the end of the text line in a hu_textline_t widget
//
// Passed the hu_textline_t
// Returns false if already empty, true if the character deleted
//

boolean HUlib_delCharFromTextLine(hu_textline_t* t)
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

void HUlib_drawTextLine(hu_textline_t *l, boolean drawcursor)
{
  int i, x = l->x, y = l->y;  // killough 1/18/98 -- support multiple lines
  unsigned char c;
  char *oc = l->cr;       //jff 2/17/98 remember default color

  // draw the new stuff
  for (i = 0; i < l->len; i++)
    {
      c = toupper(l->l[i]); //jff insure were not getting a cheap toupper conv.

      if (c=='\n')         // killough 1/18/98 -- support multiple lines
        x = 0, y += 8;
      else
        if (c=='\t')    // killough 1/23/98 -- support tab stops
          x = x - x%80 + 80;
        else
          if (c == '\x1b')  //jff 2/17/98 escape code for color change
            {               //jff 3/26/98 changed to actual escape char
              if (++i < l->len && l->l[i] >= '0' && l->l[i] <= '9')
                l->cr = colrngs[l->l[i]-'0'];
            }
          else
            if (c != ' ' && c >= l->sc && c <= 127)
              {
                int w = SHORT(l->f[c - l->sc]->width);
                if (x+w > SCREENWIDTH)
                  break;
                // killough 1/18/98 -- support multiple lines:
                V_DrawPatchTranslated(x, y, FG, l->f[c - l->sc], l->cr, 0);
                x += w;
              }
            else
              if ((x+=4) >= SCREENWIDTH)
                break;
    }

  l->cr = oc; //jff 2/17/98 restore original color

  // draw the cursor if requested
  // killough 1/18/98 -- support multiple lines
  if (drawcursor && x + SHORT(l->f['_' - l->sc]->width) <= SCREENWIDTH)
    V_DrawPatchDirect(x, y, FG, l->f['_' - l->sc]);
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
  // killough 11/98: trick to shadow variables
  int x = viewwindowx, y = viewwindowy; 
  int viewwindowx = x >> hires, viewwindowy = y >> hires;  // killough 11/98

  // Only erases when NOT in automap and the screen is reduced,
  // and the text must either need updating or refreshing
  // (because of a recent change back from the automap)

  if (!automapactive && viewwindowx && l->needsupdate)
    {
      int yoffset, lh = SHORT(l->f[0]->height) + 1;
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
}

////////////////////////////////////////////////////////
//
// Player message widget (up to 4 lines of text)
//
////////////////////////////////////////////////////////

//
// HUlib_initSText()
//
// Initialize a hu_stext_t widget. Set the position, number of lines, font,
// start char of the font, and color range to be used, and whether enabled.
//
// Passed a hu_stext_t, and the values used to initialize
// Returns nothing
//
//jff 2/16/98 add color range parameter

void HUlib_initSText(hu_stext_t *s, int x, int y, int h, patch_t **font,
                     int startchar, char *cr, boolean *on)
{
  int i;

  s->h = h;
  s->on = on;
  s->laston = true;
  s->cl = 0;
  for (i=0;i<h;i++)
    HUlib_initTextLine(s->l+i, x, y - i*(SHORT(font[0]->height)+1),
                       font, startchar, cr);
}

//
// HUlib_addLineToSText()
//
// Adds a blank line to a hu_stext_t widget
//
// Passed a hu_stext_t
// Returns nothing
//

void HUlib_addLineToSText(hu_stext_t* s)
{
  int i;
  if (++s->cl >= s->h)                  // add a clear line
    s->cl = 0;
  HUlib_clearTextLine(s->l + s->cl);
  for (i=0 ; i<s->h ; i++)              // everything needs updating
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

void HUlib_addMessageToSText(hu_stext_t *s, char *prefix, char *msg)
{
  HUlib_addLineToSText(s);
  if (prefix)
    while (*prefix)
      HUlib_addCharToTextLine(&s->l[s->cl], *prefix++);
  while (*msg)
    HUlib_addCharToTextLine(&s->l[s->cl], *msg++);
}

//
// HUlib_drawSText()
//
// Displays a hu_stext_t widget
//
// Passed a hu_stext_t
// Returns nothing
//

void HUlib_drawSText(hu_stext_t* s)
{
  int i;
  if (*s->on)
    for (i=0; i<s->h; i++)
      {
	int idx = s->cl - i;
	if (idx < 0)
	  idx += s->h; // handle queue of lines
	// need a decision made here on whether to skip the draw
	HUlib_drawTextLine(&s->l[idx], false); // no cursor, please
      }
}

//
// HUlib_eraseSText()
//
// Erases a hu_stext_t widget, when the screen is not fullsize
//
// Passed a hu_stext_t
// Returns nothing
//

void HUlib_eraseSText(hu_stext_t* s)
{
  int i;

  for (i=0 ; i<s->h ; i++)
    {
      if (s->laston && !*s->on)
        s->l[i].needsupdate = 4;
      HUlib_eraseTextLine(&s->l[i]);
    }
  s->laston = *s->on;
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

void HUlib_initMText(hu_mtext_t *m, int x, int y, int w, int h, patch_t **font,
                     int startchar, char *cr, patch_t** bgfont, boolean *on)
{
  int i;

  m->nl = 0;
  m->nr = 0;
  m->cl = -1; //jff 4/28/98 prepare for pre-increment
  m->x = x;
  m->y = y;
  m->w = w;
  m->h = h;
  m->bg = bgfont;
  m->on = on;

  // killough 11/98: simplify

  if (hud_list_bgon)
    y += HU_REFRESHSPACING;

  y += HU_REFRESHSPACING * hud_msg_lines;
  for (i=0; i<hud_msg_lines; i++, y -= HU_REFRESHSPACING)
    HUlib_initTextLine(&m->l[i], x, y, font, startchar, cr);
}

//
// HUlib_addLineToMText()
//
// Adds a blank line to a hu_mtext_t widget
//
// Passed a hu_mtext_t
// Returns nothing
//

void HUlib_addLineToMText(hu_mtext_t *m)
{
  // add a clear line
  if (++m->cl >= hud_msg_lines)
    m->cl = 0;

  HUlib_clearTextLine(&m->l[m->cl]);

  if (m->nl < hud_msg_lines)
    m->nl++;

  // needs updating
  m->l[m->cl].needsupdate = 4;
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
    while (*prefix)
      HUlib_addCharToTextLine(&m->l[m->cl], *prefix++);

  while (*msg)
    HUlib_addCharToTextLine(&m->l[m->cl], *msg++);
}

//
// HUlib_drawMBg()
//
// Draws a background box which the message display review widget can
// display over
//
// Passed position, width, height, and the background patches
// Returns nothing
//

void HUlib_drawMBg(int x, int y, int w, int h, patch_t **bgp)
{
  int xs = bgp[0]->width;
  int ys = bgp[0]->height;
  int i,j;

  // top rows
  V_DrawPatchDirect(x, y, FG, bgp[0]);    // ul
  for (j = x+xs; j < x+w-xs; j += xs)     // uc
    V_DrawPatchDirect(j, y, FG, bgp[1]);
  V_DrawPatchDirect(j, y, FG, bgp[2]);    // ur

  // middle rows
  for (i=y+ys;i<y+h-ys;i+=ys)
    {
      V_DrawPatchDirect(x, i, FG, bgp[3]);    // cl
      for (j = x+xs; j < x+w-xs; j += xs)     // cc
        V_DrawPatchDirect(j, i, FG, bgp[4]);
      V_DrawPatchDirect(j, i, FG, bgp[5]);    // cr
    }

  // bottom row
  V_DrawPatchDirect(x, i, FG, bgp[6]);    // ll
  for (j = x+xs; j < x+w-xs; j += xs)     // lc
    V_DrawPatchDirect(j, i, FG, bgp[7]);
  V_DrawPatchDirect(j, i, FG, bgp[8]);    // lr
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

void HUlib_drawMText(hu_mtext_t* m)
{
  int i;

  if (!*m->on)
    return; // if not on, don't draw

  // draw everything
  if (hud_list_bgon)
    HUlib_drawMBg(m->x, m->y, m->w, m->h, m->bg);

  for (i=0 ; i<m->nl ; i++)
    {
      int idx = m->cl - i;

      if (idx < 0)
	idx += m->nl; // handle queue of lines

      m->l[idx].x = m->x;       // killough 11/98: optional scroll up/down:
      m->l[idx].y = m->y+(hud_msg_scrollup ? m->nl-1-i : i)*HU_REFRESHSPACING;

      if (hud_list_bgon)
	m->l[idx].x += 4, m->l[idx].y += HU_REFRESHSPACING;

      HUlib_drawTextLine(&m->l[idx], false); // no cursor, please
    }
}

//
// HUlib_eraseMBg()
//
// Erases background behind hu_mtext_t widget, when the screen is not fullsize
//
// Passed a hu_mtext_t
// Returns nothing
//

static void HUlib_eraseMBg(hu_mtext_t *m)
{
  // killough 11/98: trick to shadow variables
  int x = viewwindowx, y = viewwindowy; 
  int viewwindowx = x >> hires, viewwindowy = y >> hires;  // killough 11/98

  // Only erases when NOT in automap and the screen is reduced,
  // and the text must either need updating or refreshing
  // (because of a recent change back from the automap)

  if (!automapactive && viewwindowx)
    {
      int yoffset, lh = SHORT(m->l[0].f[0]->height) + 1;
      for (y=m->y, yoffset=y*SCREENWIDTH;
           y < m->y+lh*(hud_msg_lines+2);
           y++, yoffset+=SCREENWIDTH)
        if (y < viewwindowy || y >= viewwindowy + scaledviewheight) // killough 11/98
          R_VideoErase(yoffset, SCREENWIDTH); // erase entire line
        else
          {
            // erase left border
            R_VideoErase(yoffset, viewwindowx);
            // erase right border
            R_VideoErase(yoffset + viewwindowx + scaledviewwidth, viewwindowx); // killough 11/98
          }
    }
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

  if (hud_list_bgon)
    HUlib_eraseMBg(m);

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

//
// HUlib_initIText()
//
// Initialize a hu_itext_t widget. Set the position, font,
// start char of the font, color range, and whether enabled.
//
// Passed a hu_itext_t, and the values used to initialize
// Returns nothing
//
//jff 2/16/98 add color range parameter

void HUlib_initIText(hu_itext_t *it, int x, int y, patch_t **font,
                     int startchar, char *cr, boolean *on)
{
  it->lm = 0; // default left margin is start of text
  it->on = on;
  it->laston = true;
  HUlib_initTextLine(&it->l, x, y, font, startchar, cr);
}

// The following deletion routines adhere to the left margin restriction

//
// HUlib_delCharFromIText()
//
// Deletes a character at the end of the text line in a hu_itext_t widget
//
// Passed the hu_itext_t
// Returns nothing
//

void HUlib_delCharFromIText(hu_itext_t *it)
{
  if (it->l.len != it->lm)
    HUlib_delCharFromTextLine(&it->l);
}

//
// HUlib_eraseLineFromIText()
//
// Deletes all characters from a hu_itext_t widget
//
// Passed the hu_itext_t
// Returns nothing
//

void HUlib_eraseLineFromIText(hu_itext_t *it)
{
  while (it->lm != it->l.len)
    HUlib_delCharFromTextLine(&it->l);
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

void HUlib_resetIText(hu_itext_t *it)
{
  it->lm = 0;
  HUlib_clearTextLine(&it->l);
}

//
// HUlib_addPrefixToIText()
//
// Adds a prefix string passed to a hu_itext_t widget
// Sets left margin to length of string added
//
// Passed the hu_itext_t and the prefix string
// Returns nothing
//

void HUlib_addPrefixToIText(hu_itext_t *it, char *str)
{
  while (*str)
    HUlib_addCharToTextLine(&it->l, *str++);
  it->lm = it->l.len;
}

//
// HUlib_keyInIText()
//
// Wrapper function for handling general keyed input.
//
// Passed the hu_itext_t and the char input
// Returns true if it ate the key
//

boolean HUlib_keyInIText(hu_itext_t *it, unsigned char ch)
{
  if (ch >= ' ' && ch <= '_')
    HUlib_addCharToTextLine(&it->l, (char) ch);
  else
    if (ch == key_backspace)                   // phares
      HUlib_delCharFromIText(it);
  else
    if (ch != key_enter)                       // phares
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

void HUlib_drawIText(hu_itext_t *it)
{
  hu_textline_t *l = &it->l;
  if (*it->on)
    HUlib_drawTextLine(l, true); // draw the line w/ cursor
}

//
// HUlib_eraseIText()
//
// Erases a hu_itext_t widget when the screen is not fullsize
//
// Passed the hu_itext_t
// Returns nothing
//

void HUlib_eraseIText(hu_itext_t* it)
{
  if (it->laston && !*it->on)
    it->l.needsupdate = 4;
  HUlib_eraseTextLine(&it->l);
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
