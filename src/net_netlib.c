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
//     Networking module which uses netlib
//

#include <stdlib.h>
#include <string.h>

#include "doomtype.h"
#include "i_system.h"
#include "m_argv.h"
#include "m_misc.h"
#include "net_defs.h"
#include "net_io.h"
#include "net_packet.h"
#include "net_netlib.h"
#include "z_zone.h"

//
// NETWORKING
//

#include "netlib.h"

#define DEFAULT_PORT 2342

static boolean initted = false;
static int port = DEFAULT_PORT;
static udp_socket_t udpsocket;
static udp_packet_t *recvpacket;

typedef struct
{
    net_addr_t net_addr;
    ip_address_t netlib_addr;
} addrpair_t;

static addrpair_t **addr_table;
static int addr_table_size = -1;

// Initializes the address table

static void InitAddrTable(void)
{
    addr_table_size = 16;

    addr_table = Z_Malloc(sizeof(addrpair_t *) * addr_table_size, PU_STATIC, 0);
    memset(addr_table, 0, sizeof(addrpair_t *) * addr_table_size);
}

static boolean AddressesEqual(ip_address_t *a, ip_address_t *b)
{
    return a->host == b->host && a->port == b->port;
}

// Finds an address by searching the table.  If the address is not found,
// it is added to the table.

static net_addr_t *FindAddress(ip_address_t *addr)
{
    addrpair_t *new_entry;
    int empty_entry = -1;
    int i;

    if (addr_table_size < 0)
    {
        InitAddrTable();
    }

    for (i = 0; i < addr_table_size; ++i)
    {
        if (addr_table[i] != NULL
            && AddressesEqual(addr, &addr_table[i]->netlib_addr))
        {
            return &addr_table[i]->net_addr;
        }

        if (empty_entry < 0 && addr_table[i] == NULL)
        {
            empty_entry = i;
        }
    }

    // Was not found in list.  We need to add it.

    // Is there any space in the table? If not, increase the table size

    if (empty_entry < 0)
    {
        addrpair_t **new_addr_table;
        int new_addr_table_size;

        // after reallocing, we will add this in as the first entry
        // in the new block of memory

        empty_entry = addr_table_size;

        // allocate a new array twice the size, init to 0 and copy
        // the existing table in.  replace the old table.

        new_addr_table_size = addr_table_size * 2;
        new_addr_table =
            Z_Malloc(sizeof(addrpair_t *) * new_addr_table_size, PU_STATIC, 0);
        memset(new_addr_table, 0, sizeof(addrpair_t *) * new_addr_table_size);
        memcpy(new_addr_table, addr_table,
               sizeof(addrpair_t *) * addr_table_size);
        Z_Free(addr_table);
        addr_table = new_addr_table;
        addr_table_size = new_addr_table_size;
    }

    // Add a new entry

    new_entry = Z_Malloc(sizeof(addrpair_t), PU_STATIC, 0);

    new_entry->netlib_addr = *addr;
    new_entry->net_addr.refcount = 0;
    new_entry->net_addr.handle = &new_entry->netlib_addr;
    new_entry->net_addr.module = &netlib_module;

    addr_table[empty_entry] = new_entry;

    return &new_entry->net_addr;
}

static void NETLIB_FreeAddress(net_addr_t *addr)
{
    int i;

    for (i = 0; i < addr_table_size; ++i)
    {
        if (addr == &addr_table[i]->net_addr)
        {
            Z_Free(addr_table[i]);
            addr_table[i] = NULL;
            return;
        }
    }

    I_Error("Attempted to remove an unused address!");
}

static boolean NETLIB_InitClient(void)
{
    int p;

    if (initted)
    {
        return true;
    }

    //!
    // @category net
    // @arg <n>
    //
    // Use the specified UDP port for communications, instead of
    // the default (2342).
    //

    p = M_CheckParmWithArgs("-port", 1);
    if (p > 0)
    {
        port = M_ParmArgToInt(p);
    }

    if (netlib_init() < 0)
    {
        I_Error("Failed to initialize SDLNet: %s", netlib_get_error());
    }

    udpsocket = netlib_udp_open(0);

    if (udpsocket == NULL)
    {
        I_Error("Unable to open a socket!");
    }

    recvpacket = netlib_alloc_packet(1500);

#ifdef DROP_PACKETS
    srand(time(NULL));
#endif

    initted = true;

    return true;
}

