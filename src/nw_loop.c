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
//      Loopback network module for server compiled into the client
//

#include <stdio.h>

#include "doomtype.h"
#include "i_system.h"
#include "m_misc.h"
#include "nw_defs.h"
#include "nw_loop.h"
#include "nw_packet.h"

#define MAX_QUEUE_SIZE 16

typedef struct
{
    nw_packet_t *packets[MAX_QUEUE_SIZE];
    int head, tail;
} packet_queue_t;

static packet_queue_t client_queue;
static packet_queue_t server_queue;
static nw_addr_t client_addr;
static nw_addr_t server_addr;

static void QueueInit(packet_queue_t *queue)
{
    queue->head = queue->tail = 0;
}

static void QueuePush(packet_queue_t *queue, nw_packet_t *packet)
{
    int new_tail;

    new_tail = (queue->tail + 1) % MAX_QUEUE_SIZE;

    if (new_tail == queue->head)
    {
        // queue is full

        return;
    }

    queue->packets[queue->tail] = packet;
    queue->tail = new_tail;
}

static nw_packet_t *QueuePop(packet_queue_t *queue)
{
    nw_packet_t *packet;

    if (queue->tail == queue->head)
    {
        // queue empty

        return NULL;
    }

    packet = queue->packets[queue->head];
    queue->head = (queue->head + 1) % MAX_QUEUE_SIZE;

    return packet;
}

//-----------------------------------------------------------------------------
//
// Client end code
//
//-----------------------------------------------------------------------------

static boolean NW_CL_InitClient(void)
{
    QueueInit(&client_queue);

    return true;
}

static boolean NW_CL_InitServer(void)
{
    I_Error("attempted to initialize client pipe end as a server!");
    return false;
}

static void NW_CL_SendPacket(nw_addr_t *addr, nw_packet_t *packet)
{
    QueuePush(&server_queue, NW_PacketDup(packet));
}

static boolean NW_CL_RecvPacket(nw_addr_t **addr, nw_packet_t **packet)
{
    nw_packet_t *popped;

    popped = QueuePop(&client_queue);

    if (popped != NULL)
    {
        *packet = popped;
        *addr = &client_addr;
        client_addr.module = &nw_loop_client_module;

        return true;
    }

    return false;
}

static void NW_CL_AddrToString(nw_addr_t *addr, char *buffer, int buffer_len)
{
    M_snprintf(buffer, buffer_len, "local server");
}

static void NW_CL_FreeAddress(nw_addr_t *addr)
{
}

static nw_addr_t *NW_CL_ResolveAddress(const char *address)
{
    if (address == NULL)
    {
        client_addr.module = &nw_loop_client_module;

        return &client_addr;
    }
    else
    {
        return NULL;
    }
}

nw_module_t nw_loop_client_module =
{
    NW_CL_InitClient,
    NW_CL_InitServer,
    NW_CL_SendPacket,
    NW_CL_RecvPacket,
    NW_CL_AddrToString,
    NW_CL_FreeAddress,
    NW_CL_ResolveAddress,
};

//-----------------------------------------------------------------------------
//
// Server end code
//
//-----------------------------------------------------------------------------

static boolean NW_SV_InitClient(void)
{
    I_Error("attempted to initialize server pipe end as a client!");
    return false;
}

static boolean NW_SV_InitServer(void)
{
    QueueInit(&server_queue);

    return true;
}

static void NW_SV_SendPacket(nw_addr_t *addr, nw_packet_t *packet)
{
    QueuePush(&client_queue, NW_PacketDup(packet));
}

static boolean NW_SV_RecvPacket(nw_addr_t **addr, nw_packet_t **packet)
{
    nw_packet_t *popped;

    popped = QueuePop(&server_queue);

    if (popped != NULL)
    {
        *packet = popped;
        *addr = &server_addr;
        server_addr.module = &nw_loop_server_module;

        return true;
    }

    return false;
}

static void NW_SV_AddrToString(nw_addr_t *addr, char *buffer, int buffer_len)
{
    M_snprintf(buffer, buffer_len, "local client");
}

static void NW_SV_FreeAddress(nw_addr_t *addr)
{
}

static nw_addr_t *NW_SV_ResolveAddress(const char *address)
{
    if (address == NULL)
    {
        server_addr.module = &nw_loop_server_module;
        return &server_addr;
    }
    else
    {
        return NULL;
    }
}

nw_module_t nw_loop_server_module =
{
    NW_SV_InitClient,
    NW_SV_InitServer,
    NW_SV_SendPacket,
    NW_SV_RecvPacket,
    NW_SV_AddrToString,
    NW_SV_FreeAddress,
    NW_SV_ResolveAddress,
};
