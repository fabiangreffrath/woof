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
//     Networking module which uses SDL_net
//

#include <stdlib.h>
#include <string.h>

#include <SDL3/SDL.h>
#include <SDL3_net/SDL_net.h>

#include "doomtype.h"
#include "i_system.h"
#include "m_argv.h"
#include "m_misc.h"
#include "nw_defs.h"
#include "nw_io.h"
#include "nw_packet.h"
#include "nw_sdl.h"
#include "z_zone.h"

//
// NETWORKING
//

#define DEFAULT_PORT 2342

static boolean initted = false;
static int port = DEFAULT_PORT;
static NET_DatagramSocket *udpsocket;

typedef struct
{
    nw_addr_t nw_addr;
    NET_Address *address;
    Uint16 port;
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

// Finds an address by searching the table.  If the address is not found,
// it is added to the table.  A reference is taken on the address, which
// is released again when the entry is freed.

static nw_addr_t *FindAddress(NET_Address *address, Uint16 address_port)
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
            && NET_CompareAddresses(address, addr_table[i]->address) == 0
            && address_port == addr_table[i]->port)
        {
            return &addr_table[i]->nw_addr;
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

    new_entry->address = NET_RefAddress(address);
    new_entry->port = address_port;
    new_entry->nw_addr.refcount = 0;
    new_entry->nw_addr.handle = new_entry;
    new_entry->nw_addr.module = &nw_sdl_module;

    addr_table[empty_entry] = new_entry;

    return &new_entry->nw_addr;
}

static void NW_SDL_FreeAddress(nw_addr_t *addr)
{
    int i;

    for (i = 0; i < addr_table_size; ++i)
    {
        if (addr == &addr_table[i]->nw_addr)
        {
            NET_UnrefAddress(addr_table[i]->address);
            Z_Free(addr_table[i]);
            addr_table[i] = NULL;
            return;
        }
    }

    I_Error("Attempted to remove an unused address!");
}

static boolean NW_SDL_InitSocket(Uint16 bind_port)
{
    SDL_PropertiesID props;

    if (!NET_Init())
    {
        I_Error("Failed to initialize SDL_net: %s", SDL_GetError());
    }

    props = SDL_CreateProperties();
    SDL_SetBooleanProperty(props,
                           NET_PROP_DATAGRAM_SOCKET_ALLOW_BROADCAST_BOOLEAN,
                           true);
    udpsocket = NET_CreateDatagramSocket(NULL, bind_port, props);
    SDL_DestroyProperties(props);

    if (udpsocket == NULL)
    {
        return false;
    }

#ifdef DROP_PACKETS
    NET_SimulateDatagramPacketLoss(udpsocket, 25);
#endif

    initted = true;

    return true;
}

static boolean NW_SDL_InitClient(void)
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

    if (!NW_SDL_InitSocket(0))
    {
        I_Error("Unable to open a socket: %s", SDL_GetError());
    }

    return true;
}

static boolean NW_SDL_InitServer(void)
{
    int p;

    if (initted)
    {
        return true;
    }

    p = M_CheckParmWithArgs("-port", 1);
    if (p > 0)
    {
        port = M_ParmArgToInt(p);
    }

    if (!NW_SDL_InitSocket(port))
    {
        I_Error("Unable to bind to port %i: %s", port, SDL_GetError());
    }

    return true;
}

static void NW_SDL_SendPacket(nw_addr_t *addr, nw_packet_t *packet)
{
    addrpair_t *entry;
    NET_Address *address = NULL;
    Uint16 send_port = port;

    if (addr != &nw_broadcast_addr)
    {
        entry = (addrpair_t *)addr->handle;
        address = entry->address;
        send_port = entry->port;
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

    // Sending to a NULL address broadcasts the packet; the socket must
    // have been created with broadcast permission for this to work.

    if (!NET_SendDatagram(udpsocket, address, send_port, packet->data,
                          (int)packet->len))
    {
        I_Error("Error transmitting packet: %s", SDL_GetError());
    }
}

static boolean NW_SDL_RecvPacket(nw_addr_t **addr, nw_packet_t **packet)
{
    NET_Datagram *dgram;

    if (!NET_ReceiveDatagram(udpsocket, &dgram))
    {
        I_Error("Error receiving packet: %s", SDL_GetError());
    }

    // no packets received

    if (dgram == NULL)
    {
        return false;
    }

    // Put the data into a new packet structure

    *packet = NW_NewPacket(dgram->buflen);
    memcpy((*packet)->data, dgram->buf, dgram->buflen);
    (*packet)->len = dgram->buflen;

    // Address

    *addr = FindAddress(dgram->addr, dgram->port);

    NET_DestroyDatagram(dgram);

    return true;
}

static void NW_SDL_AddrToString(nw_addr_t *addr, char *buffer, int buffer_len)
{
    addrpair_t *entry;
    const char *address;

    entry = (addrpair_t *)addr->handle;
    address = NET_GetAddressString(entry->address);

    if (address == NULL)
    {
        M_snprintf(buffer, buffer_len, "(unresolved)");
        return;
    }

    M_StringCopy(buffer, address, buffer_len);

    // If we are using the default port we just need to show the IP address,
    // but otherwise we need to include the port. This is important because
    // we use the string representation in the setup tool to provided an
    // address to connect to.
    if (entry->port != DEFAULT_PORT)
    {
        char portbuf[10];
        M_snprintf(portbuf, sizeof(portbuf), ":%i", entry->port);
        M_StringConcat(buffer, portbuf, buffer_len);
    }
}

static nw_addr_t *NW_SDL_ResolveAddress(const char *address)
{
    NET_Address *nw_address;
    char *addr_hostname;
    int addr_port;
    nw_addr_t *result = NULL;
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

    nw_address = NET_ResolveHostname(addr_hostname);

    free(addr_hostname);

    if (nw_address != NULL)
    {
        if (NET_WaitUntilResolved(nw_address, -1) == NET_SUCCESS)
        {
            result = FindAddress(nw_address, (Uint16)addr_port);
        }

        NET_UnrefAddress(nw_address);
    }

    return result;
}

static void NW_SDL_Shutdown(void)
{
    if (!initted)
    {
        return;
    }

    NET_DestroyDatagramSocket(udpsocket);
    udpsocket = NULL;

    NET_Quit();

    initted = false;
}

// Complete module

nw_module_t nw_sdl_module =
{
    NW_SDL_InitClient,
    NW_SDL_InitServer,
    NW_SDL_SendPacket,
    NW_SDL_RecvPacket,
    NW_SDL_AddrToString,
    NW_SDL_FreeAddress,
    NW_SDL_ResolveAddress,
    NW_SDL_Shutdown,
};
