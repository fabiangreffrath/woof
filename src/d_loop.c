//
// Copyright(C) 1993-1996 Id Software, Inc.
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
// DESCRIPTION:
//     Main loop code.
//

#include <string.h>

#include "d_event.h"
#include "d_loop.h"
#include "d_ticcmd.h"
#include "doomdef.h"
#include "doomstat.h"
#include "i_printf.h"
#include "i_system.h"
#include "i_timer.h"
#include "i_video.h"
#include "m_argv.h"
#include "m_fixed.h"
#include "net_client.h"
#include "net_gui.h"
#include "net_io.h"
#include "net_loop.h"
#include "net_query.h"
#include "net_netlib.h"
#include "net_server.h"
#include "s_sound.h"

// The complete set of data for a particular tic.

typedef struct
{
    ticcmd_t cmds[NET_MAXPLAYERS];
    boolean ingame[NET_MAXPLAYERS];
} ticcmd_set_t;

// Maximum time that we wait in TryRunTics() for netgame data to be
// received before we bail out and render a frame anyway.
// Vanilla Doom used 20 for this value, but we use a smaller value
// instead for better responsiveness of the menu when we're stuck.
#define MAX_NETGAME_STALL_TICS 5

//
// gametic is the tic about to (or currently being) run
// maketic is the tic that hasn't had control made for it yet
// recvtic is the latest tic received from the server.
//
// a gametic cannot be run until ticcmds are received for it
// from all players.
//

static ticcmd_set_t ticdata[BACKUPTICS];

// The index of the next tic to be made (with a call to BuildTiccmd).

int maketic;

// The number of complete tics received from the server so far.

static int recvtic;

// Index of the local player.

static int localplayer;

// Used for original sync code.

static int skiptics = 0;

// Reduce the bandwidth needed by sampling game input less and transmitting
// less.  If ticdup is 2, sample half normal, 3 = one third normal, etc.

int ticdup;

// Amount to offset the timer for game sync.

fixed_t offsetms;

// Use new client syncronisation code

static boolean new_sync = true;

// Current players in the multiplayer game.
// This is distinct from playeringame[] used by the game code, which may
// modify playeringame[] when playing back multiplayer demos.

static boolean local_playeringame[NET_MAXPLAYERS];

// Requested player class "sent" to the server on connect.
// If we are only doing a single player game then this needs to be remembered
// and saved in the game settings.

static int player_class;

// 35 fps clock adjusted by offsetms milliseconds

static int GetAdjustedTime(void)
{
    // Use the adjustments from net_client.c only if we are
    // using the new sync mode.

    if (new_sync && net_client_connected)
    {
        int time_ms;

        time_ms = I_GetTimeMS();

        time_ms += (offsetms / FRACUNIT);

        return (time_ms * TICRATE) / 1000;
    }
    else
    {
        return I_GetTime();
    }
}

void D_ProcessEvents(void);
void G_BuildTiccmd(ticcmd_t *cmd);
void M_Ticker(void);

static boolean BuildNewTic(void)
{
    int gameticdiv;
    ticcmd_t cmd;

    gameticdiv = gametic / ticdup;

    I_StartTic();
    D_ProcessEvents();

    // Always run the menu

    M_Ticker();

    if (drone)
    {
        // In drone mode, do not generate any ticcmds.

        return false;
    }

    if (new_sync)
    {
        // If playing single player, do not allow tics to buffer
        // up very far

        if (!net_client_connected && maketic - gameticdiv > 2)
        {
            return false;
        }

        // Never go more than ~200ms ahead

        if (maketic - gameticdiv > 8)
        {
            return false;
        }
    }
    else
    {
        if (maketic - gameticdiv >= 5)
        {
            return false;
        }
    }

    // printf ("mk:%i ",maketic);
    memset(&cmd, 0, sizeof(ticcmd_t));
    G_BuildTiccmd(&cmd);

    if (net_client_connected)
    {
        NET_CL_SendTiccmd(&cmd, maketic);
    }

    ticdata[maketic % BACKUPTICS].cmds[localplayer] = cmd;
    ticdata[maketic % BACKUPTICS].ingame[localplayer] = true;

    ++maketic;

    return true;
}

//
// NetUpdate
// Builds ticcmds for console player,
// sends out a packet
//
int lasttime;

