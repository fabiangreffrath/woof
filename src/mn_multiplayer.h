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

#include "mn_container.h"

extern mn_menu_t multiplayer_menu;

void MN_BindMultiplayerVariables(void);
void MN_MultiplayerInit(void);

// Copy the Create Game settings into the game globals.
void MP_ApplyCreateSettings(void);

// Menu actions, referenced from the declarative item tables.
void MP_CreateGame(void);
void MP_FindLANServers(void);
void MP_FindInternetServers(void);

#endif
