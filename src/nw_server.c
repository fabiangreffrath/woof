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
// Network server code
//

#include <limits.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "doomdef.h"
#include "doomtype.h"
#include "i_printf.h"
#include "i_system.h"
#include "i_timer.h"
#include "m_argv.h"
#include "m_misc.h"
#include "nw_client.h"
#include "nw_common.h"
#include "nw_io.h"
#include "nw_packet.h"
#include "nw_query.h"
#include "nw_server.h"
#include "nw_structrw.h"

// How often to refresh our registration with the master server.
#define MASTER_REFRESH_PERIOD 30 /* twice per minute */

// How often to re-resolve the address of the master server?
#define MASTER_RESOLVE_PERIOD 8 * 60 * 60 /* 8 hours */

typedef enum
{
    // waiting for the game to be "launched" (key player to press the start
    // button)

    SERVER_WAITING_LAUNCH,

    // game has been launched, we are waiting for all players to be ready
    // so the game can start.

    SERVER_WAITING_START,

    // in a game

    SERVER_IN_GAME,
} nw_server_state_t;

typedef struct
{
    boolean active;
    int player_number;
    nw_addr_t *addr;
    nw_connection_t connection;
    int last_send_time;
    char *name;

    // If true, the client has sent the NW_PACKET_TYPE_GAMESTART
    // message indicating that it is ready for the game to start.

    boolean ready;

    // Time that this client connected to the server.
    // This is used to determine the controller (oldest client).

    unsigned int connect_time;

    // Last time new gamedata was received from this client

    int last_gamedata_time;

    // recording a demo without -longtics

    boolean recording_lowres;

    // send queue: items to send to the client
    // this is a circular buffer

    int sendseq;
    nw_full_ticcmd_t sendqueue[BACKUPTICS];

    // Latest acknowledged by the client

    unsigned int acknowledged;

    // Value of max_players specified by the client on connect.

    int max_players;

    // Observer: receives data but does not participate in the game.

    boolean drone;

    // SHA1 hash sums of the client's WAD directory and dehacked data

    sha1_digest_t wad_sha1sum;
    sha1_digest_t deh_sha1sum;

    // Is this client is playing with the Freedoom IWAD?

    unsigned int is_freedoom;

    // Player class (for Hexen)

    int player_class;

} nw_client_t;

// structure used for the recv window

typedef struct
{
    // Whether this tic has been received yet

    boolean active;

    // Latency value received from the client

    signed int latency;

    // Last time we sent a resend request for this tic

    unsigned int resend_time;

    // Tic data itself

    nw_ticdiff_t diff;
} nw_client_recv_t;

static nw_server_state_t server_state;
static boolean server_initialized = false;
static nw_client_t clients[MAXNETNODES];
static nw_client_t *sv_players[NW_MAXPLAYERS];
static nw_context_t *server_context;
static unsigned int sv_gamemode;
static unsigned int sv_gamemission;
static nw_gamesettings_t sv_settings;

// For registration with master server:

static nw_addr_t *master_server = NULL;
static unsigned int master_refresh_time;
static unsigned int master_resolve_time;

// receive window

static unsigned int recvwindow_start;
static nw_client_recv_t recvwindow[BACKUPTICS][NW_MAXPLAYERS];

#define NW_SV_ExpandTicNum(b) NW_ExpandTicNum(recvwindow_start, (b))

static void NW_SV_DisconnectClient(nw_client_t *client)
{
    if (client->active)
    {
        NW_Conn_Disconnect(&client->connection);
    }
}

static boolean ClientConnected(nw_client_t *client)
{
    // Check that the client is properly connected: ie. not in the
    // process of connecting or disconnecting

    return client->active
           && client->connection.state == NW_CONN_STATE_CONNECTED;
}

// Send a message to be displayed on a client's console

static void NW_SV_SendConsoleMessage(nw_client_t *client, const char *s, ...)
    PRINTF_ATTR(2, 3);

static void NW_SV_SendConsoleMessage(nw_client_t *client, const char *s, ...)
{
    char buf[1024];
    va_list args;
    nw_packet_t *packet;

    va_start(args, s);
    M_vsnprintf(buf, sizeof(buf), s, args);
    va_end(args);

    packet = NW_Conn_NewReliable(&client->connection,
                                  NW_PACKET_TYPE_CONSOLE_MESSAGE);

    NW_WriteString(packet, buf);
}

// Send a message to all clients

static void NW_SV_BroadcastMessage(const char *s, ...) PRINTF_ATTR(1, 2);

static void NW_SV_BroadcastMessage(const char *s, ...)
{
    char buf[1024];
    va_list args;
    int i;

    va_start(args, s);
    M_vsnprintf(buf, sizeof(buf), s, args);
    va_end(args);

    for (i = 0; i < MAXNETNODES; ++i)
    {
        if (ClientConnected(&clients[i]))
        {
            NW_SV_SendConsoleMessage(&clients[i], "%s", buf);
        }
    }

    I_Printf(VB_INFO, "%s", buf);
}

// Assign player numbers to connected clients

static void NW_SV_AssignPlayers(void)
{
    int i;
    int pl;

    pl = 0;

    for (i = 0; i < MAXNETNODES; ++i)
    {
        if (ClientConnected(&clients[i]))
        {
            if (!clients[i].drone)
            {
                sv_players[pl] = &clients[i];
                sv_players[pl]->player_number = pl;
                ++pl;
            }
            else
            {
                clients[i].player_number = -1;
            }
        }
    }

    for (; pl < NW_MAXPLAYERS; ++pl)
    {
        sv_players[pl] = NULL;
    }
}

// Returns the number of players currently connected.

static int NW_SV_NumPlayers(void)
{
    int i;
    int result;

    result = 0;

    for (i = 0; i < NW_MAXPLAYERS; ++i)
    {
        if (sv_players[i] != NULL && ClientConnected(sv_players[i]))
        {
            result += 1;
        }
    }

    return result;
}

// Returns the number of players ready to start the game.

static int NW_SV_NumReadyPlayers(void)
{
    int result = 0;
    int i;

    for (i = 0; i < MAXNETNODES; ++i)
    {
        if (ClientConnected(&clients[i]) && !clients[i].drone
            && clients[i].ready)
        {
            ++result;
        }
    }

    return result;
}

// Returns the maximum number of players that can play.