void NetUpdate(void)
{
    int nowtime;
    int newtics;
    int i;

    // If we are running with singletics (timing a demo), this
    // is all done separately.

    if (singletics)
    {
        return;
    }

    // Run network subsystems

    NET_CL_Run();
    NET_SV_Run();

    // check time
    nowtime = GetAdjustedTime() / ticdup;
    newtics = nowtime - lasttime;

    lasttime = nowtime;

    if (skiptics <= newtics)
    {
        newtics -= skiptics;
        skiptics = 0;
    }
    else
    {
        skiptics -= newtics;
        newtics = 0;
    }

    // build new ticcmds for console player

    for (i = 0; i < newtics; i++)
    {
        if (!BuildNewTic())
        {
            break;
        }
    }
}

static void D_Disconnected(void)
{
    // In drone mode, the game cannot continue once disconnected.

    if (drone)
    {
        I_Error("Disconnected from server in drone mode.");
    }

    // disconnected from server

    I_Printf(VB_WARNING, "Disconnected from server.");
}

//
// Invoked by the network engine when a complete set of ticcmds is
// available.
//

void D_ReceiveTic(ticcmd_t *ticcmds, boolean *players_mask)
{
    int i;

    // Disconnected from server?

    if (ticcmds == NULL && players_mask == NULL)
    {
        D_Disconnected();
        return;
    }

    for (i = 0; i < NET_MAXPLAYERS; ++i)
    {
        if (!drone && i == localplayer)
        {
            // This is us.  Don't overwrite it.
        }
        else
        {
            ticdata[recvtic % BACKUPTICS].cmds[i] = ticcmds[i];
            ticdata[recvtic % BACKUPTICS].ingame[i] = players_mask[i];
        }
    }

    ++recvtic;
}

//
// Start game loop
//
// Called after the screen is set but before the game starts running.
//

void D_StartGameLoop(void)
{
    lasttime = GetAdjustedTime() / ticdup;
}

//
// Block until the game start message is received from the server.
//

static void BlockUntilStart(net_gamesettings_t *settings,
                            netgame_startup_callback_t callback)
{
    while (!NET_CL_GetSettings(settings))
    {
        NET_CL_Run();
        NET_SV_Run();

        if (!net_client_connected)
        {
            I_Error("Lost connection to server");
        }

        if (callback != NULL
            && !callback(net_client_wait_data.ready_players,
                         net_client_wait_data.num_players))
        {
            I_Error("Netgame startup aborted.");
        }

        I_Sleep(100);
    }
}

void D_StartNetGame(net_gamesettings_t *settings,
                    netgame_startup_callback_t callback)
{
    int i;

    offsetms = 0;
    recvtic = 0;

    settings->consoleplayer = 0;
    settings->num_players = 1;
    settings->player_classes[0] = player_class;

    //!
    // @category net
    //
    // Use original network client sync code rather than the improved
    // sync code.
    //

    settings->new_sync = !M_ParmExists("-oldsync");

    //!
    // @category net
    // @arg <n>
    //
    // Send n extra tics in every packet as insurance against dropped
    // packets.
    //

    i = M_CheckParmWithArgs("-extratics", 1);

    if (i > 0)
    {
        settings->extratics = M_ParmArgToInt(i);
    }
    else
    {
        settings->extratics = 1;
    }

    //!
    // @category net
    // @arg <n>
    //
    // Reduce the resolution of the game by a factor of n, reducing
    // the amount of network bandwidth needed.
    //

    i = M_CheckParmWithArgs("-dup", 1);

    if (i > 0)
    {
        settings->ticdup = M_ParmArgToInt(i);
    }
    else
    {
        settings->ticdup = 1;
    }

    if (net_client_connected)
    {
        // Send our game settings and block until game start is received
        // from the server.

        NET_CL_StartGame(settings);
        BlockUntilStart(settings, callback);

        // Read the game settings that were received.

        NET_CL_GetSettings(settings);
    }

    if (drone)
    {
        settings->consoleplayer = 0;
    }

    // Set the local player and playeringame[] values.

    localplayer = settings->consoleplayer;

    for (i = 0; i < NET_MAXPLAYERS; ++i)
    {
        local_playeringame[i] = i < settings->num_players;
    }

    // Copy settings to global variables.

    ticdup = settings->ticdup;
    new_sync = settings->new_sync;

    if (ticdup < 1)
    {
        I_Error("invalid ticdup value (%d)", ticdup);
    }

    // TODO: Message disabled until we fix new_sync.
    // if (!new_sync)
    //{
    //    printf("Syncing netgames like Vanilla Doom.\n");
    //}
}

