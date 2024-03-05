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
//      Mission begin melt/wipe screen special effect.
//
//-----------------------------------------------------------------------------

#include <string.h>

#include "doomtype.h"
#include "f_wipe.h"
#include "i_video.h"
#include "m_random.h"
#include "v_flextran.h"
#include "v_video.h"
#include "z_zone.h"

// Even in hires mode, we simulate what happens on a 320x200 screen; ie.
// the screen is vertically sliced into 160 columns that fall to the bottom
// of the screen. Other hires implementations of the melt effect look weird
// when they try to scale up the original logic to higher resolutions.

// Number of "falling columns" on the screen.
static int num_columns;

// Distance a column falls before it reaches the bottom of the screen.
#define COLUMN_MAX_Y 200

//
// SCREEN WIPE PACKAGE
//

static byte *wipe_scr_start;
static byte *wipe_scr_end;
static byte *wipe_scr;

static void wipe_shittyColMajorXform(byte *array, int width, int height)
{
  byte *dest = Z_Malloc(width*height, PU_STATIC, 0);
  int x, y;

  for(y=0;y<height;y++)
    for(x=0;x<width;x++)
      dest[x*height+y] = array[y*width+x];
  memcpy(array, dest, width*height);
  Z_Free(dest);
}

// [FG] cross-fading screen wipe implementation

static int fade_tick;

static int wipe_initColorXForm(int width, int height, int ticks)
{
  V_PutBlock(0, 0, width, height, wipe_scr_start);
  fade_tick = 0;
  return 0;
}

static int wipe_doColorXForm(int width, int height, int ticks)
{
  for (int y = 0; y < height; y++)
  {
    byte *sta = wipe_scr_start + y * width;
    byte *end = wipe_scr_end + y * width;
    byte *dst = wipe_scr + y * video.pitch;

    for (int x = 0; x < width; x++)
    {
      unsigned int *fg2rgb = Col2RGB8[fade_tick];
      unsigned int *bg2rgb = Col2RGB8[64 - fade_tick];
      unsigned int fg, bg;

      fg = fg2rgb[end[x]];
      bg = bg2rgb[sta[x]];
      fg = (fg + bg) | 0x1f07c1f;
      dst[x] = RGB32k[0][0][fg & (fg >> 15)];
    }
  }

  fade_tick += 2 * ticks;

  return (fade_tick > 64);
}

static int wipe_exit(int width, int height, int ticks)
{
  Z_Free(wipe_scr_start);
  Z_Free(wipe_scr_end);
  return 0;
}

static int *col_y;

static int wipe_initMelt(int width, int height, int ticks)
{
  int i;

  num_columns = video.unscaledw / 2;

  col_y = Z_Malloc(num_columns * sizeof(*col_y), PU_STATIC, NULL);

  // copy start screen to main screen
  V_PutBlock(0, 0, width, height, wipe_scr_start);

  // makes this wipe faster (in theory)
  // to have stuff in column-major format
  wipe_shittyColMajorXform(wipe_scr_start, width, height);
  wipe_shittyColMajorXform(wipe_scr_end, width, height);

  // setup initial column positions (y<0 => not ready to scroll yet)
  col_y[0] = -(M_Random()%16);
  for (i=1;i<num_columns;i++)
    {
      int r = (M_Random()%3) - 1;
      col_y[i] = col_y[i-1] + r;
      if (col_y[i] > 0)
        col_y[i] = 0;
      else
        if (col_y[i] == -16)
          col_y[i] = -15;
    }
  return 0;
}

static int wipe_doMelt(int width, int height, int ticks)
{
  boolean done = true;
  int x, y, xfactor, yfactor;

  while (ticks--)
    for (x=0;x<num_columns;x++)
      if (col_y[x]<0)
        {
          col_y[x]++;
          done = false;
        }
      else if (col_y[x] < COLUMN_MAX_Y)
        {
          int dy = (col_y[x] < 16) ? col_y[x]+1 : 8;
          if (col_y[x]+dy >= COLUMN_MAX_Y)
            dy = COLUMN_MAX_Y - col_y[x];
          col_y[x] += dy;
          done = false;
        }

  xfactor = (width + num_columns - 1) / num_columns;
  yfactor = (height + COLUMN_MAX_Y - 1) / COLUMN_MAX_Y;

  for (x = 0; x < width; x++)
  {
    byte *s, *d;
    int scroff = col_y[x / xfactor] * yfactor;
    if (scroff > height)
      scroff = height;

    d = wipe_scr + x;
    s = wipe_scr_end + x * height;
    for (y = 0; y < scroff; ++y)
    {
      *d = *s;
      d += video.pitch;
      s++;
    }
    s = wipe_scr_start + x * height;
    for (; y < height; ++y)
    {
      *d = *s;
      d += video.pitch;
      s++;
    }
  }
  return done;
}

static int wipe_exitMelt(int width, int height, int ticks)
{
  Z_Free(col_y);
  wipe_exit(width, height, ticks);
  return 0;
}

int wipe_StartScreen(int x, int y, int width, int height)
{
  int size = video.width * video.height;
  wipe_scr_start = Z_Malloc(size * sizeof(*wipe_scr_start), PU_STATIC, NULL);
  I_ReadScreen(wipe_scr_start);
  return 0;
}

int wipe_EndScreen(int x, int y, int width, int height)
{
  int size = video.width * video.height;
  wipe_scr_end = Z_Malloc(size * sizeof(*wipe_scr_end), PU_STATIC, NULL);
  I_ReadScreen(wipe_scr_end);
  V_DrawBlock(x, y, width, height, wipe_scr_start); // restore start scr.
  return 0;
}

static int wipe_NOP(int x, int y, int t)
{
  return 0;
}

static int (*const wipes[])(int, int, int) = {
  wipe_NOP,
  wipe_NOP,
  wipe_exit,
  wipe_initMelt,
  wipe_doMelt,
  wipe_exitMelt,
  wipe_initColorXForm,
  wipe_doColorXForm,
  wipe_exit,
};

// killough 3/5/98: reformatted and cleaned up
int wipe_ScreenWipe(int wipeno, int x, int y, int width, int height, int ticks)
{
  static boolean go;                               // when zero, stop the wipe

  width = video.width;
  height = video.height;

  if (!go)                                         // initial stuff
    {
      go = 1;
      wipe_scr = I_VideoBuffer;
      wipes[wipeno*3](width, height, ticks);
    }
  if (wipes[wipeno*3+1](width, height, ticks))     // final stuff
    {
      wipes[wipeno*3+2](width, height, ticks);
      go = 0;
    }
  return !go;
}

//----------------------------------------------------------------------------
//
// $Log: f_wipe.c,v $
// Revision 1.3  1998/05/03  22:11:24  killough
// beautification
//
// Revision 1.2  1998/01/26  19:23:16  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:02:54  rand
// Lee's Jan 19 sources
//
//----------------------------------------------------------------------------
