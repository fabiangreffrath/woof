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
// DESCRIPTION:
//     Definitions for use in networking code.
//

#ifndef NW_DEFS_H
#define NW_DEFS_H

#include "d_ticcmd.h"
#include "doomtype.h"

typedef byte sha1_digest_t[20];

// Absolute maximum number of "nodes" in the game.  This is different to
// NW_MAXPLAYERS, as there may be observers that are not participating
// (eg. left/right monitors)

#define MAXNETNODES    16

// The maximum number of players, multiplayer/networking.
// This is the maximum supported by the networking code; individual games
// have their own values for MAXPLAYERS that can be smaller.

#define NW_MAXPLAYERS 8

// Maximum length of a player's name.

#define MAXPLAYERNAME  30

// Networking and tick handling related.

#define BACKUPTICS     128

typedef struct _nw_module_s nw_module_t;
typedef struct _nw_packet_s nw_packet_t;
typedef struct _nw_addr_s nw_addr_t;
typedef struct _nw_context_s nw_context_t;

struct _nw_packet_s
{
    byte *data;
    size_t len;
    size_t alloced;
    unsigned int pos;
};

struct _nw_module_s
{
    // Initialize this module for use as a client

    boolean (*InitClient)(void);

    // Initialize this module for use as a server

    boolean (*InitServer)(void);

    // Send a packet

    void (*SendPacket)(nw_addr_t *addr, nw_packet_t *packet);

    // Check for new packets to receive
    //
    // Returns true if packet received

    boolean (*RecvPacket)(nw_addr_t **addr, nw_packet_t **packet);

    // Converts an address to a string

    void (*AddrToString)(nw_addr_t *addr, char *buffer, int buffer_len);

    // Free back an address when no longer in use

    void (*FreeAddress)(nw_addr_t *addr);

    // Try to resolve a name to an address

    nw_addr_t *(*ResolveAddress)(const char *addr);

    void (*Shutdown)(void);
};

// nw_addr_t

struct _nw_addr_s
{
    nw_module_t *module;
    int refcount;
    void *handle;
};

// Magic number sent when connecting to check this is a valid client
#define NW_MAGIC_NUMBER     1454104972U

// Old magic number used by Chocolate Doom versions before v3.0:
#define NW_OLD_MAGIC_NUMBER 3436803284U

// header field value indicating that the packet is a reliable packet

#define NW_RELIABLE_PACKET  (1 << 15)

// Supported protocols. If you're developing a fork of Chocolate
// Doom, you can add your own entry to this list while maintaining
// compatibility with Chocolate Doom servers. Higher-numbered enum values
// will be preferred when negotiating a protocol for the client and server
// to use, so the order matters.
// NOTE: The values in this enum do not have any special value outside of
// the program they're compiled in. What matters is the string representation.
typedef enum
{
    // Protocol introduced with Chocolate Doom v3.0. Each compatibility-
    // breaking change to the network protocol will produce a new protocol
    // number in this enum.
    NW_PROTOCOL_CHOCOLATE_DOOM_0,

    // Add your own protocol here; be sure to add a name for it to the list
    // in nw_common.c too.

    NW_NUM_PROTOCOLS,
    NW_PROTOCOL_UNKNOWN,
} nw_protocol_t;

// packet types

typedef enum
{
    NW_PACKET_TYPE_SYN,
    NW_PACKET_TYPE_ACK, // deprecated
    NW_PACKET_TYPE_REJECTED,
    NW_PACKET_TYPE_KEEPALIVE,
    NW_PACKET_TYPE_WAITING_DATA,
    NW_PACKET_TYPE_GAMESTART,
    NW_PACKET_TYPE_GAMEDATA,
    NW_PACKET_TYPE_GAMEDATA_ACK,
    NW_PACKET_TYPE_DISCONNECT,
    NW_PACKET_TYPE_DISCONNECT_ACK,
    NW_PACKET_TYPE_RELIABLE_ACK,
    NW_PACKET_TYPE_GAMEDATA_RESEND,
    NW_PACKET_TYPE_CONSOLE_MESSAGE,
    NW_PACKET_TYPE_QUERY,
    NW_PACKET_TYPE_QUERY_RESPONSE,
    NW_PACKET_TYPE_LAUNCH,
    NW_PACKET_TYPE_NAT_HOLE_PUNCH,
} nw_packet_type_t;