boolean D_InitNetGame(net_connect_data_t *connect_data)
{
    boolean result = false;
    net_addr_t *addr = NULL;
    int i;

    // Call D_QuitNetGame on exit:

    I_AtExit(D_QuitNetGame, true);

    player_class = connect_data->player_class;

    //!
    // @category net
    // @help
    //
    // Start a multiplayer server, listening for connections.
    //

    if (M_CheckParm("-server") > 0 || M_CheckParm("-privateserver") > 0)
    {
        NET_SV_Init();
        NET_SV_AddModule(&net_loop_server_module);
        NET_SV_AddModule(&netlib_module);
        NET_SV_RegisterWithMaster();

        net_loop_client_module.InitClient();
        addr = net_loop_client_module.ResolveAddress(NULL);
        NET_ReferenceAddress(addr);
    }
    else
    {
        //!
        // @category net
        // @help
        //
        // Automatically search the local LAN for a multiplayer
        // server and join it.
        //

        i = M_CheckParm("-autojoin");

        if (i > 0)
        {
            addr = NET_FindLANServer();

            if (addr == NULL)
            {
                I_Error("No server found on local LAN");
            }
        }

        //!
        // @arg <address>
        // @category net
        // @help
        //
        // Connect to a multiplayer server running on the given
        // address.
        //

        i = M_CheckParmWithArgs("-connect", 1);

        if (i > 0)
        {
            netlib_module.InitClient();
            addr = netlib_module.ResolveAddress(myargv[i + 1]);
            NET_ReferenceAddress(addr);

            if (addr == NULL)
            {
                I_Error("Unable to resolve '%s'\n", myargv[i + 1]);
            }
        }
    }

    if (addr != NULL)
    {
        if (M_CheckParm("-drone") > 0)
        {
            connect_data->drone = true;
        }

        if (!NET_CL_Connect(addr, connect_data))
        {
            I_Error("Failed to connect to %s:\n%s\n",
                    NET_AddrToString(addr), net_client_reject_reason);
        }

        I_Printf(VB_INFO, "D_InitNetGame: Connected to %s",
                 NET_AddrToString(addr));
        NET_ReleaseAddress(addr);

        // Wait for launch message received from server.

        NET_WaitForLaunch();

        result = true;
    }

    return result;
}

boolean D_CheckNetConnect(void)
{
    return net_client_connected;
}

void D_CheckNetPlaybackSkip(void)
{
    if (!net_client_connected)
    {
        return;
    }

    if (singletics || fastdemo || PLAYBACK_SKIP)
    {
        I_Printf(VB_INFO,
                 "Demo playback skipping is suppressed in network game.");
        singletics = false;
        fastdemo = false;
        timingdemo = false;
        playback_warp = -1;
        playback_skiptics = 0;
    }
}

//
// D_QuitNetGame
// Called before quitting to leave a net game
// without hanging the other players
//
void D_QuitNetGame(void)
{
    NET_SV_Shutdown();
    NET_CL_Disconnect();
    netlib_module.Shutdown();
}

static int GetLowTic(void)
{
    int lowtic;

    lowtic = maketic;

    if (net_client_connected)
    {
        if (drone || recvtic < lowtic)
        {
            lowtic = recvtic;
        }
    }

    return lowtic;
}

static int frameon;
static int frameskip[4];
static int oldnettics;

static void OldNetSync(void)
{
    unsigned int i;
    int keyplayer = -1;

    frameon++;

    // ideally maketic should be 1 - 3 tics above lowtic
    // if we are consistantly slower, speed up time

    for (i = 0; i < NET_MAXPLAYERS; i++)
    {
        if (local_playeringame[i])
        {
            keyplayer = i;
            break;
        }
    }

    if (keyplayer < 0)
    {
        // If there are no players, we can never advance anyway

        return;
    }

    if (localplayer == keyplayer)
    {
        // the key player does not adapt
    }
    else
    {
        if (maketic <= recvtic)
        {
            lasttime--;
            // printf ("-");
        }

        frameskip[frameon & 3] = oldnettics > recvtic;
        oldnettics = maketic;

        if (frameskip[0] && frameskip[1] && frameskip[2] && frameskip[3])
        {
            skiptics = 1;
            // printf ("+");
        }
    }
}

// Returns true if there are players in the game:

static boolean PlayersInGame(void)
{
    boolean result = false;
    unsigned int i;

    // If we are connected to a server, check if there are any players
    // in the game.

    if (net_client_connected)
    {
        for (i = 0; i < NET_MAXPLAYERS; ++i)
        {
            result = result || local_playeringame[i];
        }
    }

    // Whether single or multi-player, unless we are running as a drone,
    // we are in the game.

    if (!drone)
    {
        result = true;
    }

    return result;
}

