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
//
// DESCRIPTION:
//  In-game multiplayer menu: create and join network games.
//

#include <string.h>

#include "d_englsh.h"
#include "d_main.h"
#include "doomdef.h"
#include "doomstat.h"
#include "m_config.h"
#include "m_misc.h"
#include "mn_common.h"
#include "mn_container.h"
#include "mn_font.h"
#include "mn_menu.h"
#include "mn_multiplayer.h"
#include "mp_start.h"
#include "nw_client.h"
#include "nw_defs.h"
#include "nw_io.h"
#include "nw_query.h"
#include "nw_sdl.h"
#include "nw_server.h"
#include "sounds.h"
#include "v_palette.h"
#include "v_trans.h"

#define MP_MAX_SERVERS 16

typedef struct
{
    char address[MAX_ADDRESS];
    char description[48];
    int ping;
    int players;
    int max_players;
    int gamemode;
    int gamemission;
    boolean compatible;
} mp_server_t;

// Create Game settings, bound to the config file. They are copied into the
// game globals only when a network game is created, so that they cannot
// affect single player games.

static int mp_deathmatch;
static int mp_skill = 2;
static int mp_episode = 1;
static int mp_map = 1;
static int mp_timelimit;
static boolean mp_nomonsters;
static boolean mp_fastparm;
static boolean mp_respawnparm;

static mp_server_t servers[MP_MAX_SERVERS];
static int num_servers;
static boolean rebuild_needed;
static boolean query_active;

static mn_item_t server_items[MP_MAX_SERVERS];
static mn_page_t server_page;
static mn_menu_t server_list_menu;

//
// Config
//

void MN_BindMultiplayerVariables(void)
{
    M_BindStr("nw_player_name", &nw_player_name, DEFAULT_PLAYER_NAME, wad_no,
              "Name shown to other players");
    M_BindStr("servername", &nw_servername, "", wad_no,
              "Name of the server shown in server lists");
    M_BindBool("privateserver", &nw_privateserver, NULL, false, wad_no,
               "Do not register the server with the master server");

    M_BindNum("mp_deathmatch", &mp_deathmatch, NULL, 0, 0, 3, wad_no,
              "Game type for new network games (0 = Co-operative; "
              "1 = Deathmatch; 2 = Deathmatch 2.0; 3 = Deathmatch 3.0)");
    M_BindNum("mp_skill", &mp_skill, NULL, 2, 0, 4, wad_no,
              "Skill level for new network games (0 = Easiest; "
              "4 = Nightmare)");
    M_BindNum("mp_episode", &mp_episode, NULL, 1, 1, 9, wad_no,
              "Episode to start new network games on");
    M_BindNum("mp_map", &mp_map, NULL, 1, 1, 99, wad_no,
              "Map to start new network games on");
    M_BindNum("mp_timelimit", &mp_timelimit, NULL, 0, 0, 99, wad_no,
              "Levels of new network games will end after this many minutes "
              "(0 = No limit)");
    M_BindBool("mp_nomonsters", &mp_nomonsters, NULL, false, wad_no,
               "Disable monsters in new network games");
    M_BindBool("mp_fastparm", &mp_fastparm, NULL, false, wad_no,
               "Fast monsters in new network games");
    M_BindBool("mp_respawnparm", &mp_respawnparm, NULL, false, wad_no,
               "Respawning monsters in new network games");
}

void MP_ApplyCreateSettings(void)
{
    deathmatch = mp_deathmatch;
    startskill = mp_skill;
    startepisode = mp_episode;
    startmap = mp_map;
    timelimit = mp_timelimit;
    nomonsters = mp_nomonsters;
    fastparm = mp_fastparm;
    respawnparm = mp_respawnparm;
}

//
// Server list
//

static void RebuildServerItems(void)
{
    // The rows are drawn by ServerDraw(), the labels stay empty so that the
    // generic label layout does not interfere.
    server_page.num_items = num_servers;
}

static const char *MissionName(int mission)
{
    switch (mission)
    {
        case doom:
            return "DOOM";
        case doom2:
            return "DOOM II";
        case pack_tnt:
            return "TNT: Evilution";
        case pack_plut:
            return "The Plutonia Experiment";
        case pack_chex:
            return "Chex Quest";
        case pack_hacx:
            return "Hacx";
        case pack_rekkr:
            return "Rekkr";
        case pack_freedoom:
            return "Freedoom";
        default:
            return "another game";
    }
}

static byte *ItemColor(const mn_draw_ctx_t *ctx)
{
    return ctx->disabled   ? cr_dark
           : ctx->active   ? xlat[CR_GRAY].table
           : ctx->selected ? xlat[CR_BRIGHT].table
                           : xlat[CR_NONE].table;
}

