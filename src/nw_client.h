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
// Network client code
//

#ifndef NW_CLIENT_H
#define NW_CLIENT_H

#include "doomtype.h"
#include "nw_defs.h"

struct ticcmd_s;

#define DEFAULT_PLAYER_NAME "Player"

boolean NW_CL_Connect(nw_addr_t *addr, nw_connect_data_t *data);
void NW_CL_Disconnect(void);
void NW_CL_Run(void);
void NW_CL_Init(void);
void NW_CL_LaunchGame(void);
void NW_CL_StartGame(nw_gamesettings_t *settings);
void NW_CL_SendTiccmd(struct ticcmd_s *ticcmd, int maketic);
boolean NW_CL_GetSettings(nw_gamesettings_t *_settings);
void NW_Init(void);

void NW_BindVariables(void);

extern boolean nw_client_connected;
extern boolean nw_client_received_wait_data;
extern nw_waitdata_t nw_client_wait_data;
extern char *nw_client_reject_reason;
extern boolean nw_waiting_for_launch;
extern const char *nw_player_name;

extern sha1_digest_t nw_server_wad_sha1sum;
extern sha1_digest_t nw_server_deh_sha1sum;
extern unsigned int nw_server_is_freedoom;
extern sha1_digest_t nw_local_wad_sha1sum;
extern sha1_digest_t nw_local_deh_sha1sum;
extern unsigned int nw_local_is_freedoom;

extern boolean drone;

#endif /* #ifndef NW_CLIENT_H */
