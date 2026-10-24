//
//  Copyright (C) 2026 Roman Fomin
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

#include "doomtype.h"
#include "g_skillinfo.h"
#include "m_config.h"
#include "mn_csmenu.h"

boolean csmenu_skill;

csmenu_t csmenu;

void M_InitCustomSkill(void)
{
    csmenu_skill = sk_medium;
}

void MN_CSmenuSelectSkillLevel(void)
{
    switch (csmenu_skill)
    {
        case sk_baby:
            csmenu.halfplayerdamage = true;
            csmenu.doubleammo = true;
            break;
        case sk_nightmare:
            csmenu.doubleammo = true;
            csmenu.aggromonsters = true;
            csmenu.fastparm = true;
            csmenu.respawnparm = true;
            break;
        default:
            break;
    }

    MN_DisableItem(csmenu_skill == sk_baby, &csmenu_menu,
                   "csmenu.halfplayerdamage");
    MN_DisableItem(csmenu_skill == sk_baby || csmenu_skill == sk_nightmare,
                   &csmenu_menu, "csmenu.doubleammo");
    MN_DisableItem(csmenu_skill == sk_nightmare, &csmenu_menu,
                   "csmenu.aggromonsters", "csmenu.fastparm",
                   "csmenu.respawnparm");
}

void MN_CSMenuStartGame(void)
{
    G_UpdateCustomSkill(num_og_skills);
    M_ChooseSkill(num_og_skills);
}

void MN_BindCSMenuVariables(void)
{
    BIND_NUM_MENU(csmenu_skill, sk_baby, sk_nightmare);
    BIND_BOOL_MENU(csmenu.nomonsters);
    BIND_BOOL_MENU(csmenu.fastparm);
    BIND_BOOL_MENU(csmenu.respawnparm);
    BIND_BOOL_MENU(csmenu.pistolstart);
    BIND_BOOL_MENU(csmenu.coopspawns);
    BIND_BOOL_MENU(csmenu.doubleammo);
    BIND_BOOL_MENU(csmenu.halfplayerdamage);
    BIND_BOOL_MENU(csmenu.aggromonsters);
}
