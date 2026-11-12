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
//  Starting a network game requested from the multiplayer menu, and the
//  in-game waiting screen (there is no textscreen here).
//

#include "d_loop.h"
#include "d_main.h"
#include "doomdef.h"
#include "doomstat.h"
#include "g_game.h"
#include "i_printf.h"
#include "i_timer.h"
#include "i_video.h"
#include "m_misc.h"
#include "mn_container.h"
#include "mn_font.h"
#include "mn_menu.h"
#include "mn_multiplayer.h"
#include "mn_options.h"
#include "mp_start.h"
#include "nw_client.h"
#include "nw_defs.h"
#include "nw_sdl.h"
#include "nw_server.h"
#include "v_palette.h"
#include "v_trans.h"
#include "v_video.h"

#define MP_NOTICE_TICS (2 * TICRATE)

// Connect to the given address, or start a server when server is true.
// Unlike the command line startup, failures are not fatal: the client is
// left disconnected and false is returned. On success the client waits for
// the launch message while the main loop keeps running.

boolean D_InitNetGameFromMenu(boolean server, const char *address);

void D_CheckNetGame(void);

static boolean waiting;
static boolean is_server;
static boolean launch_sent;
static boolean reopen_menu;

static char wait_message[128];

static char notice[128];
static int notice_until;

//
// State
//

boolean MP_WaitScreenActive(void)
{
    return waiting;
}

static void MP_Notice(const char *text)
{
    M_StringCopy(notice, text, sizeof(notice));
    notice_until = I_GetTime() + MP_NOTICE_TICS;
}

// Back to the title screen, dropping whatever was running before.
static void MP_BackToTitle(void)
{
    G_ClearInput();
    paused = false;
    gameaction = ga_nothing;
    automapactive = false;
    usergame = false;
    D_StartTitle();
}

// Return to the Multiplayer menu, so that the settings can be fixed up and
// the attempt repeated.
static void MP_ReopenMenu(void)
{
    MN_StartControlPanel();
    MN_OptionsOpen();
    MN_ContainerPushMenu(&multiplayer_menu);
}

//
// Drawing
//

static void DrawCentered(int y, int color, const char *text)
{
    MN_DrawString((SCREENWIDTH - MN_GetPixelWidth(text)) / 2, y, color, text);
}

// A single status line on the menu backdrop, drawn before operations that
// may take a while (connecting, leaving).
static void DrawStatusScreen(const char *text)
{
    V_DrawBackground("FLOOR4_6");
    DrawCentered(SCREENHEIGHT / 2, CR_GRAY, text);
}

static void DrawWaitScreen(void)
{
    V_DrawBackground("FLOOR4_6");

    int y = SCREENHEIGHT / 8;

    DrawCentered(y, CR_GOLD, is_server ? "Starting a network game"
                                       : "Waiting for the game to start");

    y += 3 * MN_SPC;

    if (!nw_client_received_wait_data)
    {
        DrawCentered(y, CR_GRAY, wait_message);
        DrawCentered(SCREENHEIGHT - 40, CR_GRAY, "[ Esc ] Cancel");
        return;
    }

    const nw_waitdata_t *data = &nw_client_wait_data;

    const int total = MIN(data->num_players + data->num_drones,
                          (int)arrlen(data->player_names));

    const int list_width = 360;
    const int left = (SCREENWIDTH - list_width) / 2;

    for (int i = 0; i < total; ++i)
    {
        char buffer[64];
        char line[128];

        if (i < data->num_players)
        {
            const boolean ready = i < data->ready_players;

            M_snprintf(line, sizeof(line), "%i. %s", i + 1,
                       data->player_names[i]);
            M_snprintf(buffer, sizeof(buffer), "%s  %s", data->player_addrs[i],
                       ready ? "Ready" : "");
        }
        else
        {
            M_snprintf(line, sizeof(line), "%i. Drone", i + 1);
            buffer[0] = '\0';
        }

        MN_DrawString(left, y, CR_NONE, line);

        if (buffer[0])
        {
            const int width = MN_GetPixelWidth(buffer);
            MN_DrawString(left + list_width - width, y,
                          (i < data->ready_players) ? CR_GREEN : CR_GRAY,
                          buffer);
        }

        y += MN_SPC + 2;
    }

    y += MN_SPC;

    if (data->is_controller)
    {
        DrawCentered(y, CR_GOLD, "Press [ Enter ] to start the game");
    }
    else
    {
        DrawCentered(y, CR_GRAY, "Waiting for the host to start the game");
    }

    DrawCentered(SCREENHEIGHT - 40, CR_GRAY, "[ Esc ] Cancel");
}

