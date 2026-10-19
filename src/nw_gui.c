//
// Copyright(C) 2005-2014 Simon Howard
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// Graphical stuff related to the networking code:
//
//  * The client waiting screen when we are waiting for the server to
//    start the game.
//

#include <stdio.h>
#include <stdlib.h>

#include "config.h"
#include "doomkeys.h"
#include "doomtype.h"
#include "i_exit.h"
#include "i_printf.h"
#include "i_system.h"
#include "i_video.h"
#include "m_argv.h"
#include "m_misc.h"
#include "nw_client.h"
#include "nw_defs.h"
#include "nw_gui.h"
#include "nw_query.h"
#include "nw_server.h"

#include "textscreen.h"

static txt_window_t *window;
static int old_max_players;
static txt_label_t *player_labels[NW_MAXPLAYERS];
static txt_label_t *ip_labels[NW_MAXPLAYERS];
static txt_label_t *drone_label;
static txt_label_t *master_msg_label;
static boolean had_warning;

// Number of players we expect to be in the game. When the number is
// reached, we auto-start the game (if we're the controller). If
// zero, do not autostart.
static int expected_nodes;

static void EscapePressed(TXT_UNCAST_ARG(widget), void *unused)
{
    TXT_Shutdown();
    I_SafeExit(0);
}

static void StartGame(TXT_UNCAST_ARG(widget), TXT_UNCAST_ARG(unused))
{
    NW_CL_LaunchGame();
}

static void OpenWaitDialog(void)
{
    txt_window_action_t *cancel;

    char *title =
        M_StringJoin(PROJECT_STRING, ": Waiting for game start...");

    TXT_SetDesktopTitle(title);

    free(title);

    window = TXT_NewWindow("Waiting for game start...");

    TXT_AddWidget(window, TXT_NewLabel("\nPlease wait...\n\n"));

    cancel = TXT_NewWindowAction(KEY_ESCAPE, "Cancel");
    TXT_SignalConnect(cancel, "pressed", EscapePressed, NULL);

    TXT_SetWindowAction(window, TXT_HORIZ_LEFT, cancel);
    TXT_SetWindowPosition(window, TXT_HORIZ_CENTER, TXT_VERT_BOTTOM,
                          TXT_SCREEN_W / 2, TXT_SCREEN_H - 9);

    old_max_players = 0;
}

static void BuildWindow(void)
{
    char buf[50];
    txt_table_t *table;
    int i;

    TXT_ClearTable(window);
    table = TXT_NewTable(3);
    TXT_AddWidget(window, table);

    // Add spacers

    TXT_AddWidget(table, NULL);
    TXT_AddWidget(table, TXT_NewStrut(25, 1));
    TXT_AddWidget(table, TXT_NewStrut(17, 1));

    // Player labels

    for (i = 0; i < nw_client_wait_data.max_players; ++i)
    {
        M_snprintf(buf, sizeof(buf), " %i. ", i + 1);
        TXT_AddWidget(table, TXT_NewLabel(buf));
        player_labels[i] = TXT_NewLabel("");
        ip_labels[i] = TXT_NewLabel("");
        TXT_AddWidget(table, player_labels[i]);
        TXT_AddWidget(table, ip_labels[i]);
    }

    drone_label = TXT_NewLabel("");

    TXT_AddWidget(window, drone_label);
}

