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

#include <stdlib.h>
#include <string.h>

#include "d_iwad.h"
#include "d_main.h"
#include "doomdef.h"
#include "doomstat.h"
#include "i_printf.h"
#include "m_config.h"
#include "m_misc.h"
#include "mn_common.h"
#include "mn_container.h"
#include "mn_font.h"
#include "mn_menu.h"
#include "mn_multiplayer.h"
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
    char address[64];
    char description[48];
    int ping;
    int players;
    int max_players;
    int gamemode;
    int gamemission;
    boolean compatible;
} mp_server_t;

char mp_status_text[80] = "";

// Values of the Create Game page, bound to the config file.

static int mp_startskill;
static int mp_startepisode = 1;
static int mp_startmap = 1;
static const char *mp_address;

static mp_server_t servers[MP_MAX_SERVERS];
static int num_servers;
static boolean rebuild_needed;

static boolean query_active;

static int selected_server = -1;

static mp_request_t request;

static mn_item_t server_items[MP_MAX_SERVERS];
static mn_page_t server_page;
static mn_menu_t server_list_menu;

//
// Server list
//

static void RebuildServerItems(void)
{
    // The rows are drawn by ServerDraw(), the labels stay empty so that the
    // generic label layout does not interfere.
    server_page.num_items = num_servers;
}

static void SetAddress(const char *text)
{
    char *copy = M_StringDuplicate(text);

    free((char *)mp_address);
    mp_address = copy;
}

static void UpdateStatus(void)
{
    if (selected_server < 0 || selected_server >= num_servers)
    {
        M_snprintf(mp_status_text, sizeof(mp_status_text), "No server selected.");
        return;
    }

    const mp_server_t *server = &servers[selected_server];

    if (server->compatible)
    {
        M_snprintf(mp_status_text, sizeof(mp_status_text), "IWAD matches: %s",
                   server->description);
    }
    else
    {
        const iwad_t *iwad = NULL;
        const iwad_t **found = D_GetIwads();

        for (int i = 0; found[i]; ++i)
        {
            if (found[i]->mission == server->gamemission)
            {
                iwad = found[i];
                break;
            }
        }

        if (iwad)
        {
            M_snprintf(mp_status_text, sizeof(mp_status_text),
                       "Requires %s: %s", iwad->description,
                       server->description);
        }
        else
        {
            M_snprintf(mp_status_text, sizeof(mp_status_text),
                       "Unknown game: %s", server->description);
        }

        free(found);
    }

    // Keep the line inside the menu box.
    MN_TruncateText(mp_status_text, 400);
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

    byte *cr = ctx->disabled   ? cr_dark
               : ctx->active   ? xlat[CR_GRAY].table
               : ctx->selected ? xlat[CR_BRIGHT].table
                               : xlat[CR_NONE].table;

    char stats[16];
    char ping[16];
    char name[64];
    char addr[64];

    M_snprintf(stats, sizeof(stats), "%i/%i", server->players,
               server->max_players);
    M_snprintf(ping, sizeof(ping), "%ims", server->ping);

    const int ping_w = MN_GetPixelWidth(ping);
    const int stats_x = right - ping_w - 12 - MN_GetPixelWidth(stats);

    // The name comes first, the address fills the space before the numbers.
    const int name_w = MIN((right - left) * 2 / 5, 200);

    M_snprintf(name, sizeof(name), "%s", server->description);
    MN_TruncateText(name, name_w);
    MN_DrawStringCR(left, ctx->y, cr, NULL, name);

    const int addr_x = left + name_w + 12;

    M_StringCopy(addr, server->address, sizeof(addr));
    MN_TruncateText(addr, MAX(0, stats_x - 8 - addr_x));
    MN_DrawString(addr_x, ctx->y, CR_GRAY, addr);

    MN_DrawString(stats_x, ctx->y, CR_GREEN, stats);
    MN_DrawString(right - ping_w, ctx->y, CR_GRAY, ping);
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

    selected_server = index;
    SetAddress(servers[index].address);
    UpdateStatus();
    MN_ContainerPopMenu();
    return true;
}

