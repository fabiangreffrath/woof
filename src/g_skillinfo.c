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

// #include "dsda/args.h"
// #include "dsda/configuration.h"
// #include "dsda/utility.h"

skill_info_t skill_info;

const skill_info_t doom_skill_infos[5] = {
  {
    .ammo_factor = FRACUNIT * 2,
    .damage_factor = FRACUNIT / 2,
    .spawn_filter = 1,
    .key = 'i',
    .name = "I'm too young to die.",
    .pic_name = "M_JKILL",
    .flags = SI_EASY_BOSS_BRAIN
  },
  {
    .spawn_filter = 2,
    .key = 'h',
    .name = "Hey, not too rough.",
    .pic_name = "M_ROUGH",
    .flags = SI_EASY_BOSS_BRAIN
  },
  {
    .spawn_filter = 3,
    .key = 'h',
    .name = "Hurt me plenty.",
    .pic_name = "M_HURT",
    .flags = 0
  },
  {
    .spawn_filter = 4,
    .key = 'u',
    .name = "Ultra-Violence.",
    .pic_name = "M_ULTRA",
    .flags = 0
  },
  {
    .ammo_factor = FRACUNIT * 2,
    .spawn_filter = 5,
    .key = 'n',
    .name = "Nightmare!",
    .pic_name = "M_NMARE",
    .respawn_time = 12,
    .flags = SI_FAST_MONSTERS | SI_INSTANT_REACTION | SI_MUST_CONFIRM
  },
};

int num_skills = 5;
skill_info_t* skill_infos;

void G_InitSkills(void) {
  const skill_info_t* original_skill_infos;

  skill_infos = calloc(num_skills, sizeof(*skill_infos));

  original_skill_infos = doom_skill_infos;
  for (int i = 0; i < 5; ++i)
    skill_infos[i] = original_skill_infos[i];
}

// At startup, set-up temp game modifier configs based off args / persistent cfgs
// Only set once, as modifiers can break away from args
//
// "Always Pistol Start" is the only persistent cfg (saved in cfg file)
//

// During demo recording/playback only use args, else use cfgs
static void ResetGameModifiers(void)
{
  pistolstart = (allow_incompatibility ? clpistolstart : false);   // pistolstart not allowed in demos?
  respawnparm = clrespawnparm;
  fastparm    = clfastparm;
  nomonsters  = clnomonsters;
  coopspawns  = clcoopspawns;
}

// void G_InitGameModifiers(void)
// {
//   if (clpistolstart)
//       dsda_UpdateIntConfig(dsda_config_pistol_start, true, true);
//   if (clrespawnparm)
//       dsda_UpdateIntConfig(dsda_config_respawn_monsters, true, true);
//   if (clfastparm)
//       dsda_UpdateIntConfig(dsda_config_fast_monsters, true, true);
//   if (clnomonsters)
//       dsda_UpdateIntConfig(dsda_config_no_monsters, true, true);
//   if (clcoopspawns)
//       dsda_UpdateIntConfig(dsda_config_coop_spawns, true, true);

//   // Pistol-start config can reset other modifier configs
//   // Explicitly refresh everything for configs to match args
//   ResetGameModifiers();
// }

void G_RefreshGameSkill(void) {
  void G_RefreshFastMonsters(void);

  if (allow_incompatibility)
    ResetGameModifiers();

  skill_info = skill_infos[gameskill];

  if (respawnparm && !skill_info.respawn_time)
    skill_info.respawn_time = 12;

  if (fastparm)
    skill_info.flags |= SI_FAST_MONSTERS;

  if (coopspawns)
    skill_info.flags |= SI_SPAWN_MULTI;

  G_RefreshFastMonsters();
}

void G_UpdateGameSkill(int skill) {
  if (skill > num_skills - 1)
    skill = num_skills - 1;

  gameskill = skill;
  G_RefreshGameSkill();
}

void G_AlterGameFlags(void)
{
  if (!allow_incompatibility || !in_game)
    return;

  G_RefreshGameSkill();
}