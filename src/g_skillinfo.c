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

#include <math.h>
#include <string.h>

#include "d_main.h"
#include "doomstat.h"
#include "g_game.h"
#include "m_array.h"
#include "m_json.h"
#include "mn_menu.h"

#include "g_skillinfo.h"

skill_info_t skill_info;

int num_skills;
int num_og_skills;
int num_cskill;
skill_info_t *skill_infos;

static void ParseSkillDef()
{
    json_t *json = JS_Open("SKILLDEF", "skillinfo", (version_t){1, 0, 0});
    if (!json)
    {
        I_Error("SKILLDEF: Error while parsing file");
    }
    
    json_t *data = JS_GetObject(json, "data");
    if (JS_IsNull(data) || !JS_IsObject(data))
    {
        I_Error("SKILLDEF: no data");
    }

    json_t *skill_levels = JS_GetObject(data, "skill_levels");
    json_t *skill_level = NULL;
    JS_ArrayForEach(skill_level, skill_levels)
    {
        skill_info_t info = {0};

        double ammo_factor = JS_GetNumberValue(skill_level, "ammo_factor");
        if (ammo_factor)
            info.ammo_factor = FRACUNIT * ammo_factor;

        double damage_factor = JS_GetNumberValue(skill_level, "damage_factor");
        if (damage_factor)
            info.damage_factor = FRACUNIT * damage_factor;

        info.respawn_time = JS_GetIntegerValue(skill_level, "respawn_time");
        info.spawn_filter = JS_GetIntegerValue(skill_level, "spawn_filter");

        const char *key = JS_GetStringValue(skill_level, "key");
        if (key)
            info.key = *key;
        
        boolean must_confirm = JS_GetBooleanValue(skill_level, "must_confirm");
        if (must_confirm)
            info.flags |= SI_MUST_CONFIRM;

        const char *must_confirm_message = JS_GetStringValue(skill_level, "must_confirm_message");
        if (must_confirm_message)
            info.must_confirm = M_StringDuplicate(must_confirm_message);
        
        const char *name = JS_GetStringValue(skill_level, "name");
        if (name)
            info.name = M_StringDuplicate(name);
        
        const char *pic_name = JS_GetStringValue(skill_level, "pic_name");
        if (pic_name)
        {
            if (strlen(pic_name) > 8)
            {
                I_Error("SKILLDEF: pic_name is over 8 characters");
            }
            else
            {
                info.pic_name = M_StringDuplicate(pic_name);
            }
        }
        
        json_t *js_helper_dogs = JS_GetObject(skill_level, "helper_dogs");
        info.helper_dogs = js_helper_dogs ? JS_GetInteger(js_helper_dogs) : -1;

        boolean spawn_multi = JS_GetBooleanValue(skill_level, "spawn_multi");
        if (spawn_multi)
            info.flags |= SI_SPAWN_MULTI;
        
        boolean fast_monsters = JS_GetBooleanValue(skill_level, "fast_monsters");
        if (fast_monsters)
            info.flags |= SI_FAST_MONSTERS;
        
        boolean instant_reaction = JS_GetBooleanValue(skill_level, "instant_reaction");
        if (instant_reaction)
            info.flags |= SI_INSTANT_REACTION;
        
        boolean default_skill = JS_GetBooleanValue(skill_level, "default_skill");
        if (default_skill)
            info.flags |= SI_DEFAULT_SKILL;
        
        boolean easy_boss_brain = JS_GetBooleanValue(skill_level, "easy_boss_brain");
        if (easy_boss_brain)
            info.flags |= SI_EASY_BOSS_BRAIN;
        
        boolean no_monsters = JS_GetBooleanValue(skill_level, "no_monsters");
        if (no_monsters)
            info.flags |= SI_NO_MONSTERS;
        
        boolean pistol_start = JS_GetBooleanValue(skill_level, "pistol_start");
        if (pistol_start)
            info.flags |= SI_PISTOL_START;

        array_push(skill_infos, info);
    }

    JS_Close("SKILLDEF");
}

void G_InitSkills(void)
{
    num_skills = 5 + 1; // Custom skill
    num_og_skills = num_skills - 1;
    num_cskill = num_og_skills;

    ParseSkillDef();
}

void G_UpdateCustomSkill(int custom_skill_num)
{
    skill_infos[custom_skill_num].name = "Custom Skill...";
    skill_infos[custom_skill_num].flags = SI_NONE;
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

    if (!(gameskill == num_cskill))
    {
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
    }

    G_RefreshFastMonsters();
}

void G_UpdateGameSkill(int skill)
{
    skill = MIN(skill, num_skills - 1);

    gameskill = skill;
    G_RefreshGameSkill();
}