static int NW_SV_MaxPlayers(void)
{
    int i;

    for (i = 0; i < MAXNETNODES; ++i)
    {
        if (ClientConnected(&clients[i]))
        {
            return clients[i].max_players;
        }
    }

    return NW_MAXPLAYERS;
}

// Returns the number of drones currently connected.

static int NW_SV_NumDrones(void)
{
    int i;
    int result;

    result = 0;

    for (i = 0; i < MAXNETNODES; ++i)
    {
        if (ClientConnected(&clients[i]) && clients[i].drone)
        {
            result += 1;
        }
    }

    return result;
}

// returns the number of clients connected

static int NW_SV_NumClients(void)
{
    int count;
    int i;

    count = 0;

    for (i = 0; i < MAXNETNODES; ++i)
    {
        if (ClientConnected(&clients[i]))
        {
            ++count;
        }
    }

    return count;
}

// returns a pointer to the client which controls the server

static nw_client_t *NW_SV_Controller(void)
{
    nw_client_t *best;
    int i;

    // Find the oldest client (first to connect).

    best = NULL;

    for (i = 0; i < MAXNETNODES; ++i)
    {
        // Can't be controller?

        if (!ClientConnected(&clients[i]) || clients[i].drone)
        {
            continue;
        }

        if (best == NULL || clients[i].connect_time < best->connect_time)
        {
            best = &clients[i];
        }
    }

    return best;
}

typedef enum
{
    RANGE_LOCALHOST,   // Same process or 127.x
    RANGE_PRIVATE,     // RFC 1918
    RANGE_PUBLIC,      // The public Internet
} ip_range_t;

static ip_range_t ClientAddressRange(const char *addr)
{
    if (!strcmp(addr, "local client")
     || M_StringStartsWith(addr, "127."))
    {
        return RANGE_LOCALHOST;
    }
    if (M_StringStartsWith(addr, "10.")
     || M_StringStartsWith(addr, "192.168."))
    {
        return RANGE_PRIVATE;
    }
    return RANGE_PUBLIC;
}

static void NW_SV_SendWaitingData(nw_client_t *client)
{
    nw_waitdata_t wait_data;
    nw_packet_t *packet;
    nw_client_t *controller;
    ip_range_t client_range, player_range;
    const char *addr;
    int i;

    NW_SV_AssignPlayers();

    controller = NW_SV_Controller();

    wait_data.num_players = NW_SV_NumPlayers();
    wait_data.num_drones = NW_SV_NumDrones();
    wait_data.ready_players = NW_SV_NumReadyPlayers();
    wait_data.max_players = NW_SV_MaxPlayers();
    wait_data.is_controller = (client == controller);
    wait_data.consoleplayer = client->player_number;

    // Send the WAD and dehacked checksums of the controlling client.
    // If no controller found (?), send the details that the client
    // is expecting anyway.

    if (controller == NULL)
    {
        controller = client;
    }

    memcpy(&wait_data.wad_sha1sum, &controller->wad_sha1sum,
           sizeof(sha1_digest_t));
    memcpy(&wait_data.deh_sha1sum, &controller->deh_sha1sum,
           sizeof(sha1_digest_t));
    wait_data.is_freedoom = controller->is_freedoom;

    // We only send IP addresses to locally-connected clients (including
    // the 127.* loopback range):
    addr = NW_AddrToString(client->connection.addr);
    client_range = ClientAddressRange(addr);

    // set name and address of each player:

    for (i = 0; i < wait_data.num_players; ++i)
    {
        M_StringCopy(wait_data.player_names[i], sv_players[i]->name,
                     MAXPLAYERNAME);

        // For privacy, only local clients or those on a LAN get to see
        // addresses. Public clients only get to see their own address,
        // though we do reveal localhost addresses since they're harmless,
        // and we do reveal when a client is connected via LAN.
        addr = NW_AddrToString(sv_players[i]->addr);
        player_range = ClientAddressRange(addr);
        if (client_range == RANGE_LOCALHOST || client_range == RANGE_PRIVATE
         || i == wait_data.consoleplayer || player_range == RANGE_LOCALHOST)
        {
            M_StringCopy(wait_data.player_addrs[i], addr, MAXPLAYERNAME);
        }
        else if (player_range == RANGE_PRIVATE)
        {
            M_snprintf(wait_data.player_addrs[i], MAXPLAYERNAME,
                       "[LAN player]");
        }
        else
        {
            M_snprintf(wait_data.player_addrs[i], MAXPLAYERNAME,
                       "[address hidden]");
        }
    }

    // Construct packet:

    packet = NW_NewPacket(10);
    NW_WriteInt16(packet, NW_PACKET_TYPE_WAITING_DATA);
    NW_WriteWaitData(packet, &wait_data);

    // Send packet to client and free

    NW_Conn_SendPacket(&client->connection, packet);
    NW_FreePacket(packet);
}

// Find the latest tic which has been acknowledged as received by
// all clients.

static unsigned int NW_SV_LatestAcknowledged(void)
{
    unsigned int lowtic = UINT_MAX;
    int i;

    for (i = 0; i < MAXNETNODES; ++i)
    {
        if (ClientConnected(&clients[i]))
        {
            if (clients[i].acknowledged < lowtic)
            {
                lowtic = clients[i].acknowledged;
            }
        }
    }

    return lowtic;
}

// Possibly advance the recv window if all connected clients have
// used the data in the window

static void NW_SV_AdvanceWindow(void)
{
    unsigned int lowtic;
    int i;

    if (NW_SV_NumPlayers() <= 0)
    {
        return;
    }

    lowtic = NW_SV_LatestAcknowledged();

    // Advance the recv window until it catches up with lowtic

    while (recvwindow_start < lowtic)
    {
        boolean should_advance;

        // Check we have tics from all players for first tic in
        // the recv window

        should_advance = true;

        for (i = 0; i < NW_MAXPLAYERS; ++i)
        {
            if (sv_players[i] == NULL || !ClientConnected(sv_players[i]))
            {
                continue;
            }

            if (!recvwindow[0][i].active)
            {
                should_advance = false;
                break;
            }
        }

        if (!should_advance)
        {
            // The first tic is not complete: ie. we have not
            // received tics from all connected players.  This can
            // happen if only one player is in the game.

            break;
        }

        // Advance the window

        memmove(recvwindow, recvwindow + 1,
                sizeof(*recvwindow) * (BACKUPTICS - 1));
        memset(&recvwindow[BACKUPTICS - 1], 0, sizeof(*recvwindow));
        ++recvwindow_start;
        NW_Log("server: advanced receive window to %d", recvwindow_start);
    }
}

