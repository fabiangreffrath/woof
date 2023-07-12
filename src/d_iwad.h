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
//     Find IWAD and initialize according to IWAD type.
//

#ifndef __D_IWAD__
#define __D_IWAD__

#include "doomtype.h"
#include "doomdef.h"

typedef struct
{
    const char *name;
    GameMission_t mission;
    GameMode_t mode;
    const char *description;
} iwad_t;

char *D_DoomExeDir(void);       // killough 2/16/98: path to executable's dir
char *D_FindWADByName(const char *filename);
char *D_TryFindWADByName(const char *filename);
char *D_FindIWADFile(GameMode_t *mode, GameMission_t *mission);
boolean D_IsIWADName(const char *name);
const iwad_t **D_GetIwads(void);

#endif

