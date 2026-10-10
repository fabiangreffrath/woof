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
// Network client code
//

#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "d_loop.h"
#include "d_ticcmd.h"
#include "doomtype.h"
#include "i_printf.h"
#include "i_timer.h"
#include "m_fixed.h"
#include "m_misc.h"
#include "nw_client.h"
#include "nw_common.h"
#include "nw_io.h"
#include "nw_packet.h"
#include "nw_query.h"
#include "nw_server.h"
#include "nw_structrw.h"

typedef enum
{
    // waiting for the game to launch

    CLIENT_STATE_WAITING_LAUNCH,

    // waiting for the game to start

    CLIENT_STATE_WAITING_START,

    // in game

    CLIENT_STATE_IN_GAME,

} nw_clientstate_t;

// Type of structure used in the receive window

typedef struct
{
    // Whether this tic has been received yet

    boolean active;

    // Last time we sent a resend request for this tic

    unsigned int resend_time;

    // Tic data from server

    nw_full_ticcmd_t cmd;

} nw_server_recv_t;

// Type of structure used in the send window

typedef struct
{
    // Whether this slot is active yet

    boolean active;

    // The tic number

    unsigned int seq;

    // Time the command was generated

    unsigned int time;

    // Ticcmd diff

    nw_ticdiff_t cmd;
} nw_server_send_t;

static nw_connection_t client_connection;
static nw_clientstate_t client_state;
static nw_addr_t *server_addr;
static nw_context_t *client_context;

// game settings, as received from the server when the game started

static nw_gamesettings_t settings;

// Why did the server reject us?
char *nw_client_reject_reason = NULL;

// true if the client code is in use

boolean nw_client_connected;

// true if we have received waiting data from the server,
// and the wait data that was received.

boolean nw_client_received_wait_data;
nw_waitdata_t nw_client_wait_data;

// Waiting at the initial wait screen for the game to be launched?

boolean nw_waiting_for_launch = false;

// Name that we send to the server

const char *nw_player_name = NULL;

// Connected but not participating in the game (observer)

boolean drone = false;

// The last ticcmd constructed

static ticcmd_t last_ticcmd;

// Buffer of ticcmd diffs being sent to the server

static nw_server_send_t send_queue[BACKUPTICS];

// Receive window

static ticcmd_t recvwindow_cmd_base[NW_MAXPLAYERS];
static int recvwindow_start;
static nw_server_recv_t recvwindow[BACKUPTICS];

// Whether we need to send an acknowledgement and
// when gamedata was last received.

static boolean need_to_acknowledge;
static unsigned int gamedata_recv_time;

// The latency (time between when we sent our command and we got all
// the other players' commands from the server) for the last tic we
// received. We include this latency in tics we send to the server so
// that they can adjust to us.
static int last_latency;

// Hash checksums of our wad directory and dehacked data.

sha1_digest_t nw_local_wad_sha1sum;
sha1_digest_t nw_local_deh_sha1sum;

// Are we playing with the freedoom IWAD?

unsigned int nw_local_is_freedoom;

#define NW_CL_ExpandTicNum(b) NW_ExpandTicNum(recvwindow_start, (b))

// Called when we become disconnected from the server

static void NW_CL_Disconnected(void)
{
    D_ReceiveTic(NULL, NULL);
}

// Called when a packet is received from the server containing game
// data. This updates the clock synchronization variable (offsetms)
// using a PID filter that keeps client clocks in sync.
static void UpdateClockSync(unsigned int seq, unsigned int remote_latency)
{
    static int last_error, cumul_error;
    int latency, error;

    if (seq == send_queue[seq % BACKUPTICS].seq)
    {
        latency = I_GetTimeMS() - send_queue[seq % BACKUPTICS].time;
    }
    else if (seq > send_queue[seq % BACKUPTICS].seq)
    {
        // We have received the ticcmd from the server before we have
        // even sent ours

        latency = 0;
    }
    else
    {
        return;
    }

    // PID filter. These are manually trained parameters.
#define KP 0.1
#define KI 0.01
#define KD 0.02

    // How does our latency compare to the worst other player?
    error = latency - remote_latency;
    cumul_error += error;

    offsetms = KP * (FRACUNIT * error)
             - KI * (FRACUNIT * cumul_error)
             + (KD * FRACUNIT) * (last_error - error);

    last_error = error;
    last_latency = latency;

    NW_Log("client: latency %d, remote %d -> offset=%dms, cumul_error=%d",
            latency, remote_latency, offsetms / FRACUNIT, cumul_error);
}