// Given an address, find the corresponding client

static nw_client_t *NW_SV_FindClient(nw_addr_t *addr)
{
    int i;

    for (i = 0; i < MAXNETNODES; ++i)
    {
        if (clients[i].active && clients[i].addr == addr)
        {
            // found the client

            return &clients[i];
        }
    }

    return NULL;
}

// send a rejection packet to a client

static void NW_SV_SendReject(nw_addr_t *addr, const char *msg)
{
    nw_packet_t *packet;

    NW_Log("server: sending reject to %s", NW_AddrToString(addr));

    packet = NW_NewPacket(10);
    NW_WriteInt16(packet, NW_PACKET_TYPE_REJECTED);
    NW_WriteString(packet, msg);
    NW_SendPacket(addr, packet);
    NW_FreePacket(packet);
}

static void NW_SV_InitNewClient(nw_client_t *client, nw_addr_t *addr,
                                 nw_protocol_t protocol)
{
    client->active = true;
    client->connect_time = I_GetTimeMS();
    NW_Conn_InitServer(&client->connection, addr, protocol);
    client->addr = addr;
    NW_ReferenceAddress(addr);
    client->last_send_time = -1;

    // init the ticcmd send queue

    client->sendseq = 0;
    client->acknowledged = 0;
    client->drone = false;
    client->ready = false;

    client->last_gamedata_time = 0;

    memset(client->sendqueue, 0xff, sizeof(client->sendqueue));

    NW_Log("server: initialized new client from %s", NW_AddrToString(addr));
}

// parse a SYN from a client(initiating a connection)

static void NW_SV_ParseSYN(nw_packet_t *packet, nw_client_t *client,
                            nw_addr_t *addr)
{
    unsigned int magic;
    nw_connect_data_t data;
    nw_packet_t *reply;
    nw_protocol_t protocol;
    char *player_name;
    char *client_version;
    int num_players;
    int i;

    NW_Log("server: processing SYN packet");

    // Read the magic number and check it is the expected one.
    if (!NW_ReadInt32(packet, &magic))
    {
        NW_Log("server: error: no magic number for SYN");
        return;
    }

    switch (magic)
    {
        case NW_MAGIC_NUMBER:
            break;

        case NW_OLD_MAGIC_NUMBER:
            NW_Log("server: error: client using old magic number: %d", magic);
            NW_SV_SendReject(
                addr,
                "You are using an old client version that is not supported by "
                "this server. This server is running " PROJECT_STRING ".");
            return;

        default:
            NW_Log("server: error: wrong magic number: %d", magic);
            return;
    }

    // Read the client version string. We actually now only use this when
    // sending a reject message, as we only reject if we can't negotiate a
    // common protocol (below).
    client_version = NW_ReadString(packet);
    if (client_version == NULL)
    {
        NW_Log("server: error: no version from client");
        return;
    }

    // Read the client's list of accepted protocols. Net play between forks
    // of Chocolate Doom is accepted provided that they can negotiate a
    // common accepted protocol.
    protocol = NW_ReadProtocolList(packet);
    if (protocol == NW_PROTOCOL_UNKNOWN)
    {
        char reject_msg[256];

        M_snprintf(reject_msg, sizeof(reject_msg),
                   "Version mismatch: server version is: " PROJECT_STRING "; "
                   "client is: %s. No common compatible protocol could be "
                   "negotiated.",
                   client_version);
        NW_SV_SendReject(addr, reject_msg);
        NW_Log("server: error: no common protocol");
        return;
    }

    // Read connect data, and check that the game mode/mission are valid
    // and the max_players value is in a sensible range.
    if (!NW_ReadConnectData(packet, &data))
    {
        NW_Log("server: error: failed to read connect data");
        return;
    }

    if (/*!D_ValidGameMode(data.gamemission, data.gamemode) ||*/
        data.max_players > NW_MAXPLAYERS)
    {
        NW_Log("server: error: invalid connect data, max_players=%d, "
                "gamemission=%d, gamemode=%d",
                data.max_players, data.gamemission, data.gamemode);
        return;
    }

    // Read the player's name
    player_name = NW_ReadString(packet);
    if (player_name == NULL)
    {
        NW_Log("server: error: failed to read player name");
        return;
    }

    // At this point we have received a valid SYN.

    // Not accepting new connections?
    if (server_state != SERVER_WAITING_LAUNCH)
    {
        NW_Log("server: error: not in waiting launch state, server_state=%d",
                server_state);
        NW_SV_SendReject(addr,
                          "Server is not currently accepting connections");
        return;
    }

    // Before accepting a new client, check that there is a slot free.
    NW_SV_AssignPlayers();
    num_players = NW_SV_NumPlayers();

    if ((!data.drone && num_players >= NW_SV_MaxPlayers())
        || NW_SV_NumClients() >= MAXNETNODES)
    {
        NW_Log("server: no more players, num_players=%d, max=%d", num_players,
                NW_SV_MaxPlayers());
        NW_SV_SendReject(addr, "Server is full!");
        return;
    }

    // TODO: Add server option to allow rejecting clients which set
    // lowres_turn.  This is potentially desirable as the presence of such
    // clients affects turning resolution.

    // Adopt the game mode and mission of the first connecting client:
    if (num_players == 0 && !data.drone)
    {
        sv_gamemode = data.gamemode;
        sv_gamemission = data.gamemission;
        NW_Log("server: new game, mode=%d, mission=%d", sv_gamemode,
                sv_gamemission);
    }

    // Check the connecting client is playing the same game as all
    // the other clients
    if (data.gamemode != sv_gamemode || data.gamemission != sv_gamemission)
    {
        char msg[128];
        NW_Log("server: wrong mode/mission, %d != %d || %d != %d",
                data.gamemode, sv_gamemode, data.gamemission, sv_gamemission);
        /*
        M_snprintf(msg, sizeof(msg),
                   "Game mismatch: server is %s (%s), client is %s (%s)",
                   D_GameMissionString(sv_gamemission),
                   D_GameModeString(sv_gamemode),
                   D_GameMissionString(data.gamemission),
                   D_GameModeString(data.gamemode));
        */
        M_snprintf(msg, sizeof(msg),
                   "Game mismatch: server is %d (%d), client is %d (%d)",
                   sv_gamemission, sv_gamemode, data.gamemission,
                   data.gamemode);

        NW_SV_SendReject(addr, msg);
        return;
    }

    // Allocate a client slot if there isn't one already
    if (client == NULL)
    {
        // find a slot, or return if none found

        for (i = 0; i < MAXNETNODES; ++i)
        {
            if (!clients[i].active)
            {
                client = &clients[i];
                break;
            }
        }

        if (client == NULL)
        {
            return;
        }
    }
    else
    {
        // If this is a recently-disconnected client, deactivate
        // to allow immediate reconnection

        if (client->connection.state == NW_CONN_STATE_DISCONNECTED)
        {
            client->active = false;
        }
    }

    // Client already connected?
    if (client->active)
    {
        NW_Log("server: client is already initialized (duplicate SYN?)");
        return;
    }

    // Activate, initialize connection
    NW_SV_InitNewClient(client, addr, protocol);

    // Save the SHA1 checksums and other details.
    memcpy(client->wad_sha1sum, data.wad_sha1sum, sizeof(sha1_digest_t));
    memcpy(client->deh_sha1sum, data.deh_sha1sum, sizeof(sha1_digest_t));
    client->is_freedoom = data.is_freedoom;
    client->max_players = data.max_players;
    client->name = M_StringDuplicate(player_name);
    client->recording_lowres = data.lowres_turn;
    client->drone = data.drone;
    client->player_class = data.player_class;

    // Send a reply back to the client, indicating a successful connection
    // and specifying the protocol that will be used for communications.
    reply = NW_Conn_NewReliable(&client->connection, NW_PACKET_TYPE_SYN);
    NW_WriteString(reply, PROJECT_STRING);
    NW_WriteProtocol(reply, protocol);
}