void MP_DrawOverlay(void)
{
    if (waiting)
    {
        DrawWaitScreen();
    }
    else if (notice_until > I_GetTime())
    {
        MN_DrawNotification(notice, CR_RED, false);
    }
}

//
// Starting
//

static void ApplyRequestSettings(const mp_request_t *request)
{
    deathmatch = request->deathmatch;
    startskill = request->skill;
    startepisode = request->episode;
    startmap = request->map;
    timelimit = request->timelimit;
    nomonsters = request->nomonsters;
    fastparm = request->fast;
    respawnparm = request->respawn;
}

static void MP_StopNetwork(void)
{
    if (nw_client_connected)
    {
        NW_CL_Disconnect();
    }

    if (is_server)
    {
        NW_SV_Shutdown();
    }

    NW_ResetSocket();
}

static void MP_BeginStart(const mp_request_t *request)
{
    is_server = (request->type == MP_REQUEST_CREATE);
    launch_sent = false;

    if (is_server)
    {
        ApplyRequestSettings(request);

        // The client socket may already be bound (server search); the server
        // has to listen on the configured port.
        NW_ResetSocket();
    }

    MP_BackToTitle();

    nw_client_received_wait_data = false;
    nw_waiting_for_launch = false;

    M_StringCopy(wait_message,
                 is_server ? "Starting the server..." : "Connecting to the server...",
                 sizeof(wait_message));

    waiting = true;

    if (!nodrawers)
    {
        MP_DrawOverlay();
        I_FinishUpdate();
    }

    if (!D_InitNetGameFromMenu(is_server,
                               is_server ? NULL : request->address))
    {
        waiting = false;

        MP_StopNetwork();

        reopen_menu = true;
        MP_Notice(is_server ? "Could not start the server."
                            : "Could not connect to the server.");
        return;
    }

    I_Printf(VB_INFO, "Multiplayer: connected, waiting for the game to start");

    // NW_CL_Run() maintains this flag, but only refreshes it once a frame
    // has been run, and the launch check happens first.
    nw_waiting_for_launch = true;
}

// The launch message is in, so the network game can begin.
static void MP_FinishStart(void)
{
    // The server numbers its tics from zero, so the local clock has to
    // start over too.
    D_ResetTicState();

    netgame = true;

    D_CheckNetGame();

    // The title screen may have deferred a demo and asked for the next one.
    advancedemo = false;
    gameaction = ga_nothing;

    G_ClearInput();
    G_InitNew(startskill, startepisode, startmap, false);
    D_StartGameLoop();

    I_Printf(VB_INFO, "Multiplayer: game started");
}

static void MP_Abort(void)
{
    waiting = false;

    // Disconnecting may wait for the other side, so show something first.
    if (!nodrawers)
    {
        DrawStatusScreen("Leaving the game...");
        I_FinishUpdate();
    }

    MP_StopNetwork();

    MP_BackToTitle();

    reopen_menu = true;
    MP_Notice("Network game cancelled.");
}

//
// Main loop hooks
//

void MP_StartTick(void)
{
    if (reopen_menu)
    {
        reopen_menu = false;
        MP_ReopenMenu();
        return;
    }

    if (waiting)
    {
        if (!nw_client_connected)
        {
            waiting = false;
            MP_StopNetwork();
            MP_BackToTitle();
            reopen_menu = true;
            MP_Notice("Lost connection to the server.");
            return;
        }

        if (!nw_waiting_for_launch)
        {
            waiting = false;
            MP_FinishStart();
        }

        return;
    }

    if (!MP_RequestPending())
    {
        return;
    }

    mp_request_t request = *MP_RequestGet();
    MP_RequestClear();

    MP_BeginStart(&request);
}

boolean MP_Responder(menu_action_t action)
{
    if (!waiting)
    {
        return false;
    }

    if (action == MENU_ESCAPE)
    {
        MP_Abort();
        return true;
    }

    if (action == MENU_ENTER && nw_client_received_wait_data
        && nw_client_wait_data.is_controller && !launch_sent)
    {
        NW_CL_LaunchGame();
        launch_sent = true;
        return true;
    }

    // Swallow the remaining menu keys while the wait screen is up.
    return true;
}
