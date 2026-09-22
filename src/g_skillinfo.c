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

#include "g_skillinfo.h"

#include "d_main.h"
#include "doomstat.h"
#include "g_game.h"
#include "mn_menu.h"

skill_info_t skill_info;

const skill_info_t doom_skill_infos[5] = {
    {
     .ammo_factor = FRACUNIT * 2,
     .damage_factor = FRACUNIT / 2,
     .spawn_filter = 1,
     .key = 'i',
     .name = "I'm too young to die.",
     .pic_name = "M_JKILL",
     .helper_dogs = -1,
     .flags = SI_EASY_BOSS_BRAIN
    },
    {
     .spawn_filter = 2,
     .key = 'h',
     .name = "Hey, not too rough.",
     .pic_name = "M_ROUGH",
     .helper_dogs = -1,
     .flags = SI_EASY_BOSS_BRAIN
    },
    {
     .spawn_filter = 3,
     .key = 'h',
     .name = "Hurt me plenty.",
     .pic_name = "M_HURT",
     .helper_dogs = -1,
     .flags = 0
    },
    {
     .spawn_filter = 4,
     .key = 'u',
     .name = "Ultra-Violence.",
     .pic_name = "M_ULTRA",
     .helper_dogs = -1,
     .flags = 0
    },
    {
     .ammo_factor = FRACUNIT * 2,
     .spawn_filter = 5,
     .key = 'n',
     .name = "Nightmare!",
     .pic_name = "M_NMARE",
     .respawn_time = 12,
     .helper_dogs = -1,
     .flags = SI_FAST_MONSTERS | SI_INSTANT_REACTION | SI_MUST_CONFIRM
    },
};

int num_skills;
int num_og_skills;
int num_cskill;
skill_info_t *skill_infos;

void G_InitSkills(void)
{
    const skill_info_t *original_skill_infos;

    num_skills = 5 + 1; // Custom skill
    num_og_skills = num_skills - 1;
    num_cskill = num_og_skills;

    skill_infos = calloc(num_skills, sizeof(*skill_infos));

    original_skill_infos = doom_skill_infos;
    for (int i = 0; i < 5; ++i)
    {
        skill_infos[i] = original_skill_infos[i];
    }
}

void G_UpdateCustomSkill(int custom_skill_num)
{
    skill_infos[custom_skill_num].name = "Custom Skill...";
    skill_infos[custom_skill_num].flags = 0;
    skill_infos[custom_skill_num].respawn_time = 0;

    skill_infos[custom_skill_num].spawn_filter = csmenu_skill + 1;

    skill_infos[custom_skill_num].ammo_factor = csmenu.doubleammo ? FRACUNIT * 2 : FRACUNIT;
    skill_infos[custom_skill_num].damage_factor = csmenu.halfplayerdamage ? FRACUNIT / 2 : FRACUNIT;

  if (csmenu.respawnparm) skill_infos[custom_skill_num].respawn_time = 12;

  if (csmenu.coopspawns)     skill_infos[custom_skill_num].flags |= SI_SPAWN_MULTI;
  if (csmenu.nomonsters)     skill_infos[custom_skill_num].flags |= SI_NO_MONSTERS;
  if (csmenu.fastparm)       skill_infos[custom_skill_num].flags |= SI_FAST_MONSTERS;
  if (csmenu.aggromonsters)  skill_infos[custom_skill_num].flags |= SI_INSTANT_REACTION;
  if (csmenu.pistolstart)    skill_infos[custom_skill_num].flags |= SI_PISTOL_START;

    skill_infos[custom_skill_num].helper_dogs = csmenu.helperdogs;

    G_UpdateGameSkill(custom_skill_num);
}

void G_RefreshGameSkill(void)
{
    skill_info = skill_infos[gameskill];

    if (respawnparm && !skill_info.respawn_time)
        skill_info.respawn_time = 12;

    if (nomonsters)
        skill_info.flags |= SI_NO_MONSTERS;

    if (fastparm)
        skill_info.flags |= SI_FAST_MONSTERS;

    if (coopspawns)
        skill_info.flags |= SI_SPAWN_MULTI;

    if (pistolstart)
        skill_info.flags |= SI_PISTOL_START;

    G_RefreshFastMonsters();
}

void G_UpdateGameSkill(int skill)
{
    skill = MIN(skill, num_skills - 1);

    gameskill = skill;
    G_RefreshGameSkill();
}