// Parse a launch packet. This is sent by the key player when the "start"
// button is pressed, and causes the startup process to continue.

static void NW_SV_ParseLaunch(nw_packet_t *packet, nw_client_t *client)
{
    nw_packet_t *launchpacket;
    int num_players;
    unsigned int i;

    NW_Log("server: processing launch packet");

    // Only the controller can launch the game.

    if (client != NW_SV_Controller())
    {
        NW_Log("server: error: this client isn't the controller, %p != %p",
                (void *)client, (void *)NW_SV_Controller());
        return;
    }

    // Can only launch when we are in the waiting state.

    if (server_state != SERVER_WAITING_LAUNCH)
    {
        NW_Log("server: error: not in waiting launch state, state=%d",
                server_state);
        return;
    }

    // Forward launch on to all clients.
    NW_Log("server: sending launch to all clients");
    NW_SV_AssignPlayers();
    num_players = NW_SV_NumPlayers();

    for (i = 0; i < MAXNETNODES; ++i)
    {
        if (!ClientConnected(&clients[i]))
        {
            continue;
        }

        launchpacket = NW_Conn_NewReliable(&clients[i].connection,
                                            NW_PACKET_TYPE_LAUNCH);
        NW_WriteInt8(launchpacket, num_players);
    }

    // Now in launch state.

    server_state = SERVER_WAITING_START;
}

// Transition to the in-game state and send all players the start game
// message. Invoked once all players have indicated they are ready to
// start the game.

static void StartGame(void)
{
    nw_packet_t *startpacket;
    unsigned int i;
    int nowtime;

    // Assign player numbers

    NW_SV_AssignPlayers();

    // Check if anyone is recording a demo and set lowres_turn if so.

    sv_settings.lowres_turn = false;

    for (i = 0; i < NW_MAXPLAYERS; ++i)
    {
        if (sv_players[i] != NULL && sv_players[i]->recording_lowres)
        {
            sv_settings.lowres_turn = true;
        }
    }

    sv_settings.num_players = NW_SV_NumPlayers();

    // Copy player classes:

    for (i = 0; i < NW_MAXPLAYERS; ++i)
    {
        if (sv_players[i] != NULL)
        {
            sv_settings.player_classes[i] = sv_players[i]->player_class;
        }
        else
        {
            sv_settings.player_classes[i] = 0;
        }
    }

    nowtime = I_GetTimeMS();

    // Send start packets to each connected node

    for (i = 0; i < MAXNETNODES; ++i)
    {
        if (!ClientConnected(&clients[i]))
        {
            continue;
        }

        clients[i].last_gamedata_time = nowtime;

        startpacket = NW_Conn_NewReliable(&clients[i].connection,
                                           NW_PACKET_TYPE_GAMESTART);

        sv_settings.consoleplayer = clients[i].player_number;

        NW_WriteSettings(startpacket, &sv_settings);
    }

    // Change server state
    NW_Log("server: beginning game state");
    server_state = SERVER_IN_GAME;

    memset(recvwindow, 0, sizeof(recvwindow));
    recvwindow_start = 0;
}

// Returns true when all nodes have indicated readiness to start the game.

static boolean AllNodesReady(void)
{
    unsigned int i;

    for (i = 0; i < MAXNETNODES; ++i)
    {
        if (ClientConnected(&clients[i]) && !clients[i].ready)
        {
            return false;
        }
    }

    return true;
}

// Check if the game should start, and if so, start it.

static void CheckStartGame(void)
{
    if (!AllNodesReady())
    {
        NW_Log("server: not all clients ready to start yet");
        return;
    }

    NW_Log("server: all clients ready, starting game");
    StartGame();
}

// Send waiting data with current status to all nodes that are ready to
// start the game.

static void SendAllWaitingData(void)
{
    unsigned int i;

    for (i = 0; i < MAXNETNODES; ++i)
    {
        if (ClientConnected(&clients[i]) && clients[i].ready)
        {
            NW_SV_SendWaitingData(&clients[i]);
        }
    }
}

// Parse a game start packet

