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
//      Network packet manipulation (nw_packet_t)
//

#ifndef NW_IO_H
#define NW_IO_H

#include "doomtype.h"
#include "nw_defs.h"

extern nw_addr_t nw_broadcast_addr;

// Create a new network context.
nw_context_t *NW_NewContext(void);

// Add a network module to a context.
void NW_AddModule(nw_context_t *context, nw_module_t *module);

// Send a packet to the given address.
void NW_SendPacket(nw_addr_t *addr, nw_packet_t *packet);

// Send a broadcast using all modules in the given context.
void NW_SendBroadcast(nw_context_t *context, nw_packet_t *packet);

// Check all modules in the given context and receive a packet, returning true
// if a packet was received. The result is stored in *packet and the source is
// stored in *addr, with an implicit reference added. The packet must be freed
// by the caller and the reference releasd.
boolean NW_RecvPacket(nw_context_t *context, nw_addr_t **addr,
                       nw_packet_t **packet);

// Return a string representation of the given address. The result points to a
// static buffer and will become invalid with the next call.
char *NW_AddrToString(nw_addr_t *addr);

// Add a reference to the given address.
void NW_ReferenceAddress(nw_addr_t *addr);

// Release a reference to the given address. When there are no more references,
// the address will be freed.
void NW_ReleaseAddress(nw_addr_t *addr);

// Resolve a string representation of an address. If successful, a nw_addr_t
// pointer is received with an implicit reference that must be freed by the
// caller when it is no longer needed.
nw_addr_t *NW_ResolveAddress(nw_context_t *context, const char *address);

#endif /* #ifndef NW_IO_H */
