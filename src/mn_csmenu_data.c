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
#include "mn_defs.h"
#include "mn_csmenu.h"

static const char *skill_strings[] = {
    "I'm too young to die", "Hey, not too rough", "Hurt me plenty",
    "Ultra-Violence", "NIGHTMARE!",
};

static mn_item_t csmenu_items[] = {
    {.label = ("Skill Level"),
     .cfg = ("csmenu_skill"),
     .type = ItemType_Choice,
     .choices = (skill_strings),
     .num_choices = (sizeof(skill_strings) / sizeof(*skill_strings)),
     .choice_size = 16,
     .offset = 7 * 8,
     .action = (MN_CSmenuSelectSkillLevel),
     .description = (((void *)0))},
    MN_TOGGLE("Half damage", "csmenu.halfplayerdamage", NULL),
    MN_TOGGLE("Double ammo", "csmenu.doubleammo", NULL),
    MN_TOGGLE("Fast monsters", "csmenu.fastparm", NULL),
    MN_TOGGLE("Aggressive monsters", "csmenu.aggromonsters", NULL),
    MN_TOGGLE("Respawn monsters", "csmenu.respawnparm", NULL),
    MN_TOGGLE("No monsters", "csmenu.nomonsters", NULL),
    MN_TOGGLE("Co-op spawns", "csmenu.coopspawns", NULL),
    MN_TOGGLE("Pistol start", "csmenu.pistolstart", NULL),
    {.label = "Start Game",
     .type = ItemType_Action,
     .flags = MN_ITEM_CENTER | MN_ITEM_HIDE_VALUE,
     .action = MN_CSMenuStartGame},
};

static mn_page_t csmenu_pages[] = {
    {.items = csmenu_items, .num_items = arrlen(csmenu_items)},
};

mn_menu_t csmenu_menu = {
    .title_patch = "",
    .title_text = "",
    .background = "FLOOR4_6",
    .flags = MN_MENU_CENTER_W,
    .layout = {.max_width = MN_PX(250),
               .value_column = MN_PX(99),
               .item_spacing = 1},
    .pages = csmenu_pages,
    .num_pages = arrlen(csmenu_pages),
};
