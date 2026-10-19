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

#ifndef NW_STRUCTRW_H
#define NW_STRUCTRW_H

#include "d_ticcmd.h"
#include "doomtype.h"
#include "nw_defs.h"

void NW_WriteConnectData(nw_packet_t *packet, nw_connect_data_t *data);
boolean NW_ReadConnectData(nw_packet_t *packet, nw_connect_data_t *data);

extern void NW_WriteSettings(nw_packet_t *packet,
                              nw_gamesettings_t *settings);
extern boolean NW_ReadSettings(nw_packet_t *packet,
                                nw_gamesettings_t *settings);

extern void NW_WriteQueryData(nw_packet_t *packet,
                               nw_querydata_t *querydata);
extern boolean NW_ReadQueryData(nw_packet_t *packet,
                                 nw_querydata_t *querydata);

extern void NW_WriteTiccmdDiff(nw_packet_t *packet, nw_ticdiff_t *diff,
                                boolean lowres_turn);
extern boolean NW_ReadTiccmdDiff(nw_packet_t *packet, nw_ticdiff_t *diff,
                                  boolean lowres_turn);
extern void NW_TiccmdDiff(ticcmd_t *tic1, ticcmd_t *tic2, nw_ticdiff_t *diff);
extern void NW_TiccmdPatch(ticcmd_t *src, nw_ticdiff_t *diff, ticcmd_t *dest);

boolean NW_ReadFullTiccmd(nw_packet_t *packet, nw_full_ticcmd_t *cmd,
                           boolean lowres_turn);
void NW_WriteFullTiccmd(nw_packet_t *packet, nw_full_ticcmd_t *cmd,
                         boolean lowres_turn);

boolean NW_ReadSHA1Sum(nw_packet_t *packet, sha1_digest_t digest);
void NW_WriteSHA1Sum(nw_packet_t *packet, sha1_digest_t digest);

void NW_WriteWaitData(nw_packet_t *packet, nw_waitdata_t *data);
boolean NW_ReadWaitData(nw_packet_t *packet, nw_waitdata_t *data);

// Protocol list exchange.
nw_protocol_t NW_ReadProtocol(nw_packet_t *packet);
void NW_WriteProtocol(nw_packet_t *packet, nw_protocol_t protocol);
nw_protocol_t NW_ReadProtocolList(nw_packet_t *packet);
void NW_WriteProtocolList(nw_packet_t *packet);

#endif /* #ifndef NW_STRUCTRW_H */
