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

#ifndef NW_PACKET_H
#define NW_PACKET_H

#include "doomtype.h"
#include "nw_defs.h"

nw_packet_t *NW_NewPacket(int initial_size);
nw_packet_t *NW_PacketDup(nw_packet_t *packet);
void NW_FreePacket(nw_packet_t *packet);

boolean NW_ReadInt8(nw_packet_t *packet, unsigned int *data);
boolean NW_ReadInt16(nw_packet_t *packet, unsigned int *data);
boolean NW_ReadInt32(nw_packet_t *packet, unsigned int *data);

boolean NW_ReadSInt8(nw_packet_t *packet, signed int *data);
boolean NW_ReadSInt16(nw_packet_t *packet, signed int *data);
boolean NW_ReadSInt32(nw_packet_t *packet, signed int *data);

char *NW_ReadString(nw_packet_t *packet);
char *NW_ReadSafeString(nw_packet_t *packet);

void NW_WriteInt8(nw_packet_t *packet, unsigned int i);
void NW_WriteInt16(nw_packet_t *packet, unsigned int i);
void NW_WriteInt32(nw_packet_t *packet, unsigned int i);

void NW_WriteString(nw_packet_t *packet, const char *string);

#endif /* #ifndef NW_PACKET_H */
