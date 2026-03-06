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
//    ID24 DemoLoop Specification - User customizable title screen sequence.
//    Though originally hardcoded on the original Doom engine this allows
//    for Doom modders to define custom sequences, using any provided custom
//    graphic, music or DEMO lump.
//

#ifndef _D_DEMOLOOP_
#define _D_DEMOLOOP_

// Screen graphic or DEMO lump.
typedef enum
{
    TYPE_ART,
    TYPE_DEMO,
} dl_type_t;

// Immediate switch or screen melt.
// TODO: reimplement more cleanly at a later, more relevant moment
typedef enum
{
    WIPE_IMMEDIATE,
    WIPE_MELT,
} dl_wipe_t;

// Individual demoloop units.
typedef struct
{
    char      primary_lump[9];   // Screen graphic or DEMO lump.
    char      secondary_lump[9]; // Music lump for screen graphic.
    int       duration;          // Game tics.
    dl_type_t type;
    dl_wipe_t outro_wipe;
} demoloop_entry_t;

typedef demoloop_entry_t* demoloop_t;

// Actual DemoLoop data structure.
extern demoloop_t demoloop;
// Formerly 7 for Ultimate Doom & Final Doom, 6 otherwise.
extern int        demoloop_count;

// Perform both "D_ParseDemoLoop" and "D_GetDefaultDemoLoop".
void D_SetupDemoLoop(void);

#endif // _D_DEMOLOOP_