// Expand a nw_full_ticcmd_t, applying the diffs in cmd->cmds as
// patches against recvwindow_cmd_base.  Place the results into
// the d_net.c structures (netcmds/nettics) and save the new ticcmd
// back into recvwindow_cmd_base.

static void NW_CL_ExpandFullTiccmd(nw_full_ticcmd_t *cmd, unsigned int seq,
                                    ticcmd_t *ticcmds)
{
    int i;

    // Expand tic diffs for all players

    for (i = 0; i < NW_MAXPLAYERS; ++i)
    {
        if (i == settings.consoleplayer && !drone)
        {
            continue;
        }

        if (cmd->playeringame[i])
        {
            nw_ticdiff_t *diff;

            diff = &cmd->cmds[i];

            // Use the ticcmd diff to patch the previous ticcmd to
            // the new ticcmd

            NW_TiccmdPatch(&recvwindow_cmd_base[i], diff, &ticcmds[i]);

            // Store a copy for next time

            recvwindow_cmd_base[i] = ticcmds[i];
        }
    }
}

// Advance the receive window

static void NW_CL_AdvanceWindow(void)
{
    ticcmd_t ticcmds[NW_MAXPLAYERS];

    while (recvwindow[0].active)
    {
        // Expand tic diff data into d_net.c structures

        NW_CL_ExpandFullTiccmd(&recvwindow[0].cmd, recvwindow_start, ticcmds);
        D_ReceiveTic(ticcmds, recvwindow[0].cmd.playeringame);

        // Advance the window

        memmove(recvwindow, recvwindow + 1,
                sizeof(nw_server_recv_t) * (BACKUPTICS - 1));
        memset(&recvwindow[BACKUPTICS - 1], 0, sizeof(nw_server_recv_t));

        ++recvwindow_start;

        NW_Log("client: advanced receive window to %d", recvwindow_start);
    }
}

// Shut down the client code, etc.  Invoked after a disconnect.

static void NW_CL_Shutdown(void)
{
    if (nw_client_connected)
    {
        nw_client_connected = false;

        NW_ReleaseAddress(server_addr);

        // Shut down network module, etc.  To do.
    }
}

void NW_CL_LaunchGame(void)
{
    NW_Conn_NewReliable(&client_connection, NW_PACKET_TYPE_LAUNCH);
}

void NW_CL_StartGame(nw_gamesettings_t *settings)
{
    nw_packet_t *packet;

    // Start from a ticcmd of all zeros

    memset(&last_ticcmd, 0, sizeof(ticcmd_t));

    // Send packet

    packet =
        NW_Conn_NewReliable(&client_connection, NW_PACKET_TYPE_GAMESTART);

    NW_WriteSettings(packet, settings);
}

static void NW_CL_SendGameDataACK(void)
{
    nw_packet_t *packet;

    packet = NW_NewPacket(10);

    NW_WriteInt16(packet, NW_PACKET_TYPE_GAMEDATA_ACK);
    NW_WriteInt8(packet, recvwindow_start & 0xff);

    NW_Conn_SendPacket(&client_connection, packet);

    NW_FreePacket(packet);

    need_to_acknowledge = false;
}

static void NW_CL_SendTics(int start, int end)
{
    nw_packet_t *packet;
    int i;

    if (!nw_client_connected)
    {
        // Disconnected from server

        return;
    }

    if (start < 0)
    {
        start = 0;
    }

    // Build a new packet to send to the server

    packet = NW_NewPacket(512);
    NW_WriteInt16(packet, NW_PACKET_TYPE_GAMEDATA);

    // Write the start tic and number of tics.  Send only the low byte
    // of start - it can be inferred by the server.

    NW_WriteInt8(packet, recvwindow_start & 0xff);
    NW_WriteInt8(packet, start & 0xff);
    NW_WriteInt8(packet, end - start + 1);

    // Add the tics.

    for (i = start; i <= end; ++i)
    {
        nw_server_send_t *sendobj;

        sendobj = &send_queue[i % BACKUPTICS];

        NW_WriteInt16(packet, last_latency);

        NW_WriteTiccmdDiff(packet, &sendobj->cmd, settings.lowres_turn);
    }

    // Send the packet

    NW_Conn_SendPacket(&client_connection, packet);

    // All done!

    NW_FreePacket(packet);

    // Acknowledgement has been sent as part of the packet

    need_to_acknowledge = false;
}

