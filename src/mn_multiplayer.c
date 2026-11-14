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

#include "d_englsh.h"
#include "doomdef.h"
#include "doomstat.h"
#include "m_array.h"
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
#include "nw_server.h"
#include "v_palette.h"
#include "v_trans.h"

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

// Column layout of the server table.
enum
{
    PING_COL_W = 5 * 8,
    PLAYERS_COL_W = 8 * 8,  // "16/16"
    COL_GAP = 12
};

typedef struct
{
    int left;
    int right;
    int players_right; // right edge of the players column
    int ping_left;     // left edge of the ping column
} mp_columns_t;

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

static mp_server_t *servers;
static boolean rebuild_needed;
static boolean query_active;

static mn_item_t *server_items; // the first one is the header row
static mn_page_t server_page;
static mn_menu_t server_list_menu;

//
// Config
//

void MN_BindMultiplayerVariables(void)
{
    M_BindStr("player_name", &nw_player_name, DEFAULT_PLAYER_NAME, wad_no,
              "Name shown to other players");
    M_BindStr("servername", &nw_servername, "", wad_no,
              "Name of the server shown in server lists");
    M_BindBool("privateserver", &nw_privateserver, NULL, false, wad_no,
               "Do not register the server with the master server");

    M_BindNum("deathmatch", &mp_deathmatch, NULL, 0, 0, 3, wad_no,
              "Game type for new network games (0 = Co-operative; "
              "1 = Deathmatch; 2 = Deathmatch 2.0; 3 = Deathmatch 3.0)");
    M_BindNum("skill", &mp_skill, NULL, 2, 0, 4, wad_no,
              "Skill level for new network games (0 = Easiest; "
              "4 = Nightmare)");
    M_BindNum("episode", &mp_episode, NULL, 1, 1, 99, wad_no,
              "Episode to start new network games on");
    M_BindNum("map", &mp_map, NULL, 1, 1, 99, wad_no,
              "Map to start new network games on");
    M_BindNum("timelimit", &mp_timelimit, NULL, 0, 0, 99, wad_no,
              "Levels of new network games will end after this many minutes "
              "(0 = No limit)");
    M_BindBool("nomonsters", &mp_nomonsters, NULL, false, wad_no,
               "Disable monsters in new network games");
    M_BindBool("fastparm", &mp_fastparm, NULL, false, wad_no,
               "Fast monsters in new network games");
    M_BindBool("respawnparm", &mp_respawnparm, NULL, false, wad_no,
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

static void ServerDraw(const mn_item_t *item, const mn_draw_ctx_t *ctx);
static boolean ServerRespond(mn_item_t *item, menu_action_t action, int ch);

static const mn_item_ops_t server_ops = {ServerDraw, ServerRespond};

static mp_columns_t GetColumns(int left, int right)
{
    const mp_columns_t columns = {
        .left = left,
        .right = right,
        .players_right = right - PING_COL_W - COL_GAP,
        .ping_left = right - PING_COL_W,
    };

    return columns;
}

static void HeaderDraw(const mn_item_t *item, const mn_draw_ctx_t *ctx)
{
    const mp_columns_t columns =
        GetColumns(ctx->x, ctx->value_x + ctx->value_w);

    const char *lbl = "Server";
    MN_DrawString(columns.left, ctx->y, CR_GOLD, lbl);
    lbl = "Players";
    MN_DrawString(columns.players_right - MN_GetPixelWidth(lbl), ctx->y,
                  CR_GOLD, lbl);
    lbl = "Ping";
    MN_DrawString(columns.right - MN_GetPixelWidth(lbl), ctx->y, CR_GOLD, lbl);
}

static const mn_item_ops_t header_ops = {HeaderDraw};

static void RebuildServerItems(void)
{
    const int count = array_size(servers);

    // The rows are drawn by ServerDraw(), the labels stay empty so that the
    // generic label layout does not interfere.
    while (array_size(server_items) < count + 1)
    {
        const mn_item_t row = {
            .label = "",
            .type = ItemType_Action,
            .ops = &server_ops,
            .height = 2 * MN_SPC,
        };

        array_push(server_items, row);
    }

    server_page.items = server_items;
    server_page.num_items = count + 1;
}

// Frees the rows and puts the header back.
static void ResetServerItems(void)
{
    array_free(server_items);

    const mn_item_t header = {
        .label = "",
        .type = ItemType_Title,
        .ops = &header_ops,
        .height = MN_SPC + 2
    };

    array_push(server_items, header);

    server_page.items = server_items;
    server_page.num_items = 1;
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
    const int index = (int)(item - server_items) - 1;

    if (index < 0 || index >= array_size(servers))
    {
        return;
    }

    const mp_server_t *server = &servers[index];

    const mp_columns_t columns =
        GetColumns(ctx->x, ctx->value_x + ctx->value_w);

    char stats[16];
    char ping[16];
    char name[64];
    char addr[MAX_ADDRESS];

    M_snprintf(stats, sizeof(stats), "%i/%i", server->players,
               server->max_players);
    M_snprintf(ping, sizeof(ping), "%i", server->ping);

    M_snprintf(name, sizeof(name), "%s", server->description);
    MN_TruncateText(name, columns.players_right - 8 - columns.left);
    MN_DrawStringCR(columns.left, ctx->y, ItemColor(ctx), NULL, name);

    MN_DrawString(columns.players_right - MN_GetPixelWidth(stats), ctx->y,
                  CR_GREEN, stats);
    MN_DrawString(columns.right - MN_GetPixelWidth(ping), ctx->y, CR_GRAY,
                  ping);

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
    MN_TruncateText(addr, columns.right - columns.left);
    MN_DrawString(columns.left, ctx->y + MN_SPC, CR_GRAY, addr);
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

    const int index = (int)(item - server_items) - 1;

    if (index < 0 || index >= array_size(servers))
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

static void ServerPageDraw(const mn_menu_t *menu, const mn_page_t *page)
{
    // The header row is always there.
    if (page->num_items > 1)
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
    if (data->protocol == NW_PROTOCOL_UNKNOWN)
    {
        return;
    }

    mp_server_t server = {0};

    M_StringCopy(server.address, NW_AddrToString(addr),
                 sizeof(server.address));
    M_StringCopy(server.description, data->description,
                 sizeof(server.description));

    server.ping = ping_time;
    server.players = data->num_players;
    server.max_players = data->max_players;
    server.gamemode = data->gamemode;
    server.gamemission = data->gamemission;

    // A server with no players reports an undetermined game mode: it has
    // not adopted a game yet and takes the mode and mission of the first
    // client that connects.
    server.compatible =
        data->num_players == 0 || data->gamemode == indetermined
        || (data->gamemode == gamemode && data->gamemission == gamemission);

    array_push(servers, server);
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
    array_clear(servers);
    ResetServerItems();

    rebuild_needed = false;
    query_active = true;

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
    ResetServerItems();

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
    MN_DisableItem(gamemode == commercial, "episode");
}
