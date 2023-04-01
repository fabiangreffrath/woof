//
//  Copyright (C) 1999 by
//  id Software, Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman
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
//      Put all global state variables here.
//
//-----------------------------------------------------------------------------

#include "doomstat.h"
#include "m_misc2.h"

// Game Mode - identify IWAD as shareware, retail etc.
GameMode_t gamemode = indetermined;
GameMission_t   gamemission = doom;

// [FG] emulate a specific version of Doom
GameVersion_t gameversion = exe_doom_1_9;

GameVersions_t gameversions[] = {
    {"Doom 1.9",      "1.9",      exe_doom_1_9},
    {"Ultimate Doom", "ultimate", exe_ultimate},
    {"Final Doom",    "final",    exe_final},
    {"Chex Quest",    "chex",     exe_chex},
    { NULL,           NULL,       0},
};

// Language.
Language_t   language = english;

// Set if homebrew PWAD stuff has been added.
boolean modifiedgame;

boolean have_ssg;

//-----------------------------------------------------------------------------

// compatibility with old engines (monster behavior, metrics, etc.)
int compatibility, default_compatibility;          // killough 1/31/98

int comp[COMP_TOTAL], default_comp[COMP_TOTAL];    // killough 10/98

// [FG] overflow emulation
overflow_t overflow[EMU_TOTAL] = {
  { true, false, "spechits_overflow"},
  { true, false, "reject_overflow"},
  { true, false, "intercepts_overflow"},
  { true, false, "missedbackside_overflow"},
  { true, false, "donut_overflow"}
};

int demo_version;           // killough 7/19/98: Boom version of demo

// v1.1-like pitched sounds
int pitched_sounds;  // killough 10/98

int translucency;    // killough 10/98

int  allow_pushers = 1;      // MT_PUSH Things              // phares 3/10/98
int  default_allow_pushers;  // killough 3/1/98: make local to each game

int  variable_friction = 1;      // ice & mud               // phares 3/10/98
int  default_variable_friction;  // killough 3/1/98: make local to each game

int  weapon_recoil;              // weapon recoil                   // phares
int  default_weapon_recoil;      // killough 3/1/98: make local to each game

int player_bobbing;  // whether player bobs or not          // phares 2/25/98
int default_player_bobbing;      // killough 3/1/98: make local to each game

int monsters_remember=1;        // killough 3/1/98
int default_monsters_remember=1;

int monster_infighting=1;       // killough 7/19/98: monster<=>monster attacks
int default_monster_infighting=1;

int monster_friction=1;       // killough 10/98: monsters affected by friction 
int default_monster_friction=1;

// killough 7/19/98: classic Doom BFG
int classic_bfg, default_classic_bfg;

// killough 7/24/98: Emulation of Press Release version of Doom
int beta_emulation;

int dogs, default_dogs;         // killough 7/19/98: Marine's best friend :)
int dog_jumping, default_dog_jumping;   // killough 10/98

// killough 8/8/98: distance friends tend to move towards players
int distfriend = 128, default_distfriend = 128;

// killough 9/8/98: whether monsters are allowed to strafe or retreat
int monster_backing, default_monster_backing;

// killough 9/9/98: whether monsters are able to avoid hazards (e.g. crushers)
int monster_avoid_hazards, default_monster_avoid_hazards;

// killough 9/9/98: whether monsters help friends
int help_friends, default_help_friends;

int flashing_hom;     // killough 10/98

int doom_weapon_toggles; // killough 10/98

int monkeys, default_monkeys;

// [FG] centered weapon sprite
int center_weapon;

int cosmetic_bobbing;

char *MAPNAME(int e, int m)
{
  static char name[9];

  if (gamemode == commercial)
    M_snprintf(name, sizeof(name), "MAP%02d", m);
  else
    M_snprintf(name, sizeof(name), "E%dM%d", e, m);

  return name;
}

//----------------------------------------------------------------------------
//
// $Log: doomstat.c,v $
// Revision 1.5  1998/05/12  12:46:12  phares
// Removed OVER_UNDER code
//
// Revision 1.4  1998/05/05  16:29:01  phares
// Removed RECOIL and OPT_BOBBING defines
//
// Revision 1.3  1998/05/03  23:12:13  killough
// beautify, move most global switch variables here
//
// Revision 1.2  1998/01/26  19:23:10  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:03:06  rand
// Lee's Jan 19 sources
//
//----------------------------------------------------------------------------