static void ServerDraw(const mn_item_t *item, const mn_draw_ctx_t *ctx)
{
    const int index = item - server_items;

    if (index < 0 || index >= num_servers)
    {
        return;
    }

    const mp_server_t *server = &servers[index];

    const int left = ctx->x;
    const int right = ctx->value_x + ctx->value_w;

    char stats[16];
    char ping[16];
    char name[64];
    char addr[MAX_ADDRESS];

    M_snprintf(stats, sizeof(stats), "%i/%i", server->players,
               server->max_players);
    M_snprintf(ping, sizeof(ping), "%ims", server->ping);

    const int ping_w = MN_GetPixelWidth(ping);
    const int stats_x = right - ping_w - 12 - MN_GetPixelWidth(stats);

    M_snprintf(name, sizeof(name), "%s", server->description);
    MN_TruncateText(name, stats_x - 12 - left);
    MN_DrawStringCR(left, ctx->y, ItemColor(ctx), NULL, name);

    MN_DrawString(stats_x, ctx->y, CR_GREEN, stats);
    MN_DrawString(right - ping_w, ctx->y, CR_GRAY, ping);

    // The address goes on its own line: an IPv6 address does not fit next
    // to the name. The game is appended, so that a server running another
    // IWAD can be told apart before joining it. An idle server has not
    // adopted a game yet and reports no meaningful mission.
    char game[32] = "";

    if (server->gamemode != indetermined)
    {
        M_snprintf(game, sizeof(game), "  %s",
                   MissionName(server->gamemission));
    }

    M_snprintf(addr, sizeof(addr), "%s%s", server->address, game);
    MN_TruncateText(addr, right - left);
    MN_DrawString(left, ctx->y + MN_SPC, CR_GRAY, addr);
}

static void ShowMismatch(const mp_server_t *server)
{
    static char message[160];

    M_snprintf(message, sizeof(message),
               "This server runs a different game:\n\n%s\n\n"
               "You are running %s.\n\n" PRESSKEY,
               MissionName(server->gamemission), MissionName(gamemission));

    M_StartMessage(message, NULL, false);
}

static boolean ServerRespond(mn_item_t *item, menu_action_t action, int ch)
{
    if (action != MENU_ENTER)
    {
        return false;
    }

    const int index = item - server_items;

    if (index < 0 || index >= num_servers)
    {
        return false;
    }

    if (!servers[index].compatible)
    {
        ShowMismatch(&servers[index]);
        return true;
    }

    MP_RequestJoin(servers[index].address);
    MN_CloseMenu();
    return true;
}

static const mn_item_ops_t server_ops = {ServerDraw, ServerRespond};

static void ServerPageDraw(const mn_menu_t *menu, const mn_page_t *page)
{
    if (page->num_items > 0)
    {
        return;
    }

    const char *text =
        query_active ? "Searching for servers..." : "No servers found.";

    MN_DrawString((SCREENWIDTH - MN_GetPixelWidth(text)) / 2,
                  menu->viewport.y + menu->viewport.h / 2, CR_GRAY, text);
}

static void QueryCallback(nw_addr_t *addr, nw_querydata_t *data,
                          unsigned int ping_time, void *user_data)
{
    if (data->protocol == NW_PROTOCOL_UNKNOWN || num_servers >= MP_MAX_SERVERS)
    {
        return;
    }

    mp_server_t *server = &servers[num_servers];

    M_StringCopy(server->address, NW_AddrToString(addr),
                 sizeof(server->address));
    M_StringCopy(server->description, data->description,
                 sizeof(server->description));

    server->ping = ping_time;
    server->players = data->num_players;
    server->max_players = data->max_players;
    server->gamemode = data->gamemode;
    server->gamemission = data->gamemission;

    // A server with no players reports an undetermined game mode: it has
    // not adopted a game yet and takes the mode and mission of the first
    // client that connects. Requiring an exact match here would reject
    // every idle server, which is what most servers in the internet list
    // are.
    server->compatible =
        data->num_players == 0 || data->gamemode == indetermined
        || (data->gamemode == gamemode && data->gamemission == gamemission);

    ++num_servers;
    rebuild_needed = true;
}

static void ServerListTicker(mn_menu_t *menu)
{
    if (query_active)
    {
        if (!NW_Query_Poll(QueryCallback, NULL))
        {
            query_active = false;
            rebuild_needed = true;
        }
    }

    if (rebuild_needed)
    {
        RebuildServerItems();
        MN_ContainerRelayout();
        rebuild_needed = false;
    }
}

static void StartQuery(int internet)
{
    num_servers = 0;
    rebuild_needed = false;
    query_active = true;

    server_page.num_items = 0;
    server_list_menu.title_text = internet ? "Internet Servers" : "LAN Servers";

    if (internet)
    {
        if (!NW_StartMasterQuery())
        {
            query_active = false;
            M_StartMessage("Failed to resolve the master server.\n\n" PRESSKEY,
                           NULL, false);
            return;
        }
    }
    else
    {
        NW_StartLANQuery();
    }

    MN_ContainerPushMenu(&server_list_menu);
}

void MP_FindLANServers(void)
{
    StartQuery(0);
}

void MP_FindInternetServers(void)
{
    StartQuery(1);
}

//
// Actions
//

void MP_CreateGame(void)
{
    MP_RequestCreate();
    MN_CloseMenu();
}

//
// Init
//

void MN_MultiplayerInit(void)
{
    for (int i = 0; i < MP_MAX_SERVERS; ++i)
    {
        server_items[i] = (mn_item_t){
            .label = "",
            .type = ItemType_Action,
            .ops = &server_ops,
            .height = 2 * MN_SPC,
        };
    }

    server_page.items = server_items;
    server_page.num_items = 0;
    server_page.draw = ServerPageDraw;

    server_list_menu.title_patch = "";
    server_list_menu.title_text = "Servers";
    server_list_menu.background = "FLOOR4_6";
    server_list_menu.flags = MN_MENU_CENTER_LAYOUT;
    server_list_menu.layout = (mn_layout_t){.max_width = MN_PX(500),
                                            .items_min_width = MN_PX(250)};
    server_list_menu.pages = &server_page;
    server_list_menu.num_pages = 1;
    server_list_menu.ticker = ServerListTicker;

    // Episode only exists in the original DOOM.
    MN_DisableItem(gamemode == commercial, "mp_episode");
}
