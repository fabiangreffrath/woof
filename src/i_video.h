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

#define FOV_DEFAULT      90
#define FOV_MIN          60
#define FOV_MAX          120
#define ASPECT_RATIO_MAX 3.6 // Up to 32:9 aspect ratio.
#define ASPECT_RATIO_MIN (4.0 / 3.0)

typedef struct
{
    int max;
    int step;
} resolution_scaling_t;

resolution_scaling_t I_GetResolutionScaling(void);

// Called by D_DoomMain,
// determines the hardware configuration
// and sets up the video mode

void I_InitGraphics(void);
void I_ShutdownGraphics(void);
void I_QuitVideo(void);

// Takes full 8 bit values.
void I_SetPalette(byte *palette);

void I_FinishUpdate(void);

void I_ReadScreen(pixel_t *dst);

void I_UpdateHudAnchoring(void);
void I_ResetScreen(void); // killough 10/98
void I_ToggleVsync(void); // [JN] Calls native SDL vsync toggle

void I_DynamicResolution(void);
void I_ResetDRS(void);

extern int current_video_height;
#define DRS_MIN_HEIGHT 400
extern boolean dynamic_resolution;
extern boolean uncapped;
extern int fps;
extern int custom_fov;    // Custom FOV set by the player.
extern boolean resetneeded;
extern boolean setrefreshneeded;
extern boolean toggle_fullscreen;
extern boolean toggle_exclusive_fullscreen;
extern boolean correct_aspect_ratio;
extern boolean screenvisible;

extern int gamma2;
byte I_GetNearestColor(byte *palette, int r, int g, int b);

boolean I_WritePNGfile(char *filename); // [FG] screenshots in PNG format

void *I_GetSDLWindow(void);
void *I_GetSDLRenderer(void);

void I_InitWindowIcon(void);

void I_ShowMouseCursor(boolean toggle);
void I_ResetRelativeMouseState(void);

void I_UpdatePriority(boolean active);

void I_BindVideoVariables(void);

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
