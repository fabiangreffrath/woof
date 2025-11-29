//
// Copyright(C) 2020 by Ryan Krafnick
// Copyright(C) 2025 by Guilherme Miranda
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// DESCRIPTION:
//	Doom Speed Demos Archive, demo analysis
//

#ifndef __G_ANALYSIS__
#define __G_ANALYSIS__

#include "doomtype.h"

extern int demo_analysis;

extern boolean demo_pacifist;
extern boolean demo_reality;
extern boolean demo_almost_reality;
extern boolean demo_reborn;
extern int demo_missed_monsters;
extern int demo_missed_secrets;
extern int demo_missed_weapons;
extern boolean demo_tyson_weapons;
extern boolean demo_100k;
extern boolean demo_100s;
extern boolean demo_any_counted_monsters;
extern boolean demo_any_monsters;
extern boolean demo_any_secrets;
extern boolean demo_any_weapons;
extern boolean demo_stroller;
extern boolean demo_nomo;
extern boolean demo_respawn;
extern boolean demo_fast;
extern boolean demo_turbo;
extern boolean demo_weapon_collector;

// TODO?
// DSDA's -track params are not supported

/*
extern int demo_kills_on_map;
extern boolean demo_100k_on_map;
extern boolean demo_100k_note_shown;
extern boolean demo_pacifist_note_shown;
extern boolean demo_reality_note_shown;
extern boolean demo_almost_reality_note_shown;
*/

extern void G_ResetAnalysis(void);
extern void G_WriteAnalysis(void);
extern const char* G_DetectCategory(void);

#endif
