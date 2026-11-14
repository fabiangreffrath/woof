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
//  in-game waiting screen.
//

#include "d_loop.h"
#include "d_main.h"
#include "d_net.h"
#include "doomdef.h"
#include "doomstat.h"
#include "g_game.h"
#include "i_printf.h"
#include "i_video.h"
#include "m_argv.h"
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

static boolean waiting;
static boolean is_server;
static boolean launch_sent;
static boolean reopen_menu;
static boolean start_wipe_pending;
static boolean fail_pending;

static char wait_message[128];
static char fail_message[128];

static mp_request_t request;

static void DrawStatusScreen(const char *text);

//
// Requests
//

void MP_RequestCreate(void)
{
    request = (mp_request_t){.type = MP_REQUEST_CREATE};
}

void MP_RequestJoin(const char *address)
{
    request = (mp_request_t){.type = MP_REQUEST_JOIN};
    M_StringCopy(request.address, address, sizeof(request.address));
}

void MP_RequestSingleGame(int skill, int episode, int map)
{
    request = (mp_request_t){.type = MP_REQUEST_SINGLE, .skill = skill,
                             .episode = episode, .map = map};
}

void MP_RequestLeave(void)
{
    request = (mp_request_t){.type = MP_REQUEST_LEAVE};
}

boolean MP_RequestPending(void)
{
    return request.type != MP_REQUEST_NONE;
}

void MP_RequestClear(void)
{
    request = (mp_request_t){0};
}

//
// State
//

boolean MP_WaitScreenActive(void)
{
    return waiting;
}

void MP_WaitForLaunch(void)
{
    is_server = M_CheckParm("-server") > 0 || M_CheckParm("-privateserver") > 0;
    launch_sent = false;

    nw_client_received_wait_data = false;
    nw_waiting_for_launch = true;

    waiting = true;
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

// Report a failure and go back to the menu: the message is shown once the
// menu is up, so that dismissing it keeps the menu open.

static void MP_Fail(const char *text)
{
    M_StringCopy(fail_message, text, sizeof(fail_message));
    fail_pending = true;
    reopen_menu = true;
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

    is_server = false;
}

// Leave the current network game, if any.

static void MP_LeaveNetGame(void)
{
    if (!netgame && !nw_client_connected && !is_server)
    {
        return;
    }

    // Disconnecting may wait for the other side, so show something first.
    if (!nodrawers)
    {
        DrawStatusScreen("Leaving the network game...");
        I_FinishUpdate();
    }

    MP_StopNetwork();

    netgame = false;
    solonet = false;
    deathmatch = 0;
    timelimit = 0;
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
        char line[128];
        char addr[MAX_ADDRESS];

        if (i < data->num_players)
        {
            M_snprintf(line, sizeof(line), "%i. %s", i + 1,
                       data->player_names[i]);
            M_StringCopy(addr, data->player_addrs[i], sizeof(addr));
        }
        else
        {
            M_snprintf(line, sizeof(line), "%i. Drone", i + 1);
            addr[0] = '\0';
        }

        // The name is green once the player is ready.

        MN_DrawString(left, y,
                      (i < data->ready_players) ? CR_GREEN : CR_NONE, line);

        if (addr[0] != '\0')
        {
            MN_TruncateText(addr, list_width - 120);
            MN_DrawString(left + list_width - MN_GetPixelWidth(addr), y,
                          CR_GRAY, addr);
        }

        y += MN_SPC + 2;
    }

    y += MN_SPC;

    DrawCentered(y, data->is_controller ? CR_GOLD : CR_GRAY,
                 data->is_controller ? "Press [ Enter ] to start the game"
                                     : "Waiting for the host to start the game");

    DrawCentered(SCREENHEIGHT - 40, CR_GRAY, "[ Esc ] Cancel");
}

void MP_DrawWaitScreen(void)
{
    if (waiting)
    {
        DrawWaitScreen();
    }
}

//
// Starting
//

static void MP_BeginStart(const mp_request_t *pending)
{
    // Leave the current game first: MP_LeaveNetGame() needs to know whether
    // this client is also the server of the game being left.

    MP_LeaveNetGame();

    is_server = (pending->type == MP_REQUEST_CREATE);
    launch_sent = false;

    if (is_server)
    {
        MP_ApplyCreateSettings();

        // The client socket may already be bound (server search); the server
        // has to listen on its own port.
        NW_ResetSocket();
    }

    MP_BackToTitle();

    nw_client_received_wait_data = false;
    nw_waiting_for_launch = false;

    M_StringCopy(wait_message,
                 is_server ? "Starting the server..."
                           : "Connecting to the server...",
                 sizeof(wait_message));

    waiting = true;

    if (!nodrawers)
    {
        MP_DrawWaitScreen();
        I_FinishUpdate();
    }

    if (!D_InitNetGameFromMenu(is_server,
                               is_server ? NULL : pending->address))
    {
        waiting = false;
        MP_StopNetwork();
        MP_Fail(is_server ? "Could not start the server."
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

    // Players are spawned by the first tic, and the view is not rendered
    // while gametic is zero, so the level would not be drawn for the start
    // wipe without this.
    D_RunFirstTic();

    D_StartGameLoop();

    // The wipe freezes the loop for a while; the tic clock is resynced
    // afterwards, otherwise all that time is replayed as a burst of tics
    // and the level starts in the middle of the weapon raise.
    start_wipe_pending = true;

    I_Printf(VB_INFO, "Multiplayer: game started");
}

void MP_AfterStartWipe(void)
{
    if (start_wipe_pending)
    {
        start_wipe_pending = false;
        D_StartGameLoop();
    }
}

static void MP_Abort(void)
{
    waiting = false;

    MP_LeaveNetGame();

    MP_BackToTitle();

    reopen_menu = true;
}

//
// Main loop hooks
//

void MP_StartTick(void)
{
    // A start wipe always happens right after a request, so a pending flag
    // here means that the wipe was skipped.
    start_wipe_pending = false;

    if (reopen_menu)
    {
        reopen_menu = false;
        MP_ReopenMenu();

        if (fail_pending)
        {
            fail_pending = false;
            M_StartMessage(fail_message, NULL, false);
        }

        return;
    }

    if (waiting)
    {
        if (!nw_client_connected)
        {
            waiting = false;
            MP_LeaveNetGame();
            MP_BackToTitle();
            MP_Fail("Lost connection to the server.");
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

    mp_request_t pending = request;
    MP_RequestClear();

    switch (pending.type)
    {
        case MP_REQUEST_LEAVE:
            MP_LeaveNetGame();
            MP_BackToTitle();
            break;

        case MP_REQUEST_SINGLE:
            MP_LeaveNetGame();
            G_DeferedInitNew(pending.skill, pending.episode, pending.map);
            break;

        case MP_REQUEST_CREATE:
        case MP_REQUEST_JOIN:
            MP_BeginStart(&pending);
            break;

        default:
            break;
    }
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
