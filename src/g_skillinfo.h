//
// Copyright(C) 2023 by Ryan Krafnick
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
//	DSDA Skill Info
//

#ifndef G_SKILLINFO_H
#define G_SKILLINFO_H

#include "doomdef.h"
#include "m_fixed.h"

#define SI_SPAWN_MULTI      0x0001
#define SI_FAST_MONSTERS    0x0002
#define SI_INSTANT_REACTION 0x0004
#define SI_DEFAULT_SKILL    0x0008
#define SI_EASY_BOSS_BRAIN  0x0010
#define SI_MUST_CONFIRM     0x0020
#define SI_NO_MONSTERS      0x0040
#define SI_PISTOL_START     0x0080

typedef uint16_t skill_info_flags_t;

typedef struct {
  fixed_t ammo_factor;
  fixed_t damage_factor;
  int respawn_time;
  int spawn_filter;
  char key;
  const char* must_confirm;
  const char* name;
  const char* pic_name;
  int helper_dogs;
  skill_info_flags_t flags;
} skill_info_t;

extern skill_info_t skill_info;
extern skill_info_t *skill_infos;

extern int num_skills;
extern int num_og_skills;

void G_InitSkills(void);
void G_RefreshGameSkill(void);
void G_UpdateGameSkill(int skill);
void G_UpdateCustomSkill(int custom_skill_num);

#endif