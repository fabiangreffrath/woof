// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// $Id: i_video.h,v 1.4 1998/05/03 22:40:58 killough Exp $
//
//  Copyright (C) 1999 by
//  id Software, Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman
//  Copyright(C) 2020-2021 Fabian Greffrath
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
//      System specific interface stuff.
//
//-----------------------------------------------------------------------------

#ifndef __I_VIDEO__
#define __I_VIDEO__


#include "doomtype.h"

extern int SCREENWIDTH;
extern int SCREENHEIGHT;
extern int NONWIDEWIDTH; // [crispy] non-widescreen SCREENWIDTH
extern int WIDESCREENDELTA; // [crispy] horizontal widescreen offset
void I_GetScreenDimensions (void); // [crispy] re-calculate WIDESCREENDELTA

enum
{
  RATIO_ORIG,
  RATIO_MATCH_SCREEN,
  RATIO_16_10,
  RATIO_16_9,
  RATIO_21_9,
  NUM_RATIOS
};

// [FG] support more joystick and mouse buttons
#define MAX_JSB NUM_CONTROLLER_BUTTONS
#define MAX_MB NUM_MOUSE_BUTTONS

// Called by D_DoomMain,
// determines the hardware configuration
// and sets up the video mode

void I_InitGraphics (void);
void I_ShutdownGraphics(void);

// Takes full 8 bit values.
void I_SetPalette (byte* palette);

void I_FinishUpdate (void);

void I_ReadScreen (byte* scr);

int I_DoomCode2ScanCode(int);   // killough
int I_ScanCode2DoomCode(int);   // killough

void I_ResetScreen(void);   // killough 10/98
void I_ToggleToggleFullScreen(void); // [FG] fullscreen mode menu toggle

extern int use_vsync;  // killough 2/8/98: controls whether vsync is called
extern int page_flip;  // killough 8/15/98: enables page flipping (320x200)
extern int disk_icon;  // killough 10/98
extern int hires;      // killough 11/98

extern int useaspect;
extern int uncapped; // [FG] uncapped rendering frame rate
extern int integer_scaling; // [FG] force integer scales
extern int vga_porch_flash; // emulate VGA "porch" behaviour
extern int widescreen; // widescreen mode
extern int video_display; // display index
extern int window_width, window_height;
extern char *window_position;
extern int fullscreen_width, fullscreen_height; // [FG] exclusive fullscreen

extern int gamma2;
byte I_GetPaletteIndex(byte *palette, int r, int g, int b);

boolean I_WritePNGfile(char *filename); // [FG] screenshots in PNG format

void *I_GetSDLWindow(void);
void *I_GetSDLRenderer(void);

void I_InitWindowIcon(void);

#endif

//----------------------------------------------------------------------------
//
// $Log: i_video.h,v $
// Revision 1.4  1998/05/03  22:40:58  killough
// beautification
//
// Revision 1.3  1998/02/09  03:01:51  killough
// Add vsync for flicker-free blits
//
// Revision 1.2  1998/01/26  19:27:01  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:03:08  rand
// Lee's Jan 19 sources
//
//----------------------------------------------------------------------------