// Add a new ticcmd to the send queue

void NW_CL_SendTiccmd(ticcmd_t *ticcmd, int maketic)
{
    nw_ticdiff_t diff;
    nw_server_send_t *sendobj;
    int starttic, endtic;

    // Calculate the difference to the last ticcmd

    NW_TiccmdDiff(&last_ticcmd, ticcmd, &diff);

    // Store in the send queue

    sendobj = &send_queue[maketic % BACKUPTICS];
    sendobj->active = true;
    sendobj->seq = maketic;
    sendobj->time = I_GetTimeMS();
    sendobj->cmd = diff;

    last_ticcmd = *ticcmd;

    // Send to server.

    starttic = maketic - settings.extratics;
    endtic = maketic;

    if (starttic < 0)
    {
        starttic = 0;
    }

    NW_Log("client: generated tic %d, sending %d-%d", maketic, starttic,
            endtic);
    NW_CL_SendTics(starttic, endtic);
}

// Parse a SYN packet received back from the server indicating a successful
// connection attempt.
static void NW_CL_ParseSYN(nw_packet_t *packet)
{
    nw_protocol_t protocol;
    char *server_version;

    NW_Log("client: processing SYN response");

    server_version = NW_ReadSafeString(packet);
    if (server_version == NULL)
    {
        NW_Log("client: error: failed to read server version");
        return;
    }

    protocol = NW_ReadProtocol(packet);
    if (protocol == NW_PROTOCOL_UNKNOWN)
    {
        NW_Log("client: error: can't find a common protocol");
        return;
    }

    // We are now successfully connected.
    NW_Log("client: connected to server");
    client_connection.state = NW_CONN_STATE_CONNECTED;
    client_connection.protocol = protocol;

    // Even though we have negotiated a compatible protocol, the game may still
    // desync. Chocolate Doom's philosophy makes this unlikely, but if we're
    // playing with a forked version, or even against a different version that
    // fixes a compatibility issue, we may still have problems.
    if (strcmp(server_version, PROJECT_STRING) != 0)
    {
        I_Printf(VB_WARNING,
                 "NW_CL_ParseSYN: This is '%s', but the server is "
                 "'%s'. It is possible that this mismatch may cause the game "
                 "to desync.",
                 PROJECT_STRING, server_version);
    }
}

static void SetRejectReason(const char *s)
{
    free(nw_client_reject_reason);
    if (s != NULL)
    {
        nw_client_reject_reason = strdup(s);
    }
    else
    {
        nw_client_reject_reason = NULL;
    }
}

static void NW_CL_ParseReject(nw_packet_t *packet)
{
    char *msg;

    msg = NW_ReadSafeString(packet);
    if (msg == NULL)
    {
        return;
    }

    if (client_connection.state == NW_CONN_STATE_CONNECTING)
    {
        client_connection.state = NW_CONN_STATE_DISCONNECTED;
        client_connection.disconnect_reason = NW_DISCONNECT_REMOTE;
        SetRejectReason(msg);
    }
}

// data received while we are waiting for the game to start

static void NW_CL_ParseWaitingData(nw_packet_t *packet)
{
    nw_waitdata_t wait_data;

    if (!NW_ReadWaitData(packet, &wait_data))
    {
        // Invalid packet?
        return;
    }

    if (wait_data.num_players > wait_data.max_players
        || wait_data.ready_players > wait_data.num_players
        || wait_data.max_players > NW_MAXPLAYERS)
    {
        // insane data

        return;
    }

    if ((wait_data.consoleplayer >= 0 && drone)
        || (wait_data.consoleplayer < 0 && !drone)
        || (wait_data.consoleplayer >= wait_data.num_players))
    {
        // Invalid player number

        return;
    }

    memcpy(&nw_client_wait_data, &wait_data, sizeof(nw_waitdata_t));
    nw_client_received_wait_data = true;
}

static void NW_CL_ParseLaunch(nw_packet_t *packet)
{
    unsigned int num_players;

    NW_Log("client: processing launch packet");

    if (client_state != CLIENT_STATE_WAITING_LAUNCH)
    {
        NW_Log("client: error: not in waiting launch state, client_state=%d",
                client_state);
        return;
    }

    // The launch packet contains the number of players that will be
    // in the game when it starts, so that we can do the startup
    // progress indicator (the wait data is unreliable).

    if (!NW_ReadInt8(packet, &num_players))
    {
        NW_Log("client: error: failed to read number of players");
        return;
    }

    nw_client_wait_data.num_players = num_players;
    client_state = CLIENT_STATE_WAITING_START;
    NW_Log("client: now waiting for game start");
}