typedef enum
{
    NW_MASTER_PACKET_TYPE_ADD,
    NW_MASTER_PACKET_TYPE_ADD_RESPONSE,
    NW_MASTER_PACKET_TYPE_QUERY,
    NW_MASTER_PACKET_TYPE_QUERY_RESPONSE,
    NW_MASTER_PACKET_TYPE_GET_METADATA,
    NW_MASTER_PACKET_TYPE_GET_METADATA_RESPONSE,
    NW_MASTER_PACKET_TYPE_SIGN_START,
    NW_MASTER_PACKET_TYPE_SIGN_START_RESPONSE,
    NW_MASTER_PACKET_TYPE_SIGN_END,
    NW_MASTER_PACKET_TYPE_SIGN_END_RESPONSE,
    NW_MASTER_PACKET_TYPE_NAT_HOLE_PUNCH,
    NW_MASTER_PACKET_TYPE_NAT_HOLE_PUNCH_ALL,
} nw_master_packet_type_t;

// Settings specified when the client connects to the server.

typedef struct
{
    int gamemode;
    int gamemission;
    int lowres_turn;
    int drone;
    int max_players;
    int is_freedoom;
    sha1_digest_t wad_sha1sum;
    sha1_digest_t deh_sha1sum;
    int player_class;
} nw_connect_data_t;

// Game settings sent by client to server when initiating game start,
// and received from the server by clients when the game starts.

#define NW_GAME_OPTION_SIZE 64

typedef struct
{
    int ticdup;
    int extratics;
    int deathmatch;
    int episode;
    int nomonsters;
    int fast_monsters;
    int respawn_monsters;
    int map;
    int skill;
    int gameversion;
    int lowres_turn;
    int new_sync;
    int timelimit;
    int loadgame;
    int random; // [Strife only]

    // These fields are only used by the server when sending a game
    // start message:

    int num_players;
    int consoleplayer;

    // Hexen player classes:

    int player_classes[NW_MAXPLAYERS];

    // for Boom and higher compatibility

    int demo_version;
    byte options[NW_GAME_OPTION_SIZE];

} nw_gamesettings_t;

#define NW_TICDIFF_FORWARD     (1 << 0)
#define NW_TICDIFF_SIDE        (1 << 1)
#define NW_TICDIFF_TURN        (1 << 2)
#define NW_TICDIFF_BUTTONS     (1 << 3)
#define NW_TICDIFF_CONSISTANCY (1 << 4)
#define NW_TICDIFF_CHATCHAR    (1 << 5)
#define NW_TICDIFF_RAVEN       (1 << 6)
#define NW_TICDIFF_STRIFE      (1 << 7)

typedef struct
{
    unsigned int diff;
    ticcmd_t cmd;
} nw_ticdiff_t;

// Complete set of ticcmds from all players

typedef struct
{
    signed int latency;
    unsigned int seq;
    boolean playeringame[NW_MAXPLAYERS];
    nw_ticdiff_t cmds[NW_MAXPLAYERS];
} nw_full_ticcmd_t;

// Data sent in response to server queries

typedef struct
{
    const char *version;
    int server_state;
    int num_players;
    int max_players;
    int gamemode;
    int gamemission;
    const char *description;
    nw_protocol_t protocol;
} nw_querydata_t;

// Data sent by the server while waiting for the game to start.

typedef struct
{
    int num_players;
    int num_drones;
    int ready_players;
    int max_players;
    int is_controller;
    int consoleplayer;
    char player_names[NW_MAXPLAYERS][MAXPLAYERNAME];
    char player_addrs[NW_MAXPLAYERS][MAXPLAYERNAME];
    sha1_digest_t wad_sha1sum;
    sha1_digest_t deh_sha1sum;
    int is_freedoom;
} nw_waitdata_t;

#endif /* #ifndef NW_DEFS_H */