static boolean NETLIB_InitServer(void)
{
    int p;

    if (initted)
    {
        return true;
    }

    p = M_CheckParmWithArgs("-port", 1);
    if (p > 0)
    {
        port = atoi(myargv[p + 1]);
    }

    if (netlib_init() < 0)
    {
        I_Error("Failed to initialize SDLNet: %s", netlib_get_error());
    }

    udpsocket = netlib_udp_open(port);

    if (udpsocket == NULL)
    {
        I_Error("Unable to bind to port %i", port);
    }

    recvpacket = netlib_alloc_packet(1500);
#ifdef DROP_PACKETS
    srand(time(NULL));
#endif

    initted = true;

    return true;
}

static void NETLIB_SendPacket(net_addr_t *addr, net_packet_t *packet)
{
    udp_packet_t netlib_packet;
    ip_address_t ip;

    if (addr == &net_broadcast_addr)
    {
        netlib_resolve_host(&ip, NULL, port);
        ip.host = INADDR_BROADCAST;
    }
    else
    {
        ip = *((ip_address_t *)addr->handle);
    }

#if 0
    {
        static int this_second_sent = 0;
        static int lasttime;

        this_second_sent += packet->len + 64;

        if (I_GetTime() - lasttime > TICRATE)
        {
            printf("%i bytes sent in the last second\n", this_second_sent);
            lasttime = I_GetTime();
            this_second_sent = 0;
        }
    }
#endif

#ifdef DROP_PACKETS
    if ((rand() % 4) == 0)
    {
        return;
    }
#endif

    netlib_packet.channel = 0;
    netlib_packet.data = packet->data;
    netlib_packet.len = packet->len;
    netlib_packet.address = ip;

    if (!netlib_udp_send(udpsocket, -1, &netlib_packet))
    {
        I_Error("Error transmitting packet: %s", netlib_get_error());
    }
}

static boolean NETLIB_RecvPacket(net_addr_t **addr, net_packet_t **packet)
{
    int result;

    result = netlib_udp_recv(udpsocket, recvpacket);

    if (result < 0)
    {
        I_Error("Error receiving packet: %s", netlib_get_error());
    }

    // no packets received

    if (result == 0)
    {
        return false;
    }

    // Put the data into a new packet structure

    *packet = NET_NewPacket(recvpacket->len);
    memcpy((*packet)->data, recvpacket->data, recvpacket->len);
    (*packet)->len = recvpacket->len;

    // Address

    *addr = FindAddress(&recvpacket->address);

    return true;
}

static void NETLIB_AddrToString(net_addr_t *addr, char *buffer, int buffer_len)
{
    ip_address_t *ip;
    uint32_t host;
    uint16_t port;

    ip = (ip_address_t *)addr->handle;
    host = netlib_read32(&ip->host);
    port = netlib_read16(&ip->port);

    M_snprintf(buffer, buffer_len, "%i.%i.%i.%i", (host >> 24) & 0xff,
               (host >> 16) & 0xff, (host >> 8) & 0xff, host & 0xff);

    // If we are using the default port we just need to show the IP address,
    // but otherwise we need to include the port. This is important because
    // we use the string representation in the setup tool to provided an
    // address to connect to.
    if (port != DEFAULT_PORT)
    {
        char portbuf[10];
        M_snprintf(portbuf, sizeof(portbuf), ":%i", port);
        M_StringConcat(buffer, portbuf, buffer_len);
    }
}

static net_addr_t *NETLIB_ResolveAddress(const char *address)
{
    ip_address_t ip;
    char *addr_hostname;
    int addr_port;
    int result;
    char *colon;

    colon = strchr(address, ':');

    addr_hostname = M_StringDuplicate(address);
    if (colon != NULL)
    {
        addr_hostname[colon - address] = '\0';
        addr_port = atoi(colon + 1);
    }
    else
    {
        addr_port = port;
    }

    result = netlib_resolve_host(&ip, addr_hostname, addr_port);

    free(addr_hostname);

    if (result)
    {
        // unable to resolve

        return NULL;
    }
    else
    {
        return FindAddress(&ip);
    }
}

static void NETLIB_Shutdown(void)
{
    if (!initted)
    {
        return;
    }

    netlib_quit();
}

// Complete module

net_module_t netlib_module =
{
    NETLIB_InitClient,
    NETLIB_InitServer,
    NETLIB_SendPacket,
    NETLIB_RecvPacket,
    NETLIB_AddrToString,
    NETLIB_FreeAddress,
    NETLIB_ResolveAddress,
    NETLIB_Shutdown,
};
