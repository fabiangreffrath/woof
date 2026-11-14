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

#include "doomdef.h"
#include "doomstat.h"
#include "mn_container.h"
#include "mn_multiplayer.h"
#include "nw_defs.h"

#define MN_LAYOUT {.max_width = MN_PX(500), .items_min_width = MN_PX(250)}

#define MN_NUM(lbl, key, sfx, desc)   \
    {.label = (lbl),                  \
     .cfg = (key),                    \
     .type = ItemType_Number,         \
     .suffix = (sfx),                 \
     .description = (desc)}

#define MN_ACTION(lbl, act, desc) \
    {.label = (lbl),              \
     .type = ItemType_Action,     \
     .action = (act),             \
     .description = (desc)}

#define MN_TOGGLE_INV(lbl, key, desc) \
    {.label = (lbl),                  \
     .cfg = (key),                    \
     .type = ItemType_Toggle,         \
     .flags = MN_ITEM_INVERTED,       \
     .description = (desc)}

#define MN_TEXT(lbl, key, size, desc) \
    {.label = (lbl),                  \
     .cfg = (key),                    \
     .type = ItemType_Text,           \
     .choice_size = (size),           \
     .description = (desc)}

// The skill names follow the current IWAD, as the setup tool does.

static const char *doom_skills[] = {
    "I'm too young to die.", "Hey, not too rough.", "Hurt me plenty.",
    "Ultra-Violence.", "NIGHTMARE!",
};

static const char *chex_skills[] = {
    "Easy does it", "Not so sticky", "Gobs of goo", "Extreme ooze",
    "SUPER SLIMEY!",
};

static const char *hacx_skills[] = {
    "Please don't shoot!", "Aggrh, I need health!", "Let's rip them apart!",
    "I am immortal", "INSANITY!",
};

static const char *rekkr_skills[] = {
    "Scrapper", "Brawler", "Fighter", "Wrecker", "BERSERKER",
};

static const char *freedoom_skills[] = {
    "Please don't kill me!", "Will this hurt?", "Bring on the pain.",
    "Extreme carnage.", "MAYHEM!",
};

static void MP_GetSkillStrings(mn_item_t *item)
{
    const char **skills = doom_skills;

    switch (gamemission)
    {
        case pack_chex:
        case pack_chex3v:
            skills = chex_skills;
            break;

        case pack_hacx:
            skills = hacx_skills;
            break;

        case pack_rekkr:
            skills = rekkr_skills;
            break;

        case pack_freedoom:
            skills = freedoom_skills;
            break;

        default:
            break;
    }

    item->choices = skills;
    item->num_choices = 5;
}

static const char *game_type_strings[] = {
    "Co-operative", "Deathmatch", "Deathmatch 2.0", "Deathmatch 3.0",
};

static mn_item_t create_items[] = {
    MN_CHOICE("Game Type", "deathmatch", game_type_strings, 15,
              "Game Type:\n\nCo-operative: Play the level together.\n\n"
              "Deathmatch: Fight other players.\n\n"
              "Deathmatch 2.0: Weapons stay, items respawn.\n\n"
              "Deathmatch 3.0: Weapons stay, items respawn, no ammo pickups."),
    {.label = "Skill",
     .cfg = "skill",
     .type = ItemType_Choice,
     .get_choices = MP_GetSkillStrings,
     .choice_size = 18,
     .description = "Skill level for the new game."},
    MN_NUM("Episode", "episode", NULL,
           "Episode to start on (ignored for DOOM II and its add-ons)."),
    MN_NUM("Level Warp", "map", NULL, "Map to start on."),
    MN_GAP(8),
    MN_TITLE("Rules"),
    MN_NUM("Time Limit", "timelimit", "min",
           "Levels will end after this many minutes (0 = No limit)."),
    MN_TOGGLE_INV("Monsters Enabled", "nomonsters",
                  "Monsters Enabled:\n\nSpawn monsters in the level."),
    MN_TOGGLE("Fast Monsters", "fastparm",
              "Fast Monsters:\n\nMonsters move and attack faster."),
    MN_TOGGLE("Respawning Monsters", "respawnparm",
              "Respawning Monsters:\n\nKilled monsters come back to life."),
    MN_GAP(8),
    MN_TITLE("Server"),
    MN_TEXT("Server Name", "servername", MAXPLAYERNAME - 1,
            "Server Name:\n\nName shown in server lists."),
    MN_TOGGLE_INV("Register with Master Server", "privateserver",
                  "Register with Master Server:\n\n"
                  "Make the server publicly visible on the Internet."),
    MN_GAP(8),
    MN_ACTION("Start Game", MP_CreateGame,
              "Start a server with the currently loaded WADs."),
};

static mn_item_t join_items[] = {
    MN_ACTION("Find Server on Local Network", MP_FindLANServers,
              "Search the local network for compatible servers."),
    MN_ACTION("Find Server on Internet", MP_FindInternetServers,
              "Search the master server for compatible servers."),
};

static mn_item_t config_items[] = {
    MN_TEXT("Player Name", "player_name", MAXPLAYERNAME - 1,
            "Player Name:\n\nName shown to other players."),
};

static mn_page_t multiplayer_pages[] = {
    MN_TAB("Create Game", create_items),
    MN_TAB("Join Game", join_items),
    MN_TAB("Configuration", config_items),
};

mn_menu_t multiplayer_menu = {
    .title_patch = "",
    .title_text = "Multiplayer",
    .background = "FLOOR4_6",
    .flags = MN_MENU_HINTS | MN_MENU_CENTER_LAYOUT,
    .layout = MN_LAYOUT,
    .pages = multiplayer_pages,
    .num_pages = arrlen(multiplayer_pages),
};