static void NW_CL_ParseGameStart(nw_packet_t *packet)
{
    NW_Log("client: processing game start packet");

    if (!NW_ReadSettings(packet, &settings))
    {
        NW_Log("client: error: failed to read settings");
        return;
    }

    if (client_state != CLIENT_STATE_WAITING_START)
    {
        NW_Log("client: error: not in waiting start state, client_state=%d",
                client_state);
        return;
    }

    if (settings.num_players > NW_MAXPLAYERS
        || settings.consoleplayer >= (signed int)settings.num_players)
    {
        // insane values
        NW_Log("client: error: bad settings, num_players=%d, consoleplayer=%d",
                settings.num_players, settings.consoleplayer);
        return;
    }

    if ((drone && settings.consoleplayer >= 0)
        || (!drone && settings.consoleplayer < 0))
    {
        // Invalid player number: must be positive for real players,
        // negative for drones
        NW_Log("client: error: mismatch: drone=%d, consoleplayer=%d", drone,
                settings.consoleplayer);
        return;
    }

    NW_Log("client: beginning game state");
    client_state = CLIENT_STATE_IN_GAME;

    // Clear the receive window

    memset(recvwindow, 0, sizeof(recvwindow));
    recvwindow_start = 0;
    memset(&recvwindow_cmd_base, 0, sizeof(recvwindow_cmd_base));

    // Clear the send queue

    memset(&send_queue, 0x00, sizeof(send_queue));
}

static void NW_CL_SendResendRequest(int start, int end)
{
    nw_packet_t *packet;
    unsigned int nowtime;
    int i;

    // printf("CL: Send resend %i-%i\n", start, end);

    packet = NW_NewPacket(64);
    NW_WriteInt16(packet, NW_PACKET_TYPE_GAMEDATA_RESEND);
    NW_WriteInt32(packet, start);
    NW_WriteInt8(packet, end - start + 1);
    NW_Conn_SendPacket(&client_connection, packet);
    NW_FreePacket(packet);

    nowtime = I_GetTimeMS();

    // Save the time we sent the resend request

    for (i = start; i <= end; ++i)
    {
        int index;

        index = i - recvwindow_start;

        if (index < 0 || index >= BACKUPTICS)
        {
            continue;
        }

        recvwindow[index].resend_time = nowtime;
    }
}

// Check for expired resend requests

static void NW_CL_CheckResends(void)
{
    int i;
    int resend_start, resend_end;
    unsigned int nowtime;
    boolean maybe_deadlocked;

    nowtime = I_GetTimeMS();
    maybe_deadlocked = nowtime - gamedata_recv_time > 1000;

    resend_start = -1;
    resend_end = -1;

    for (i = 0; i < BACKUPTICS; ++i)
    {
        nw_server_recv_t *recvobj;
        boolean need_resend;

        recvobj = &recvwindow[i];

        // if need_resend is true, this tic needs another retransmit
        // request (300ms timeout)

        need_resend = !recvobj->active && recvobj->resend_time != 0
                      && nowtime > recvobj->resend_time + 300;

        // if no game data has been received in a long time, we may be in
        // a deadlock scenario where tics from the server have been lost, so
        // we've stopped generating any more, so the server isn't sending us
        // any, so we don't get any to trigger a resend request. So force the
        // first few tics in the receive window to be requested.
        if (i == 0 && !recvobj->active && recvobj->resend_time == 0
            && maybe_deadlocked)
        {
            need_resend = true;
        }

        if (need_resend)
        {
            // Start a new run of resend tics?

            if (resend_start < 0)
            {
                resend_start = i;
            }

            resend_end = i;
        }
        else if (resend_start >= 0)
        {
            // End of a run of resend tics
            NW_Log("client: resend request timed out for %d-%d (%d)",
                    recvwindow_start + resend_start,
                    recvwindow_start + resend_end,
                    recvwindow[resend_start].resend_time);
            NW_CL_SendResendRequest(recvwindow_start + resend_start,
                                     recvwindow_start + resend_end);
            resend_start = -1;
        }
    }

    if (resend_start >= 0)
    {
        NW_Log("client: resend request timed out for %d-%d (%d)",
                recvwindow_start + resend_start, recvwindow_start + resend_end,
                recvwindow[resend_start].resend_time);
        NW_CL_SendResendRequest(recvwindow_start + resend_start,
                                 recvwindow_start + resend_end);
    }

    // We have received some data from the server and not acknowledged
    // it yet.  Normally this gets acknowledged when we send our game
    // data, but if the client is a drone we need to do this.

    if (need_to_acknowledge && nowtime - gamedata_recv_time > 200)
    {
        NW_Log("client: no game data received since %d: triggering ack",
                gamedata_recv_time);
        NW_CL_SendGameDataACK();
    }
}

