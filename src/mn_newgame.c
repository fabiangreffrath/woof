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
#include "m_config.h"
#include "mn_container.h"
#include "mn_csmenu.h"
#include "mn_defs.h"
#include "mn_multiplayer.h"
#include "mn_options.h"

static boolean menu_extended_newgame;

static mn_item_t newgame_items[] = {
    {.label = "Single Player",
     .type = ItemType_Action,
     .action = MN_NewGame,
     .flags = MN_ITEM_HIDE_VALUE},
    MN_MENU("Custom Skill", &csmenu_menu),
    MN_MENU("Multiplayer", &multiplayer_menu),
};

static mn_page_t newgame_pages[] = {
    {.items = newgame_items, .num_items = arrlen(newgame_items)},
};

mn_menu_t newgame_menu = {
    .title_patch = "M_NEWG",
    .title_text = "New Game",
    .flags = MN_MENU_CENTER_W | MN_MENU_CENTER_H,
    .layout = {.max_width = MN_PX(100), .item_spacing = 1},
    .pages = newgame_pages,
    .num_pages = arrlen(newgame_pages),
};

boolean MN_NewGameMenuOpen(void)
{
    if (menu_extended_newgame)
    {
        MN_BindMenu(&newgame_menu);
        MN_BindMenu(&csmenu_menu);
        MN_BindMenu(&multiplayer_menu);
        MN_ContainerOpen(&newgame_menu);
        return true;
    }
    return false;
}

void MN_BindNewGameVariables(void)
{
    BIND_BOOL(menu_extended_newgame, true, "Extended New Game Menu");
}