static void NW_SV_ParseGameStart(nw_packet_t *packet, nw_client_t *client)
{
    nw_gamesettings_t settings;

    NW_Log("server: processing game start packet");

    // Can only start a game if we are in the waiting start state.

    if (server_state != SERVER_WAITING_START)
    {
        NW_Log("server: error: not in waiting start state, server_state=%d",
                server_state);
        return;
    }

    if (client == NW_SV_Controller())
    {
        if (!NW_ReadSettings(packet, &settings))
        {
            // Malformed packet
            NW_Log("server: error: no settings from controller");
            return;
        }

        // Check the game settings are valid

        if (!NW_ValidGameSettings(sv_gamemode, sv_gamemission, &settings))
        {
            NW_Log("server: error: invalid game settings");
            return;
        }

        sv_settings = settings;
    }

    client->ready = true;

    CheckStartGame();

    // Update all ready clients with the current state (number of players
    // ready, etc.). This is used by games that show startup progress
    // (eg. Hexen's spinal loading)

    SendAllWaitingData();
}

// Send a resend request to a client

static void NW_SV_SendResendRequest(nw_client_t *client, int start, int end)
{
    nw_packet_t *packet;
    nw_client_recv_t *recvobj;
    int i;
    unsigned int nowtime;
    int index;

    NW_Log("server: send resend to %s for tics %d-%d",
            NW_AddrToString(client->addr), start, end);

    packet = NW_NewPacket(20);

    NW_WriteInt16(packet, NW_PACKET_TYPE_GAMEDATA_RESEND);
    NW_WriteInt32(packet, start);
    NW_WriteInt8(packet, end - start + 1);

    NW_Conn_SendPacket(&client->connection, packet);
    NW_FreePacket(packet);

    // Store the time we send the resend request

    nowtime = I_GetTimeMS();

    for (i = start; i <= end; ++i)
    {
        index = i - recvwindow_start;

        if (index >= BACKUPTICS)
        {
            // Outside the range

            continue;
        }

        recvobj = &recvwindow[index][client->player_number];

        recvobj->resend_time = nowtime;
    }
}

// Check for expired resend requests

static void NW_SV_CheckResends(nw_client_t *client)
{
    int i;
    int player;
    int resend_start, resend_end;
    unsigned int nowtime;

    nowtime = I_GetTimeMS();

    player = client->player_number;
    resend_start = -1;
    resend_end = -1;

    for (i = 0; i < BACKUPTICS; ++i)
    {
        nw_client_recv_t *recvobj;
        boolean need_resend;

        recvobj = &recvwindow[i][player];

        // if need_resend is true, this tic needs another retransmit
        // request (300ms timeout)

        need_resend = !recvobj->active && recvobj->resend_time != 0
                      && nowtime > recvobj->resend_time + 300;

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
            NW_Log("server: resend request to %s timed out for %d-%d",
                    NW_AddrToString(client->addr),
                    recvwindow_start + resend_start,
                    recvwindow_start + resend_end);
            //&recvwindow[resend_start][player].resend_time);
            NW_SV_SendResendRequest(client, recvwindow_start + resend_start,
                                     recvwindow_start + resend_end);

            resend_start = -1;
        }
    }

    if (resend_start >= 0)
    {
        NW_Log("server: resend request to %s timed out for %d-%d",
                NW_AddrToString(client->addr), recvwindow_start + resend_start,
                recvwindow_start + resend_end);
        //&recvwindow[resend_start][player].resend_time);
        NW_SV_SendResendRequest(client, recvwindow_start + resend_start,
                                 recvwindow_start + resend_end);
    }
}

// Process game data from a client

static void NW_SV_ParseGameData(nw_packet_t *packet, nw_client_t *client)
{
    nw_client_recv_t *recvobj;
    unsigned int seq;
    unsigned int ackseq;
    unsigned int num_tics;
    unsigned int nowtime;
    unsigned long i;
    int player;
    int resend_start, resend_end;
    int index;

    if (server_state != SERVER_IN_GAME)
    {
        NW_Log("server: error: not in game state: server_state=%d",
                server_state);
        return;
    }

    if (client->drone)
    {
        // Drones do not contribute any game data.
        NW_Log("server: error: game data from a drone?");
        return;
    }

    player = client->player_number;

    // Read header
    if (!NW_ReadInt8(packet, &ackseq) || !NW_ReadInt8(packet, &seq)
        || !NW_ReadInt8(packet, &num_tics))
    {
        NW_Log("server: error: failed to read header");
        return;
    }

    NW_Log("server: got game data, seq=%d, num_tics=%d, ackseq=%d", seq,
            num_tics, ackseq);

    // Get the current time
    nowtime = I_GetTimeMS();

    // Expand 8-bit values to the full sequence number
    ackseq = NW_SV_ExpandTicNum(ackseq);
    seq = NW_SV_ExpandTicNum(seq);

    // Sanity checks

    for (i = 0; i < num_tics; ++i)
    {
        nw_ticdiff_t diff;
        signed int latency;

        if (!NW_ReadSInt16(packet, &latency)
            || !NW_ReadTiccmdDiff(packet, &diff, sv_settings.lowres_turn))
        {
            return;
        }

        index = seq + i - recvwindow_start;

        if (index < 0 || index >= BACKUPTICS)
        {
            // Not in range of the recv window

            continue;
        }

        recvobj = &recvwindow[index][player];
        recvobj->active = true;
        recvobj->diff = diff;
        recvobj->latency = latency;

        client->last_gamedata_time = nowtime;
        NW_Log("server: stored tic %lu for player %d", (unsigned long)seq + i,
                player);
    }

    // Higher acknowledgement point?

    if (ackseq > client->acknowledged)
    {
        NW_Log("server: acknowledged up to %d", ackseq);
        client->acknowledged = ackseq;
    }

    // Has this been received out of sequence, ie. have we not received
    // all tics before the first tic in this packet?  If so, send a
    // resend request.

    // printf("SV: %p: %i\n", client, seq);

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
        recvobj = &recvwindow[index][player];

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
        NW_Log("server: request resend for %d-%d before %d",
                recvwindow_start + resend_start,
                recvwindow_start + resend_end - 1, seq);
        NW_SV_SendResendRequest(client, recvwindow_start + resend_start,
                                 recvwindow_start + resend_end - 1);
    }
}

