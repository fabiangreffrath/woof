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
//     Network packet I/O.  Base layer for sending/receiving packets,
//     through the network module system
//

#include <stdio.h>

#include "i_system.h"
#include "nw_defs.h"
#include "nw_io.h"
#include "z_zone.h"

#define MAX_MODULES 16

struct _nw_context_s
{
    nw_module_t *modules[MAX_MODULES];
    int num_modules;
};

nw_addr_t nw_broadcast_addr;

nw_context_t *NW_NewContext(void)
{
    nw_context_t *context;

    context = Z_Malloc(sizeof(nw_context_t), PU_STATIC, 0);
    context->num_modules = 0;

    return context;
}

void NW_AddModule(nw_context_t *context, nw_module_t *module)
{
    if (context->num_modules >= MAX_MODULES)
    {
        I_Error("No more modules for context");
    }

    context->modules[context->num_modules] = module;
    ++context->num_modules;
}

nw_addr_t *NW_ResolveAddress(nw_context_t *context, const char *addr)
{
    int i;
    nw_addr_t *result;

    for (i = 0; i < context->num_modules; ++i)
    {
        result = context->modules[i]->ResolveAddress(addr);

        if (result != NULL)
        {
            NW_ReferenceAddress(result);
            return result;
        }
    }

    return NULL;
}

void NW_SendPacket(nw_addr_t *addr, nw_packet_t *packet)
{
    addr->module->SendPacket(addr, packet);
}

void NW_SendBroadcast(nw_context_t *context, nw_packet_t *packet)
{
    int i;

    for (i = 0; i < context->num_modules; ++i)
    {
        context->modules[i]->SendPacket(&nw_broadcast_addr, packet);
    }
}

boolean NW_RecvPacket(nw_context_t *context, nw_addr_t **addr,
                       nw_packet_t **packet)
{
    int i;

    // check all modules for new packets

    for (i = 0; i < context->num_modules; ++i)
    {
        if (context->modules[i]->RecvPacket(addr, packet))
        {
            NW_ReferenceAddress(*addr);
            return true;
        }
    }

    return false;
}

// Note: this prints into a static buffer, calling again overwrites
// the first result

char *NW_AddrToString(nw_addr_t *addr)
{
    static char buf[128];

    addr->module->AddrToString(addr, buf, sizeof(buf) - 1);

    return buf;
}

void NW_ReferenceAddress(nw_addr_t *addr)
{
    if (addr == NULL)
    {
        return;
    }
    ++addr->refcount;
    // printf("%s: +refcount=%d\n", NW_AddrToString(addr), addr->refcount);
}

void NW_ReleaseAddress(nw_addr_t *addr)
{
    if (addr == NULL)
    {
        return;
    }

    --addr->refcount;
    // printf("%s: -refcount=%d\n", NW_AddrToString(addr), addr->refcount);
    if (addr->refcount <= 0)
    {
        addr->module->FreeAddress(addr);
    }
}
