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

#include "doomtype.h"
#include "i_system.h"
#include "m_argv.h"
#include "m_misc.h"
#include "net_defs.h"
#include "net_io.h"
#include "net_packet.h"
#include "net_sdl.h"
#include "z_zone.h"

//
// NETWORKING
//

#include <SDL3_net/SDL_net.h>

#define DEFAULT_PORT 2342

static boolean initted = false;
static int port = DEFAULT_PORT;
static SDLNet_DatagramSocket *udpsocket;
//static UDPpacket *recvpacket;

typedef struct
{
    net_addr_t net_addr;
    SDLNet_Address *sdl_addr;
} addrpair_t;

static addrpair_t **addr_table;
static int addr_table_size = -1;

// Initializes the address table

static void NET_SDL_InitAddrTable(void)
{
    addr_table_size = 16;

    addr_table = Z_Malloc(sizeof(addrpair_t *) * addr_table_size, PU_STATIC, 0);
    memset(addr_table, 0, sizeof(addrpair_t *) * addr_table_size);
}

// Finds an address by searching the table.  If the address is not found,
// it is added to the table.

static net_addr_t *NET_SDL_FindAddress(SDLNet_Address *addr)
{
    addrpair_t *new_entry;
    int empty_entry = -1;
    int i;

    if (addr_table_size < 0)
    {
        NET_SDL_InitAddrTable();
    }

    for (i = 0; i < addr_table_size; ++i)
    {
        if (addr_table[i] != NULL
            && SDLNet_CompareAddresses(addr, addr_table[i]->sdl_addr))
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

    new_entry->sdl_addr = addr;
    new_entry->net_addr.refcount = 0;
    new_entry->net_addr.handle = &new_entry->sdl_addr;
    new_entry->net_addr.module = &net_sdl_module;

    addr_table[empty_entry] = new_entry;

    return &new_entry->net_addr;
}

static void NET_SDL_FreeAddress(net_addr_t *addr)
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

    I_Error("NET_SDL_FreeAddress: Attempted to remove an unused address!");
}

static boolean NET_SDL_InitClient(void)
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

    if (!SDLNet_Init())
    {
        I_Error("NET_SDL_InitClient: Failed to initialize SDLNet: %s",
                SDL_GetError());
    }

    udpsocket = SDLNet_CreateDatagramSocket(NULL, port);

    if (udpsocket == NULL)
    {
        I_Error("NET_SDL_InitClient: Unable to open a socket!");
    }

    //recvpacket = SDLNet_AllocPacket(1500);

#ifdef DROP_PACKETS
    srand(time(NULL));
#endif

    initted = true;

    return true;
}

static boolean NET_SDL_InitServer(void)
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

    if (!SDLNet_Init())
    {
        I_Error("NET_SDL_InitServer: Failed to initialize SDLNet: %s",
                SDL_GetError());
    }

    udpsocket = SDLNet_CreateDatagramSocket(NULL, port);

    if (udpsocket == NULL)
    {
        I_Error("NET_SDL_InitServer: Unable to bind to port %i", port);
    }

    //recvpacket = SDLNet_AllocPacket(1500);
#ifdef DROP_PACKETS
    srand(time(NULL));
#endif

    initted = true;

    return true;
}

static void NET_SDL_SendPacket(net_addr_t *addr, net_packet_t *packet)
{
    // UDPpacket sdl_packet;
    // IPaddress ip;

    SDLNet_Address *sdladdr;

    if (addr == &net_broadcast_addr)
    {
        //SDLNet_ResolveHostname();
        //ip.host = INADDR_BROADCAST;
        sdladdr = NULL;
    }
    else
    {
        sdladdr = (SDLNet_Address *)addr->handle;
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

    if (!SDLNet_SendDatagram(udpsocket, sdladdr, 0, packet->data, packet->len))
    {
        I_Error("NET_SDL_SendPacket: Error transmitting packet: %s",
                SDL_GetError());
    }
}

static boolean NET_SDL_RecvPacket(net_addr_t **addr, net_packet_t **packet)
{
    SDLNet_Datagram *dgram = NULL;

    bool result = SDLNet_ReceiveDatagram(udpsocket, &dgram);

    if (!result)
    {
        I_Error("NET_SDL_RecvPacket: Error receiving packet: %s",
                SDL_GetError());
    }

    // no packets received

    if (dgram == NULL)
    {
        return false;
    }

    // Put the data into a new packet structure

    memcpy((*packet)->data, dgram->buf, dgram->buflen);
    (*packet)->len = dgram->buflen;

    // Address

    *addr = NET_SDL_FindAddress(dgram->addr);

    SDLNet_DestroyDatagram(dgram);

    return true;
}

void NET_SDL_AddrToString(net_addr_t *addr, char *buffer, int buffer_len)
{
    // IPaddress *ip;
    // uint32_t host;
    // uint16_t port;

    // ip = (IPaddress *)addr->handle;
    // host = SDLNet_Read32(&ip->host);
    // port = SDLNet_Read16(&ip->port);

    // M_snprintf(buffer, buffer_len, "%i.%i.%i.%i", (host >> 24) & 0xff,
    //            (host >> 16) & 0xff, (host >> 8) & 0xff, host & 0xff);

    M_snprintf(buffer, buffer_len, "%s",
               SDLNet_GetAddressString((SDLNet_Address *)addr->handle));

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

net_addr_t *NET_SDL_ResolveAddress(const char *address)
{
    //IPaddress ip;
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

    SDLNet_Address *sdladdr = SDLNet_ResolveHostname(addr_hostname);

    free(addr_hostname);

    result = SDLNet_WaitUntilResolved(sdladdr, 10000);

    if (result == -1)
    {
        // unable to resolve

        return NULL;
    }
    else
    {
        return NET_SDL_FindAddress(sdladdr);
    }
}

void NET_SDL_Shutdown(void)
{
    if (!initted)
    {
        return;
    }

    SDLNet_Quit();
}

// Complete module

net_module_t net_sdl_module =
{
    NET_SDL_InitClient,
    NET_SDL_InitServer,
    NET_SDL_SendPacket,
    NET_SDL_RecvPacket,
    NET_SDL_AddrToString,
    NET_SDL_FreeAddress,
    NET_SDL_ResolveAddress,
    NET_SDL_Shutdown,
};