static void NW_SV_ParseGameDataACK(nw_packet_t *packet, nw_client_t *client)
{
    unsigned int ackseq;

    NW_Log("server: processing game data ack packet");

    if (server_state != SERVER_IN_GAME)
    {
        NW_Log("server: error: not in game state, server_state=%d",
                server_state);
        return;
    }

    // Read header

    if (!NW_ReadInt8(packet, &ackseq))
    {
        NW_Log("server: error: missing acknowledgement field");
        return;
    }

    // Expand 8-bit values to the full sequence number

    ackseq = NW_SV_ExpandTicNum(ackseq);

    // Higher acknowledgement point than we already have?

    if (ackseq > client->acknowledged)
    {
        NW_Log("server: acknowledged up to %d", ackseq);
        client->acknowledged = ackseq;
    }
}

static void NW_SV_SendTics(nw_client_t *client, unsigned int start,
                            unsigned int end)
{
    nw_packet_t *packet;
    unsigned int i;

    packet = NW_NewPacket(500);

    NW_WriteInt16(packet, NW_PACKET_TYPE_GAMEDATA);

    // Send the start tic and number of tics

    NW_WriteInt8(packet, start & 0xff);
    NW_WriteInt8(packet, end - start + 1);

    // Write the tics

    for (i = start; i <= end; ++i)
    {
        nw_full_ticcmd_t *cmd;

        cmd = &client->sendqueue[i % BACKUPTICS];

        if (i != cmd->seq)
        {
            I_Error("Wanted to send %i, but %i is in its place", i, cmd->seq);
        }

        // Add command

        NW_WriteFullTiccmd(packet, cmd, sv_settings.lowres_turn);
    }

    // Send packet

    NW_Conn_SendPacket(&client->connection, packet);

    NW_FreePacket(packet);
}

// Parse a retransmission request from a client

static void NW_SV_ParseResendRequest(nw_packet_t *packet,
                                      nw_client_t *client)
{
    unsigned int start, last;
    unsigned int num_tics;
    unsigned int i;

    NW_Log("server: processing resend request");

    // Read the starting tic and number of tics

    if (!NW_ReadInt32(packet, &start) || !NW_ReadInt8(packet, &num_tics))
    {
        NW_Log("server: error: missing fields for resend");
        return;
    }

    // printf("SV: %p: resend %i-%i\n", client, start, start+num_tics-1);

    // Check we have all the requested tics

    last = start + num_tics - 1;

    for (i = start; i <= last; ++i)
    {
        nw_full_ticcmd_t *cmd;

        cmd = &client->sendqueue[i % BACKUPTICS];

        if (i != cmd->seq)
        {
            // We do not have the requested tic (any more)
            // This is pretty fatal.  We could disconnect the client,
            // but then again this could be a spoofed packet.  Just
            // ignore it.
            NW_Log("server: error: don't have tic %d any more, can't resend",
                    i);
            return;
        }
    }

    // Resend those tics
    NW_Log("server: resending tics %d-%d", start, last);
    NW_SV_SendTics(client, start, last);
}

// Send a response back to the client

void NW_SV_SendQueryResponse(nw_addr_t *addr)
{
    nw_packet_t *reply;
    nw_querydata_t querydata;
    int p;

    // Version

    querydata.version = PROJECT_STRING;

    // Server state

    querydata.server_state = server_state;

    // Number of players/maximum players

    querydata.num_players = NW_SV_NumPlayers();
    querydata.max_players = NW_SV_MaxPlayers();

    // Game mode/mission

    querydata.gamemode = sv_gamemode;
    querydata.gamemission = sv_gamemission;

    //!
    // @category net
    // @arg <name>
    //
    // When starting a network server, specify a name for the server.
    //

    p = M_CheckParmWithArgs("-servername", 1);

    if (p > 0)
    {
        querydata.description = myargv[p + 1];
    }
    else
    {
        querydata.description = "Unnamed server";
    }

    // Send it and we're done.
    NW_Log("server: sending query response to %s", NW_AddrToString(addr));
    reply = NW_NewPacket(64);
    NW_WriteInt16(reply, NW_PACKET_TYPE_QUERY_RESPONSE);
    NW_WriteQueryData(reply, &querydata);
    NW_SendPacket(addr, reply);
    NW_FreePacket(reply);
}

static void NW_SV_ParseHolePunch(nw_packet_t *packet)
{
    const char *addr_string;
    nw_packet_t *sendpacket;
    nw_addr_t *addr;

    addr_string = NW_ReadString(packet);
    if (addr_string == NULL)
    {
        NW_Log("server: error: hole punch request but no address provided");
        return;
    }

    addr = NW_ResolveAddress(server_context, addr_string);
    if (addr == NULL)
    {
        NW_Log("server: error: failed to resolve address: %s", addr_string);
        return;
    }

    sendpacket = NW_NewPacket(16);
    NW_WriteInt16(sendpacket, NW_PACKET_TYPE_NAT_HOLE_PUNCH);
    NW_SendPacket(addr, sendpacket);
    NW_FreePacket(sendpacket);
    NW_ReleaseAddress(addr);
    NW_Log("server: sent hole punch to %s", addr_string);
}

static void NW_SV_MasterPacket(nw_packet_t *packet)
{
    unsigned int packet_type;

    // Read the packet type

    if (!NW_ReadInt16(packet, &packet_type))
    {
        NW_Log("server: error: no packet type in master server message");
        return;
    }

    NW_Log("server: packet from master server; type %d", packet_type);
    NW_LogPacket(packet);

    switch (packet_type)
    {
        case NW_MASTER_PACKET_TYPE_ADD_RESPONSE:
            NW_Query_AddResponse(packet);
            break;

        case NW_MASTER_PACKET_TYPE_NAT_HOLE_PUNCH:
            NW_SV_ParseHolePunch(packet);
            break;
    }
}

// Process a packet received by the server

