//
//  Copyright (C) 2025 Guilherme Miranda
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
//  DESCRIPTION:
//    ID24 DemoLoop

#include "doomdef.h"

// Screen graphic or DEMO lump, NONE is used for fault tolerance.
typedef enum
{
    TYPE_NONE = -1,
    TYPE_ART_SCREEN,
    TYPE_DEMO_LUMP,
} dl_type_t;

// Immediate switch or screen melt, NONE is used for fault tolerance.
typedef enum
{
    OUTRO_WIPE_NONE = -1,
    OUTRO_WIPE_IMMEDIATE,
    OUTRO_WIPE_SCREEM_MELT,
} dl_outro_wipe_t;

// Individual demoloop units.
typedef struct
{
    const char*     primary_lump;   // Screen graphic or DEMO lump.
    const char*     secondary_lump; // Music lump for screen graphic.
    int             duration;       // Game tics.
    dl_type_t       type;
    dl_outro_wipe_t outro_wipe;
} demoloop_entry_t;

typedef demoloop_entry_t* demoloop_t;

// Actual DemoLoop data structure.
extern demoloop_t demoloop;
// Formerly 7 for Ultimate Doom & Final Doom, 6 otherwise.
extern int        demoloop_count;
// Formerly "demosequence".
extern int        demoloop_current;

// Parse the DEMOLOOP, returning NULL to "demoloop" should any errors occur.
void D_ParseDemoLoop(void);
// If "demoloop" is NULL, check for defaults using mission and mode.
void D_GetDefaultDemoLoop(GameMission_t mission, GameMode_t mode);
// Perform both "D_ParseDemoLoop" and "D_GetDefaultDemoLoop".
void D_SetupDemoLoop(void);
