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
// Common code shared between the client and server
//

#ifndef NW_COMMON_H
#define NW_COMMON_H

#include "doomdef.h"
#include "doomtype.h"
#include "nw_defs.h"

typedef enum
{
    // Client has sent a SYN, is waiting for a SYN in response.
    NW_CONN_STATE_CONNECTING,

    // Successfully connected.
    NW_CONN_STATE_CONNECTED,

    // Sent a DISCONNECT packet, waiting for a DISCONNECT_ACK reply
    NW_CONN_STATE_DISCONNECTING,

    // Client successfully disconnected
    NW_CONN_STATE_DISCONNECTED,

    // We are disconnected, but in a sleep state, waiting for several
    // seconds.  This is in case the DISCONNECT_ACK we sent failed
    // to arrive, and we need to send another one.  We keep this as
    // a valid connection for a few seconds until we are sure that
    // the other end has successfully disconnected as well.
    NW_CONN_STATE_DISCONNECTED_SLEEP,

} nw_connstate_t;

// Reason a connection was terminated

typedef enum
{
    // As the result of a local disconnect request

    NW_DISCONNECT_LOCAL,

    // As the result of a remote disconnect request

    NW_DISCONNECT_REMOTE,

    // Timeout (no data received in a long time)

    NW_DISCONNECT_TIMEOUT,

} nw_disconnect_reason_t;

#define MAX_RETRIES 5

typedef struct nw_reliable_packet_s nw_reliable_packet_t;

typedef struct
{
    nw_connstate_t state;
    nw_disconnect_reason_t disconnect_reason;
    nw_addr_t *addr;
    nw_protocol_t protocol;
    int last_send_time;
    int num_retries;
    int keepalive_send_time;
    int keepalive_recv_time;
    nw_reliable_packet_t *reliable_packets;
    int reliable_send_seq;
    int reliable_recv_seq;
} nw_connection_t;

void NW_Conn_SendPacket(nw_connection_t *conn, nw_packet_t *packet);
void NW_Conn_InitClient(nw_connection_t *conn, nw_addr_t *addr,
                         nw_protocol_t protocol);
void NW_Conn_InitServer(nw_connection_t *conn, nw_addr_t *addr,
                         nw_protocol_t protocol);
boolean NW_Conn_Packet(nw_connection_t *conn, nw_packet_t *packet,
                        unsigned int *packet_type);
void NW_Conn_Disconnect(nw_connection_t *conn);
void NW_Conn_Run(nw_connection_t *conn);
nw_packet_t *NW_Conn_NewReliable(nw_connection_t *conn, int packet_type);

// Other miscellaneous common functions
unsigned int NW_ExpandTicNum(unsigned int relative, unsigned int b);
boolean NW_ValidGameSettings(GameMode_t mode, GameMission_t mission,
                              nw_gamesettings_t *settings);

void NW_OpenLog(void);
void NW_Log(const char *fmt, ...) PRINTF_ATTR(1, 2);
void NW_LogPacket(nw_packet_t *packet);

#endif /* #ifndef NW_COMMON_H */