static void NW_SV_Packet(nw_packet_t *packet, nw_addr_t *addr)
{
    nw_client_t *client;
    unsigned int packet_type;

    // Response from master server?

    if (addr != NULL && addr == master_server)
    {
        NW_SV_MasterPacket(packet);
        return;
    }

    // Find which client this packet came from

    client = NW_SV_FindClient(addr);

    // Read the packet type

    if (!NW_ReadInt16(packet, &packet_type))
    {
        // no packet type

        return;
    }

    NW_Log("server: packet from %s; type %d", NW_AddrToString(addr),
            packet_type & ~NW_RELIABLE_PACKET);
    NW_LogPacket(packet);

    if (packet_type == NW_PACKET_TYPE_SYN)
    {
        NW_SV_ParseSYN(packet, client, addr);
    }
    else if (packet_type == NW_PACKET_TYPE_QUERY)
    {
        NW_SV_SendQueryResponse(addr);
    }
    else if (client == NULL)
    {
        // Must come from a valid client; ignore otherwise
    }
    else if (NW_Conn_Packet(&client->connection, packet, &packet_type))
    {
        // Packet was eaten by the common connection code
    }
    else
    {
        // printf("SV: %s: %i\n", NW_AddrToString(addr), packet_type);

        switch (packet_type)
        {
            case NW_PACKET_TYPE_GAMESTART:
                NW_SV_ParseGameStart(packet, client);
                break;
            case NW_PACKET_TYPE_LAUNCH:
                NW_SV_ParseLaunch(packet, client);
                break;
            case NW_PACKET_TYPE_GAMEDATA:
                NW_SV_ParseGameData(packet, client);
                break;
            case NW_PACKET_TYPE_GAMEDATA_ACK:
                NW_SV_ParseGameDataACK(packet, client);
                break;
            case NW_PACKET_TYPE_GAMEDATA_RESEND:
                NW_SV_ParseResendRequest(packet, client);
                break;
            default:
                // unknown packet type

                break;
        }
    }
}

static void NW_SV_PumpSendQueue(nw_client_t *client)
{
    nw_full_ticcmd_t cmd;
    int recv_index;
    int num_players;
    int i;
    int starttic, endtic;

    // If a client has not sent any acknowledgments for a while,
    // wait until they catch up.

    if (client->sendseq - NW_SV_LatestAcknowledged() > 40)
    {
        return;
    }

    // Work out the index into the receive window

    recv_index = client->sendseq - recvwindow_start;

    if (recv_index < 0 || recv_index >= BACKUPTICS)
    {
        return;
    }

    // Check if we can generate a new entry for the send queue
    // using the data in recvwindow.

    num_players = 0;

    for (i = 0; i < NW_MAXPLAYERS; ++i)
    {
        if (sv_players[i] == client)
        {
            // Client does not rely on itself for data

            continue;
        }

        if (sv_players[i] == NULL || !ClientConnected(sv_players[i]))
        {
            continue;
        }

        if (!recvwindow[recv_index][i].active)
        {
            // We do not have this player's ticcmd, so we cannot
            // generate a complete command yet.

            return;
        }

        ++num_players;
    }

    // If this is a game with only a single player in it, we might
    // be sending a ticcmd set containing 0 ticcmds. This is fine;
    // however, there's nothing to stop the game running on ahead
    // and never stopping. Don't let the server get too far ahead
    // of the client.

    if (num_players == 0 && client->sendseq > recvwindow_start + 10)
    {
        return;
    }

    // We have all data we need to generate a command for this tic.

    cmd.seq = client->sendseq;

    // Add ticcmds from all players

    cmd.latency = 0;

    for (i = 0; i < NW_MAXPLAYERS; ++i)
    {
        nw_client_recv_t *recvobj;

        if (sv_players[i] == client)
        {
            // Not the player we are sending to

            cmd.playeringame[i] = false;
            continue;
        }

        if (sv_players[i] == NULL || !recvwindow[recv_index][i].active)
        {
            cmd.playeringame[i] = false;
            continue;
        }

        cmd.playeringame[i] = true;

        recvobj = &recvwindow[recv_index][i];

        cmd.cmds[i] = recvobj->diff;

        if (recvobj->latency > cmd.latency)
        {
            cmd.latency = recvobj->latency;
        }
    }

    // printf("SV: %i: latency %i\n", client->player_number, cmd.latency);

    // Add into the queue

    client->sendqueue[client->sendseq % BACKUPTICS] = cmd;

    // Transmit the new tic to the client

    starttic = client->sendseq - sv_settings.extratics;
    endtic = client->sendseq;

    if (starttic < 0)
    {
        starttic = 0;
    }

    NW_Log("server: send tics %d-%d to %s", starttic, endtic,
            NW_AddrToString(client->addr));
    NW_SV_SendTics(client, starttic, endtic);

    ++client->sendseq;
}

// Prevent against deadlock: resend requests are usually only
// triggered if we miss a packet and receive the next one.
// If we miss a whole load of packets, we can end up in a
// deadlock situation where the client will not send any more.
// If we don't receive any game data in a while, trigger a resend
// request for the next tic we're expecting.

void NW_SV_CheckDeadlock(nw_client_t *client)
{
    int nowtime;
    int i;

    // Don't expect game data from clients.

    if (client->drone)
    {
        return;
    }

    nowtime = I_GetTimeMS();

    // If we haven't received anything for a long time, it may be a deadlock.

    if (nowtime - client->last_gamedata_time > 1000)
    {
        NW_Log("server: no gamedata from %s since %d - deadlock?",
                NW_AddrToString(client->addr), client->last_gamedata_time);

        // Search the receive window for the first tic we are expecting
        // from this player.

        for (i = 0; i < BACKUPTICS; ++i)
        {
            if (!recvwindow[i][client->player_number].active)
            {
                NW_Log("server: deadlock: sending resend request for %d-%d",
                        recvwindow_start + i, recvwindow_start + i + 5);

                // Found a tic we haven't received.  Send a resend request.

                NW_SV_SendResendRequest(client, recvwindow_start + i,
                                         recvwindow_start + i + 5);

                client->last_gamedata_time = nowtime;
                break;
            }
        }

        // If we sent a resend request to break the deadlock, also trigger a
        // resend of any tics we have sitting in the send queue, in case the
        // client is blocked waiting on tics from us that have been lost.
        // This fixes deadlock with some older clients which do not send
        // resends to break deadlock.
        if (i < BACKUPTICS && client->sendseq > client->acknowledged)
        {
            NW_Log("server: also resending tics %d-%d to break deadlock",
                    client->acknowledged, client->sendseq - 1);
            NW_SV_SendTics(client, client->acknowledged, client->sendseq - 1);
        }
    }
}

// Called when all players have disconnected.  Return to listening for
// players to start a new game, and disconnect any drones still connected.

