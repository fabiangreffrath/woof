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
//     Querying servers to find their current status.
//

#ifndef NW_QUERY_H
#define NW_QUERY_H

#include "doomtype.h"
#include "nw_defs.h"

typedef void (*nw_query_callback_t)(nw_addr_t *addr,
                                     nw_querydata_t *querydata,
                                     unsigned int ping_time, void *user_data);

extern int NW_StartLANQuery(void);
extern int NW_StartMasterQuery(void);

extern void NW_LANQuery(void);
extern void NW_MasterQuery(void);
extern void NW_QueryAddress(const char *addr);
extern nw_addr_t *NW_FindLANServer(void);

extern int NW_Query_Poll(nw_query_callback_t callback, void *user_data);

extern nw_addr_t *NW_Query_ResolveMaster(nw_context_t *context);
extern void NW_Query_AddToMaster(nw_addr_t *master_addr);
extern boolean NW_Query_CheckAddedToMaster(boolean *result);
extern void NW_Query_AddResponse(nw_packet_t *packet);
extern void NW_RequestHolePunch(nw_context_t *context, nw_addr_t *addr);

#endif /* #ifndef NW_QUERY_H */