// Parsing of NW_PACKET_TYPE_GAMEDATA packets
// (packets containing the actual ticcmd data)

static void NW_CL_ParseGameData(nw_packet_t *packet)
{
    nw_server_recv_t *recvobj;
    unsigned int seq, num_tics;
    unsigned int nowtime;
    int resend_start, resend_end;
    unsigned long i;
    int index;

    NW_Log("client: processing game data packet");

    // Read header
    if (!NW_ReadInt8(packet, &seq) || !NW_ReadInt8(packet, &num_tics))
    {
        NW_Log("client: error: failed to read header");
        return;
    }

    nowtime = I_GetTimeMS();

    // Whatever happens, we now need to send an acknowledgement of our
    // current receive point.

    if (!need_to_acknowledge)
    {
        need_to_acknowledge = true;
        gamedata_recv_time = nowtime;
    }

    // Expand byte value into the full tic number
    seq = NW_CL_ExpandTicNum(seq);
    NW_Log("client: got game data, seq=%d, num_tics=%d", seq, num_tics);

    for (i = 0; i < num_tics; ++i)
    {
        nw_full_ticcmd_t cmd;

        index = seq - recvwindow_start + i;

        if (!NW_ReadFullTiccmd(packet, &cmd, settings.lowres_turn))
        {
            NW_Log("client: error: failed to read ticcmd %lu",
                    (unsigned long)i);
            return;
        }

        if (index < 0 || index >= BACKUPTICS)
        {
            // Out of range of the recv window

            continue;
        }

        // Store in the receive window

        recvobj = &recvwindow[index];

        recvobj->active = true;
        recvobj->cmd = cmd;
        NW_Log("client: stored tic %lu in receive window",
                (unsigned long)seq + i);

        // If a packet is lost or arrives out of order, we might get
        // the tic in the next packet instead (because of extratic).
        // If that's the case then the latency for receiving that tic
        // now will be bogus. So we only use the last tic in the packet
        // to trigger a clock sync update.
        if (i == num_tics - 1)
        {
            UpdateClockSync(seq + i, cmd.latency);
        }
    }

    // Has this been received out of sequence, ie. have we not received
    // all tics before the first tic in this packet?  If so, send a
    // resend request.

    // printf("CL: %p: %i\n", client, seq);

    resend_end = seq - recvwindow_start;

    if (resend_end <= 0)
    {
        return;
    }

    if (resend_end >= BACKUPTICS)
    {
        resend_end = BACKUPTICS - 1;
    }

    index = resend_end - 1;
    resend_start = resend_end;

    while (index >= 0)
    {
        recvobj = &recvwindow[index];

        if (recvobj->active)
        {
            // ended our run of unreceived tics

            break;
        }

        if (recvobj->resend_time != 0)
        {
            // Already sent a resend request for this tic

            break;
        }

        resend_start = index;
        --index;
    }

    // Possibly send a resend request
    if (resend_start < resend_end)
    {
        NW_Log("client: request resend for %d-%d before %d",
                recvwindow_start + resend_start,
                recvwindow_start + resend_end - 1, seq);
        NW_CL_SendResendRequest(recvwindow_start + resend_start,
                                 recvwindow_start + resend_end - 1);
    }
}

// Parse a resend request from the server due to a dropped packet