static void UpdateGUI(void)
{
    txt_window_action_t *startgame;
    char buf[50];
    unsigned int i;

    // If the value of max_players changes, we must rebuild the
    // contents of the window. This includes when the first
    // waiting data packet is received.

    if (nw_client_received_wait_data)
    {
        if (nw_client_wait_data.max_players != old_max_players)
        {
            BuildWindow();
        }
    }
    else
    {
        return;
    }

    for (i = 0; i < nw_client_wait_data.max_players; ++i)
    {
        txt_color_t color = TXT_COLOR_BRIGHT_WHITE;

        if ((signed)i == nw_client_wait_data.consoleplayer)
        {
            color = TXT_COLOR_YELLOW;
        }

        TXT_SetFGColor(player_labels[i], color);
        TXT_SetFGColor(ip_labels[i], color);

        if (i < nw_client_wait_data.num_players)
        {
            TXT_SetLabel(player_labels[i],
                         nw_client_wait_data.player_names[i]);
            TXT_SetLabel(ip_labels[i], nw_client_wait_data.player_addrs[i]);
        }
        else
        {
            TXT_SetLabel(player_labels[i], "");
            TXT_SetLabel(ip_labels[i], "");
        }
    }

    if (nw_client_wait_data.num_drones > 0)
    {
        M_snprintf(buf, sizeof(buf), " (+%i observer clients)",
                   nw_client_wait_data.num_drones);
        TXT_SetLabel(drone_label, buf);
    }
    else
    {
        TXT_SetLabel(drone_label, "");
    }

    if (nw_client_wait_data.is_controller)
    {
        startgame = TXT_NewWindowAction(' ', "Start game");
        TXT_SignalConnect(startgame, "pressed", StartGame, NULL);
    }
    else
    {
        startgame = NULL;
    }

    TXT_SetWindowAction(window, TXT_HORIZ_RIGHT, startgame);
}

static void BuildMasterStatusWindow(void)
{
    txt_window_t *master_window;

    master_window = TXT_NewWindow(NULL);
    master_msg_label = TXT_NewLabel("");
    TXT_AddWidget(master_window, master_msg_label);

    // This window is here purely for information, so it should be
    // in the background.

    TXT_LowerWindow(master_window);
    TXT_SetWindowPosition(master_window, TXT_HORIZ_CENTER, TXT_VERT_CENTER,
                          TXT_SCREEN_W / 2, TXT_SCREEN_H - 4);
    TXT_SetWindowAction(master_window, TXT_HORIZ_LEFT, NULL);
    TXT_SetWindowAction(master_window, TXT_HORIZ_CENTER, NULL);
    TXT_SetWindowAction(master_window, TXT_HORIZ_RIGHT, NULL);
}

static void CheckMasterStatus(void)
{
    boolean added;

    if (!NW_Query_CheckAddedToMaster(&added))
    {
        return;
    }

    if (master_msg_label == NULL)
    {
        BuildMasterStatusWindow();
    }

    if (added)
    {
        TXT_SetLabel(
            master_msg_label,
            "Your server is now registered with the global master server.\n"
            "Other players can find your server online.");
    }
    else
    {
        TXT_SetLabel(
            master_msg_label,
            "Failed to register with the master server. Your server is not\n"
            "publicly accessible. You may need to reconfigure your Internet\n"
            "router to add a port forward for UDP port 2342. Look up\n"
            "information on port forwarding online.");
    }
}

static void ParseCommandLineArgs(void)
{
    int i;

    //!
    // @arg <n>
    // @category net
    //
    // Autostart the netgame when n nodes (clients) have joined the server.
    //

    i = M_CheckParmWithArgs("-nodes", 1);
    if (i > 0)
    {
        expected_nodes = M_ParmArgToInt(i);
    }
}

static void CheckAutoLaunch(void)
{
    int nodes;

    if (nw_client_received_wait_data && nw_client_wait_data.is_controller
        && expected_nodes > 0)
    {
        nodes =
            nw_client_wait_data.num_players + nw_client_wait_data.num_drones;

        if (nodes >= expected_nodes)
        {
            StartGame(NULL, NULL);
            expected_nodes = 0;
        }
    }
}

void NW_WaitForLaunch(void)
{
    if (!TXT_Init())
    {
        I_Printf(VB_ERROR, "Failed to initialize GUI");
        I_SafeExit(-1);
    }

    TXT_SetColor(TXT_COLOR_BLUE, 0x04, 0x14, 0x40); // Romero's "funky blue" color

    I_InitWindowIcon();

    ParseCommandLineArgs();
    OpenWaitDialog();
    had_warning = false;

    while (nw_waiting_for_launch)
    {
        UpdateGUI();
        CheckAutoLaunch();
        CheckMasterStatus();

        TXT_DispatchEvents();
        TXT_DrawDesktop();

        NW_CL_Run();
        NW_SV_Run();

        if (!nw_client_connected)
        {
            I_Error("Lost connection to server");
        }

        TXT_Sleep(100);
    }

    TXT_Shutdown();
}
