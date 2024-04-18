//
//  Copyright (C) 1999 by
//  id Software, Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman
//  Copyright (C) 2022 Roman Fomin
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
//      Timer functions.
//

#ifndef __I_TIMER__
#define __I_TIMER__

#include "doomtype.h"

// Called by D_DoomLoop,
// returns current time in tics.
extern int (*I_GetTime)(void);
int I_GetTime_RealTime(void); // killough

extern int (*I_GetFracTime)(void);

// [FG] Same as I_GetTime, but returns time in milliseconds
int I_GetTimeMS(void);

uint64_t I_GetTimeUS(void);

void I_SetTimeScale(int scale);

void I_SetFastdemoTimer(boolean on);

// [FG] toggle demo warp mode
void I_EnableWarp(boolean warp);

// Pause for a specified number of ms
void I_Sleep(int ms);

void I_SleepUS(uint64_t us);

// Initialize timer
void I_InitTimer(void);

// Wait for vertical retrace or pause a bit.
void I_WaitVBL(int count);

#endif