static void NW_CL_ParseResendRequest(nw_packet_t *packet)
{
    static unsigned int start;
    static unsigned int end;
    static unsigned int num_tics;

    NW_Log("client: processing resend request");

    if (drone)
    {
        // Drones don't send gamedata.
        NW_Log("client: error: resend request but we're a drone?");
        return;
    }

    if (!NW_ReadInt32(packet, &start) || !NW_ReadInt8(packet, &num_tics))
    {
        NW_Log("client: error: couldn't read start and num_tics");
        return;
    }

    end = start + num_tics - 1;

    // printf("requested resend %i-%i .. ", start, end);
    NW_Log("client: resend request: start=%d, num_tics=%d", start, num_tics);

    // Check we have the tics being requested.  If not, reduce the
    // window of tics to only what we have.

    while (start <= end
           && (!send_queue[start % BACKUPTICS].active
               || send_queue[start % BACKUPTICS].seq != start))
    {
        ++start;
    }

    while (start <= end
           && (!send_queue[end % BACKUPTICS].active
               || send_queue[end % BACKUPTICS].seq != end))
    {
        --end;
    }

    // Resend those tics
    if (start <= end)
    {
        NW_Log("client: resending %d-%d", start, end);
        NW_CL_SendTics(start, end);
    }
    else
    {
        NW_Log("client: don't have the tics to resend");
    }
}

// Console message that the server wants the client to print

static void NW_CL_ParseConsoleMessage(nw_packet_t *packet)
{
    char *msg;

    msg = NW_ReadSafeString(packet);

    if (msg == NULL)
    {
        return;
    }

    I_Printf(VB_INFO, "Message from server:\n%s", msg);
}

// parse a received packet

static void NW_CL_ParsePacket(nw_packet_t *packet)
{
    unsigned int packet_type;

    if (!NW_ReadInt16(packet, &packet_type))
    {
        return;
    }

    NW_Log("client: packet from server, type %d",
            packet_type & ~NW_RELIABLE_PACKET);
    NW_LogPacket(packet);

    if (NW_Conn_Packet(&client_connection, packet, &packet_type))
    {
        // Packet eaten by the common connection code
    }
    else
    {
        switch (packet_type)
        {
            case NW_PACKET_TYPE_SYN:
                NW_CL_ParseSYN(packet);
                break;

            case NW_PACKET_TYPE_REJECTED:
                NW_CL_ParseReject(packet);
                break;

            case NW_PACKET_TYPE_WAITING_DATA:
                NW_CL_ParseWaitingData(packet);
                break;

            case NW_PACKET_TYPE_LAUNCH:
                NW_CL_ParseLaunch(packet);
                break;

            case NW_PACKET_TYPE_GAMESTART:
                NW_CL_ParseGameStart(packet);
                break;

            case NW_PACKET_TYPE_GAMEDATA:
                NW_CL_ParseGameData(packet);
                break;

            case NW_PACKET_TYPE_GAMEDATA_RESEND:
                NW_CL_ParseResendRequest(packet);
                break;

            case NW_PACKET_TYPE_CONSOLE_MESSAGE:
                NW_CL_ParseConsoleMessage(packet);
                break;

            default:
                break;
        }
    }
}

// "Run" the client code: check for new packets, send packets as
// needed

void NW_CL_Run(void)
{
    nw_addr_t *addr;
    nw_packet_t *packet;

    if (!nw_client_connected)
    {
        return;
    }

    while (NW_RecvPacket(client_context, &addr, &packet))
    {
        // only accept packets from the server

        if (addr == server_addr)
        {
            NW_CL_ParsePacket(packet);
        }

        NW_FreePacket(packet);
        NW_ReleaseAddress(addr);
    }

    // Run the common connection code to send any packets as needed

    NW_Conn_Run(&client_connection);

    if (client_connection.state == NW_CONN_STATE_DISCONNECTED
        || client_connection.state == NW_CONN_STATE_DISCONNECTED_SLEEP)
    {
        NW_CL_Disconnected();

        NW_CL_Shutdown();
    }

    nw_waiting_for_launch = client_connection.state == NW_CONN_STATE_CONNECTED
                             && client_state == CLIENT_STATE_WAITING_LAUNCH;

    if (client_state == CLIENT_STATE_IN_GAME)
    {
        // Possibly advance the receive window

        NW_CL_AdvanceWindow();

        // Check if our resend requests have timed out

        NW_CL_CheckResends();
    }
}