static void NW_SV_GameEnded(void)
{
    int i;

    server_state = SERVER_WAITING_LAUNCH;
    sv_gamemode = indetermined;

    for (i = 0; i < MAXNETNODES; ++i)
    {
        if (clients[i].active)
        {
            NW_SV_DisconnectClient(&clients[i]);
        }
    }
}

// Perform any needed action on a client

static void NW_SV_RunClient(nw_client_t *client)
{
    // Run common code

    NW_Conn_Run(&client->connection);

    if (client->connection.state == NW_CONN_STATE_DISCONNECTED
        && client->connection.disconnect_reason == NW_DISCONNECT_TIMEOUT)
    {
        NW_Log("server: client at %s timed out",
                NW_AddrToString(client->addr));
        NW_SV_BroadcastMessage("Client '%s' timed out and disconnected",
                                client->name);
    }

    // Is this client disconnected?

    if (client->connection.state == NW_CONN_STATE_DISCONNECTED)
    {
        client->active = false;

        // If we were about to start a game, any player disconnecting
        // should cause an abort.

        if (server_state == SERVER_WAITING_START && !client->drone)
        {
            NW_SV_BroadcastMessage("Game startup aborted because "
                                    "player '%s' disconnected.",
                                    client->name);
            NW_SV_GameEnded();
        }

        free(client->name);
        NW_ReleaseAddress(client->addr);

        // Are there any clients left connected?  If not, return the
        // server to the waiting-for-players state.
        //
        // Disconnect any drones still connected.

        if (NW_SV_NumPlayers() <= 0)
        {
            NW_Log("server: no player clients left, game ended");
            NW_SV_GameEnded();
        }
    }

    if (!ClientConnected(client))
    {
        // client has not yet finished connecting

        return;
    }

    if (server_state == SERVER_WAITING_LAUNCH)
    {
        // Waiting for the game to start

        // Send information once every second

        if (client->last_send_time < 0
            || I_GetTimeMS() - client->last_send_time > 1000)
        {
            NW_SV_SendWaitingData(client);
            client->last_send_time = I_GetTimeMS();
        }
    }

    if (server_state == SERVER_IN_GAME)
    {
        NW_SV_PumpSendQueue(client);
        NW_SV_CheckDeadlock(client);
    }
}

// Add a network module to the server context

void NW_SV_AddModule(nw_module_t *module)
{
    module->InitServer();
    NW_AddModule(server_context, module);
}

// Initialize server and wait for connections

void NW_SV_Init(void)
{
    int i;

    // initialize send/receive context

    server_context = NW_NewContext();

    // no clients yet

    for (i = 0; i < MAXNETNODES; ++i)
    {
        clients[i].active = false;
    }

    NW_SV_AssignPlayers();

    server_state = SERVER_WAITING_LAUNCH;
    sv_gamemode = indetermined;
    server_initialized = true;
}

static void UpdateMasterServer(void)
{
    unsigned int now;

    now = I_GetTimeMS();

    // The address of the master server can change. Periodically
    // re-resolve the master server to update.

    if (now - master_resolve_time > MASTER_RESOLVE_PERIOD * 1000)
    {
        nw_addr_t *new_addr;

        new_addr = NW_Query_ResolveMaster(server_context);
        NW_ReleaseAddress(master_server);
        master_server = new_addr;

        master_resolve_time = now;
    }

    // Possibly refresh our registration with the master server.

    if (now - master_refresh_time > MASTER_REFRESH_PERIOD * 1000)
    {
        NW_Query_AddToMaster(master_server);
        master_refresh_time = now;
    }
}

void NW_SV_RegisterWithMaster(void)
{
    //!
    // @category net
    // @help
    //
    // When running a server, don't register with the global master server.
    // Implies -server.
    //

    if (!M_CheckParm("-privateserver"))
    {
        master_server = NW_Query_ResolveMaster(server_context);
    }
    else
    {
        master_server = NULL;
    }

    // Send request.

    if (master_server != NULL)
    {
        NW_Query_AddToMaster(master_server);
        master_refresh_time = I_GetTimeMS();
        master_resolve_time = master_refresh_time;
    }
}

// Run server code to check for new packets/send packets as the server
// requires

void NW_SV_Run(void)
{
    nw_addr_t *addr;
    nw_packet_t *packet;
    int i;

    if (!server_initialized)
    {
        return;
    }

    while (NW_RecvPacket(server_context, &addr, &packet))
    {
        NW_SV_Packet(packet, addr);
        NW_FreePacket(packet);
        NW_ReleaseAddress(addr);
    }

    if (master_server != NULL)
    {
        UpdateMasterServer();
    }

    // "Run" any clients that may have things to do, independent of responses
    // to received packets

    for (i = 0; i < MAXNETNODES; ++i)
    {
        if (clients[i].active)
        {
            NW_SV_RunClient(&clients[i]);
        }
    }

    switch (server_state)
    {
        case SERVER_WAITING_LAUNCH:
            break;

        case SERVER_WAITING_START:
            CheckStartGame();
            break;

        case SERVER_IN_GAME:
            NW_SV_AdvanceWindow();

            for (i = 0; i < NW_MAXPLAYERS; ++i)
            {
                if (sv_players[i] != NULL && ClientConnected(sv_players[i]))
                {
                    NW_SV_CheckResends(sv_players[i]);
                }
            }
            break;
    }
}

void NW_SV_Shutdown(void)
{
    int i;
    boolean running;
    int start_time;

    if (!server_initialized)
    {
        return;
    }

    I_Printf(VB_WARNING, "SV: Shutting down server...");

    // Disconnect all clients

    for (i = 0; i < MAXNETNODES; ++i)
    {
        if (clients[i].active)
        {
            NW_SV_DisconnectClient(&clients[i]);
        }
    }

    // Wait for all clients to finish disconnecting

    start_time = I_GetTimeMS();
    running = true;

    while (running)
    {
        // Check if any clients are still not finished

        running = false;

        for (i = 0; i < MAXNETNODES; ++i)
        {
            if (clients[i].active)
            {
                running = true;
            }
        }

        // Timed out?

        if (I_GetTimeMS() - start_time > 5000)
        {
            running = false;
            I_Printf(VB_WARNING,
                     "SV: Timed out waiting for clients to disconnect.");
        }

        // Run the client code in case this is a loopback client.

        NW_CL_Run();
        NW_SV_Run();

        // Don't hog the CPU

        I_Sleep(1);
    }
}
