//
//  Copyright (C) 2026 Roman Fomin
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
// DESCRIPTION:
//  In-game multiplayer menu: create and join network games.
//

#ifndef MN_MULTIPLAYER_H
#define MN_MULTIPLAYER_H

#include "doomtype.h"
#include "mn_container.h"

extern mn_menu_t multiplayer_menu;

// Server info line on the Join Game page.
extern char mp_status_text[80];

void MN_BindMultiplayerVariables(void);
void MN_MultiplayerInit(void);

// Menu actions, referenced from the declarative item tables.
void MP_CreateGame(void);
void MP_JoinGame(void);
void MP_FindLANServers(void);
void MP_FindInternetServers(void);

typedef enum
{
    MP_REQUEST_NONE,
    MP_REQUEST_CREATE,
    MP_REQUEST_JOIN
} mp_request_type_t;

// A game start requested from the menu. Consumed by the main loop, so that
// the network game is set up outside of the menu responder.
typedef struct
{
    mp_request_type_t type;
    int deathmatch;
    int skill;
    int episode;
    int map;
    int timelimit;
    boolean nomonsters;
    boolean fast;
    boolean respawn;
    int port;
    boolean privateserver;
    char address[64];
} mp_request_t;

boolean MP_RequestPending(void);
const mp_request_t *MP_RequestGet(void);
void MP_RequestClear(void);

#endif
