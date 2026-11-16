//
// Copyright(C) 1993-1996 Id Software, Inc.
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
//  DOOM Network game communication and protocol,
//  all OS independend parts.
//

#ifndef D_NET_H
#define D_NET_H

#include "d_ticcmd.h"
#include "doomtype.h"

void D_RunTic(ticcmd_t *cmds, boolean *ingame);

void D_ConnectNetGame(void);

boolean D_InitNetGameFromMenu(boolean server, const char *address);

void D_CheckNetGame(void);

#endif
