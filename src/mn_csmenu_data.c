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

#include "mn_container.h"
#include "mn_csmenu.h"

static const char *skill_strings[] = {
    "I'm too young to die", "Hey, not too rough", "Hurt me plenty",
    "Ultra-Violence", "NIGHTMARE!",
};

static mn_item_t csmenu_items[] = {
    MN_GAP(10),
    MN_CHOICE_ACT("Skill Level", "csmenu_skill", skill_strings, 16,
                  MN_CSmenuSelectSkillLevel, NULL),
    MN_TOGGLE("Half damage", "csmenu.halfplayerdamage", NULL),
    MN_TOGGLE("Double ammo", "csmenu.doubleammo", NULL),
    MN_TOGGLE("Fast monsters", "csmenu.fastparm", NULL),
    MN_TOGGLE("Aggressive monsters", "csmenu.aggromonsters", NULL),
    MN_TOGGLE("Respawn monsters", "csmenu.respawnparm", NULL),
    MN_GAP(6),
    MN_TOGGLE("No monsters", "csmenu.nomonsters", NULL),
    MN_TOGGLE("Co-op spawns", "csmenu.coopspawns", NULL),
    MN_TOGGLE("Pistol start", "csmenu.pistolstart", NULL),
    MN_GAP(6),
    {.label = "Start Game", .type = MN_ITEM_ACTION,
     .flags = MN_ITEM_CENTER | MN_ITEM_HIDE_VALUE,
     .action = MN_CSMenuStartGame},
    MN_END
};

static mn_page_t csmenu_pages[] = {
    {.items = csmenu_items},
};

mn_menu_t csmenu_menu = {
    .title_patch = "",
    .title_text = "",
    .background = "FLOOR4_6",
    .flags = MN_MENU_CENTER_LAYOUT,
    .layout = {.max_width = MN_PX(250)},
    .pages = csmenu_pages,
    .num_pages = arrlen(csmenu_pages),
};
