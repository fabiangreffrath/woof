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
#include "p_mobj.h"

typedef struct demo_analysis_s
{
    boolean pacifist;
    boolean reality;
    boolean almost_reality;
    boolean reborn;
    int missed_monsters;
    int missed_secrets;
    int missed_weapons;
    boolean tyson;
    boolean kill_100;
    boolean secret_100;
    boolean any_counted_monsters;
    boolean any_monsters;
    boolean any_secrets;
    boolean any_weapons;
    boolean stroller;
    boolean nomo;
    boolean respawn;
    boolean fast;
    boolean turbo;
    boolean collector;
} demo_analysis_t;

extern demo_analysis_t analysis;

extern boolean G_IsWeapon(mobj_t *mobj);
extern void G_ResetAnalysis(void);
extern void G_WriteAnalysis(void);
extern const char* G_DetectCategory(void);

#endif
