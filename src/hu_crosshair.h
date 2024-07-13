//  Copyright (C) 1999 by
//  id Software, Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman
//  Copyright (C) 2023-2024 Fabian Greffrath
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
// DESCRIPTION:  Crosshair HUD component
//
//-----------------------------------------------------------------------------

#ifndef __HU_CROSSHAIR_H__
#define __HU_CROSSHAIR_H__

#include "doomtype.h"

extern int hud_crosshair;

// [Alaux] Lock crosshair on target
typedef enum
{
    crosstarget_off,
    crosstarget_highlight,
    crosstarget_health, // [Alaux] Color crosshair by target health
} crosstarget_t;

extern crosstarget_t hud_crosshair_target;
extern struct mobj_s *crosshair_target;

extern boolean hud_crosshair_health;
extern boolean hud_crosshair_lockon;
extern int hud_crosshair_color;
extern int hud_crosshair_target_color;

#define HU_CROSSHAIRS 10
extern const char *crosshair_lumps[HU_CROSSHAIRS];
extern const char *crosshair_strings[HU_CROSSHAIRS];

void HU_InitCrosshair(void);

void HU_StartCrosshair(void);

void HU_UpdateCrosshair(void);
void HU_UpdateCrosshairLock(int x, int y);

void HU_DrawCrosshair(void);

#endif