static void NW_CL_SendSYN(nw_connect_data_t *data)
{
    nw_packet_t *packet;

    NW_Log("client: sending SYN");

    packet = NW_NewPacket(10);
    NW_WriteInt16(packet, NW_PACKET_TYPE_SYN);
    NW_WriteInt32(packet, NW_MAGIC_NUMBER);
    NW_WriteString(packet, PROJECT_STRING);
    NW_WriteProtocolList(packet);
    NW_WriteConnectData(packet, data);
    NW_WriteString(packet, nw_player_name);
    NW_Conn_SendPacket(&client_connection, packet);
    NW_FreePacket(packet);
}

// Connect to a server
boolean NW_CL_Connect(nw_addr_t *addr, nw_connect_data_t *data)
{
    int start_time;
    int last_send_time;
    boolean sent_hole_punch;

    server_addr = addr;
    NW_ReferenceAddress(addr);

    memcpy(nw_local_wad_sha1sum, data->wad_sha1sum, sizeof(sha1_digest_t));
    memcpy(nw_local_deh_sha1sum, data->deh_sha1sum, sizeof(sha1_digest_t));
    nw_local_is_freedoom = data->is_freedoom;

    // create a new network I/O context and add just the necessary module
    client_context = NW_NewContext();

    // initialize module for client mode
    if (!addr->module->InitClient())
    {
        SetRejectReason("Failed to initialize client module");
        return false;
    }

    NW_AddModule(client_context, addr->module);

    nw_client_connected = true;
    nw_client_received_wait_data = false;
    sent_hole_punch = false;

    NW_Conn_InitClient(&client_connection, addr, NW_PROTOCOL_UNKNOWN);

    // try to connect
    start_time = I_GetTimeMS();
    last_send_time = -1;
    SetRejectReason("Unknown reason");

    while (client_connection.state == NW_CONN_STATE_CONNECTING)
    {
        int nowtime = I_GetTimeMS();

        // Send a SYN packet every second.
        if (nowtime - last_send_time > 1000 || last_send_time < 0)
        {
            NW_CL_SendSYN(data);
            last_send_time = nowtime;
        }

        // time out after 5 seconds
        if (nowtime - start_time > 5000)
        {
            SetRejectReason("No response from server");
            break;
        }

        if (!sent_hole_punch && nowtime - start_time > 2000)
        {
            NW_Log("client: no response to SYN, requesting hole punch");
            NW_RequestHolePunch(client_context, addr);
            sent_hole_punch = true;
        }

        // run client code
        NW_CL_Run();

        // run the server, just in case we are doing a loopback connect
        NW_SV_Run();

        // Don't hog the CPU
        I_Sleep(1);
    }

    if (client_connection.state == NW_CONN_STATE_CONNECTED)
    {
        // connected ok!
        NW_Log("client: connected successfully");
        SetRejectReason(NULL);
        client_state = CLIENT_STATE_WAITING_LAUNCH;
        drone = data->drone;

        return true;
    }
    else
    {
        // failed to connect
        NW_Log("client: failed to connect");
        NW_CL_Shutdown();

        return false;
    }
}

// read game settings received from server

boolean NW_CL_GetSettings(nw_gamesettings_t *_settings)
{
    if (client_state != CLIENT_STATE_IN_GAME)
    {
        return false;
    }

    memcpy(_settings, &settings, sizeof(nw_gamesettings_t));

    return true;
}

// disconnect from the server

void NW_CL_Disconnect(void)
{
    int start_time;

    if (!nw_client_connected)
    {
        return;
    }

    NW_Log("client: beginning disconnect");
    NW_Conn_Disconnect(&client_connection);

    start_time = I_GetTimeMS();

    while (client_connection.state != NW_CONN_STATE_DISCONNECTED
           && client_connection.state != NW_CONN_STATE_DISCONNECTED_SLEEP)
    {
        if (I_GetTimeMS() - start_time > 5000)
        {
            // time out after 5 seconds

            NW_Log("client: no acknowledgement of disconnect received");
            client_state = CLIENT_STATE_WAITING_START;

            I_Printf(VB_WARNING,
                     "NW_CL_Disconnect: Timeout while disconnecting "
                     "from server");
            break;
        }

        NW_CL_Run();
        NW_SV_Run();

        I_Sleep(1);
    }

    // Finished sending disconnect packets, etc.
    NW_Log("client: disconnect complete");
    NW_CL_Shutdown();
}

void NW_CL_Init(void)
{
    if (nw_player_name == NULL)
    {
        nw_player_name = M_StringDuplicate(DEFAULT_PLAYER_NAME);
    }
}

void NW_Init(void)
{
    NW_OpenLog();
    NW_CL_Init();
}
