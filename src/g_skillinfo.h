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

typedef enum
{
    SI_SPAWN_MULTI = (1u << 0),
    SI_FAST_MONSTERS = (1u << 1),
    SI_INSTANT_REACTION = (1u << 2),
    SI_DEFAULT_SKILL = (1u << 3),
    SI_EASY_BOSS_BRAIN = (1u << 4),
    SI_MUST_CONFIRM = (1u << 5),
    SI_NO_MONSTERS = (1u << 6),
    SI_PISTOL_START = (1u << 7),
} skill_info_flags_t;

typedef struct
{
    fixed_t ammo_factor;
    fixed_t damage_factor;
    int respawn_time;
    int spawn_filter;
    char key;
    const char *must_confirm;
    const char *name;
    const char *pic_name;
    int helper_dogs;
    skill_info_flags_t flags;
} skill_info_t;

extern skill_info_t skill_info;
extern skill_info_t *skill_infos;

typedef enum
{
    sk_default = -2,
    sk_none = -1, // jff 3/24/98 create unpicked skill setting
    sk_baby = 0,
    sk_easy,
    sk_medium,
    sk_hard,
    sk_nightmare,
    sk_custom,
} skill_t;

extern int num_skills;
extern int num_og_skills;
extern int num_cskill;

void G_InitSkills(void);
void G_RefreshGameSkill(void);
void G_UpdateGameSkill(int skill);
void G_UpdateCustomSkill(int custom_skill_num);

#endif
