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
//  Starting a network game requested from the multiplayer menu.
//

#ifndef MP_START_H
#define MP_START_H

#include "doomtype.h"
#include "mn_common.h"
#include "nw_defs.h"

typedef enum
{
    MP_REQUEST_NONE,
    MP_REQUEST_CREATE,
    MP_REQUEST_JOIN,
    MP_REQUEST_SINGLE,
    MP_REQUEST_LEAVE
} mp_request_type_t;

typedef struct
{
    mp_request_type_t type;
    char address[MAX_ADDRESS];
    int skill;
    int episode;
    int map;
} mp_request_t;

// Queue a request; it is handled by MP_StartTick(), outside of the menu code.

void MP_RequestCreate(void);
void MP_RequestJoin(const char *address);
void MP_RequestSingleGame(int skill, int episode, int map);
void MP_RequestLeave(void);

boolean MP_RequestPending(void);
void MP_RequestClear(void);

// Handles a pending request and the waiting-for-launch state. Called once
// per frame at the top of the main loop.
void MP_StartTick(void);

// Draws the waiting screen on top of everything else, if it is up.
void MP_DrawWaitScreen(void);

boolean MP_WaitScreenActive(void);

// Called by the display code after the screen wipe has finished; a network
// game started from the menu resyncs its tic clock here.
void MP_AfterStartWipe(void);

// Menu keys while the waiting screen is up.
boolean MP_Responder(menu_action_t action);

#endif