static const mn_item_ops_t server_ops = {ServerDraw, ServerRespond};

static void ServerPageDraw(const mn_menu_t *menu, const mn_page_t *page)
{
    if (page->num_items > 0)
    {
        return;
    }

    const char *text = query_active ? "Searching for servers..." : "No servers found.";

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

    M_StringCopy(server->address, NW_AddrToString(addr), sizeof(server->address));
    M_StringCopy(server->description, data->description,
                 sizeof(server->description));

    server->ping = ping_time;
    server->players = data->num_players;
    server->max_players = data->max_players;
    server->gamemode = data->gamemode;
    server->gamemission = data->gamemission;
    server->compatible = (data->gamemode == gamemode
                          && data->gamemission == gamemission);

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

static void StartQuery(int mode)
{
    num_servers = 0;
    selected_server = -1;
    rebuild_needed = false;
    query_active = true;

    server_page.num_items = 0;
    server_list_menu.title_text = mode ? "Internet Servers" : "LAN Servers";

    if (mode)
    {
        if (!NW_StartMasterQuery())
        {
            query_active = false;
            M_StringCopy(mp_status_text, "Failed to resolve the master server.",
                         sizeof(mp_status_text));
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
// Requests
//

boolean MP_RequestPending(void)
{
    return request.type != MP_REQUEST_NONE;
}

const mp_request_t *MP_RequestGet(void)
{
    return &request;
}

void MP_RequestClear(void)
{
    memset(&request, 0, sizeof(request));
    request.type = MP_REQUEST_NONE;
}

static void FillCommonRequest(void)
{
    MP_RequestClear();

    request.deathmatch = deathmatch;
    request.timelimit = timelimit;
    request.nomonsters = clnomonsters;
    request.fast = clfastparm;
    request.respawn = clrespawnparm;
    request.port = nw_port;
    request.privateserver = nw_privateserver;
}

void MP_CreateGame(void)
{
    FillCommonRequest();

    request.type = MP_REQUEST_CREATE;
    request.skill = mp_startskill;
    request.episode = mp_startepisode;
    request.map = mp_startmap;

    I_Printf(VB_INFO, "Multiplayer: starting a %s game on port %i",
             deathmatch ? "deathmatch" : "co-operative", nw_port);

    MN_CloseMenu();
}

void MP_JoinGame(void)
{
    if (mp_address == NULL || *mp_address == '\0')
    {
        M_StringCopy(mp_status_text, "Enter a server address.", sizeof(mp_status_text));
        M_StartSound(sfx_mnuerr);
        return;
    }

    // if (selected_server >= 0 && selected_server < num_servers
    //     && !servers[selected_server].compatible)
    // {
    //     UpdateStatus();
    //     M_StartSound(sfx_mnuerr);
    //     return;
    // }

    FillCommonRequest();

    request.type = MP_REQUEST_JOIN;
    M_StringCopy(request.address, mp_address, sizeof(request.address));

    I_Printf(VB_INFO, "Multiplayer: connecting to %s", mp_address);

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
    MN_DisableItem(gamemode == commercial, "startepisode");
}

void MN_BindMultiplayerVariables(void)
{
    M_BindNum("startskill", &mp_startskill, NULL, 2, 0, 4, wad_no,
              "Skill level for the next multiplayer game (0 = Easiest; "
              "4 = Nightmare)");
    M_BindNum("startepisode", &mp_startepisode, NULL, 1, 1, 9, wad_no,
              "Episode to start the next multiplayer game on");
    M_BindNum("startmap", &mp_startmap, NULL, 1, 1, 99, wad_no,
              "Map to start the next multiplayer game on");
    M_BindStr("connect_address", &mp_address, "", wad_no,
              "Address of the server to connect to");
}
