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
// DESCRIPTION:
//      System specific interface stuff.
//
//-----------------------------------------------------------------------------

#ifndef __I_VIDEO__
#define __I_VIDEO__


#include "doomtype.h"
#include "tables.h"

extern int SCREENWIDTH;
extern int SCREENHEIGHT;
extern int NONWIDEWIDTH; // [crispy] non-widescreen SCREENWIDTH
extern int WIDESCREENDELTA; // [crispy] horizontal widescreen offset

extern angle_t FOV;

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

void I_ResetScreen(void);   // killough 10/98
void I_ToggleVsync(void); // [JN] Calls native SDL vsync toggle

extern boolean use_vsync;  // killough 2/8/98: controls whether vsync is called
extern boolean disk_icon;  // killough 10/98
extern int hires, default_hires;      // killough 11/98
extern int hires_mult, hires_square;

extern boolean use_aspect;
extern boolean uncapped, default_uncapped; // [FG] uncapped rendering frame rate

extern boolean fullscreen;
extern boolean exclusive_fullscreen;
extern int fpslimit; // when uncapped, limit framerate to this value
extern int fps;
extern boolean integer_scaling; // [FG] force integer scales
extern boolean vga_porch_flash; // emulate VGA "porch" behaviour
extern int widescreen; // widescreen mode
extern int video_display; // display index
extern boolean screenvisible;
extern boolean need_reset;
extern boolean smooth_scaling;
extern boolean toggle_fullscreen;
extern boolean toggle_exclusive_fullscreen;
extern boolean default_grabmouse;

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
