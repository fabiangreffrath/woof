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

#define FOV_DEFAULT 90
#define FOV_MIN 60
#define FOV_MAX 120
#define ASPECT_RATIO_MAX 3.6 // Up to 32:9 ultrawide.
#define ASPECT_RATIO_MIN (4.0 / 3.0)

typedef enum
{
    RATIO_ORIG,
    RATIO_AUTO,
    RATIO_16_10,
    RATIO_16_9,
    RATIO_21_9,
    NUM_RATIOS
} aspect_ratio_mode_t;

typedef struct
{
    int max;
    int step;
} resolution_scaling_t;

void I_GetResolutionScaling(resolution_scaling_t *rs);

// Called by D_DoomMain,
// determines the hardware configuration
// and sets up the video mode

void I_InitGraphics(void);
void I_ShutdownGraphics(void);

// Takes full 8 bit values.
void I_SetPalette(byte* palette);

void I_FinishUpdate(void);

void I_ReadScreen(byte* dst);

void I_ResetScreen(void);   // killough 10/98
void I_ToggleVsync(void); // [JN] Calls native SDL vsync toggle

void I_DynamicResolution(void);

extern boolean drs_skip_frame;

extern boolean use_vsync;  // killough 2/8/98: controls whether vsync is called
extern boolean disk_icon;  // killough 10/98
extern int current_video_height;

#define DRS_MIN_HEIGHT 400
extern boolean dynamic_resolution;

extern boolean use_aspect;
extern boolean uncapped, default_uncapped; // [FG] uncapped rendering frame rate

extern boolean fullscreen;
extern boolean exclusive_fullscreen;
extern int fpslimit; // when uncapped, limit framerate to this value
extern int fps;
extern boolean vga_porch_flash; // emulate VGA "porch" behaviour
extern aspect_ratio_mode_t widescreen, default_widescreen; // widescreen mode
extern int custom_fov; // Custom FOV set by the player.
extern int video_display; // display index
extern boolean screenvisible;
extern boolean window_focused;
extern boolean resetneeded;
extern boolean setrefreshneeded;
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

void I_ShowMouseCursor(boolean on);
void I_ResetRelativeMouseState(void);

boolean I_ChangeRes(void);
void I_CheckHOM(void);

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