int D_GetPlayersInNetGame(void)
{
    int i;
    int result = 0;

    if (net_client_connected)
    {
        for (i = 0; i < NET_MAXPLAYERS; ++i)
        {
            if (local_playeringame[i])
            {
                ++result;
            }
        }
    }

    return result;
}

// When using ticdup, certain values must be cleared out when running
// the duplicate ticcmds.

static void TicdupSquash(ticcmd_set_t *set)
{
    ticcmd_t *cmd;
    unsigned int i;

    for (i = 0; i < NET_MAXPLAYERS; ++i)
    {
        cmd = &set->cmds[i];
        cmd->chatchar = 0;
        if (cmd->buttons & BT_SPECIAL)
        {
            cmd->buttons = 0;
        }
    }
}

// When running in single player mode, clear all the ingame[] array
// except the local player.

static void SinglePlayerClear(ticcmd_set_t *set)
{
    unsigned int i;

    for (i = 0; i < NET_MAXPLAYERS; ++i)
    {
        if (i != localplayer)
        {
            set->ingame[i] = false;
        }
    }
}

//
// TryRunTics
//

void RunTic(ticcmd_t *cmds, boolean *ingame);

void TryRunTics(void)
{
    int i;
    int lowtic;
    int entertic;
    static int oldentertics;
    int realtics;
    int availabletics;
    int counts;

// [AM] If we've uncapped the framerate and there are no tics
//      to run, return early instead of waiting around.
#define return_early (uncapped && counts == 0 && screenvisible)

    // get real tics
    entertic = I_GetTime() / ticdup;
    realtics = entertic - oldentertics;
    oldentertics = entertic;

    // in singletics mode, run a single tic every time this function
    // is called.

    if (singletics)
    {
        BuildNewTic();
    }
    else
    {
        NetUpdate();
    }

    lowtic = GetLowTic();

    availabletics = lowtic - gametic / ticdup;

    // decide how many tics to run

    if (new_sync)
    {
        counts = availabletics;

        // [AM] If we've uncapped the framerate and there are no tics
        //      to run, return early instead of waiting around.
        if (return_early)
        {
            return;
        }
    }
    else
    {
        // decide how many tics to run
        if (realtics < availabletics - 1)
        {
            counts = realtics + 1;
        }
        else if (realtics < availabletics)
        {
            counts = realtics;
        }
        else
        {
            counts = availabletics;
        }

        // [AM] If we've uncapped the framerate and there are no tics
        //      to run, return early instead of waiting around.
        if (return_early)
        {
            return;
        }

        if (counts < 1)
        {
            counts = 1;
        }

        if (net_client_connected)
        {
            OldNetSync();
        }
    }

    if (counts < 1)
    {
        counts = 1;
    }

    // wait for new tics if needed
    while (!PlayersInGame() || lowtic < gametic / ticdup + counts)
    {
        NetUpdate();

        lowtic = GetLowTic();

        if (lowtic < gametic / ticdup)
        {
            I_Error("lowtic < gametic");
        }

        // Still no tics to run? Sleep until some are available.
        if (lowtic < gametic / ticdup + counts)
        {
            // If we're in a netgame, we might spin forever waiting for
            // new network data to be received. So don't stay in here
            // forever - give the menu a chance to work.
            if (I_GetTime() / ticdup - entertic >= MAX_NETGAME_STALL_TICS)
            {
                return;
            }

            I_Sleep(1);
        }
    }

    // run the count * ticdup dics
    while (counts--)
    {
        ticcmd_set_t *set;

        if (!PlayersInGame())
        {
            return;
        }

        set = &ticdata[(gametic / ticdup) % BACKUPTICS];

        if (!net_client_connected)
        {
            SinglePlayerClear(set);
        }

        for (i = 0; i < ticdup; i++)
        {
            if (gametic / ticdup > lowtic)
            {
                I_Error("gametic > lowtic");
            }

            memcpy(local_playeringame, set->ingame, sizeof(local_playeringame));

            RunTic(set->cmds, set->ingame);
            gametic++;

            // modify command for duplicated tics

            TicdupSquash(set);
        }

        NetUpdate(); // check for new console commands
    }

    // killough 3/16/98: change consoleplayer to displayplayer
    S_UpdateSounds(players[displayplayer].mo); // move positional sounds
}
