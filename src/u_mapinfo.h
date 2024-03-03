//-----------------------------------------------------------------------------
//
// Copyright 2017 Christoph Oelckers
// Copyright 2019 Fernando Carmona Varo
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//-----------------------------------------------------------------------------

#ifndef __UMAPINFO_H
#define __UMAPINFO_H

#include "doomtype.h"

typedef struct
{
    int type;
    int special;
    int tag;
} bossaction_t;

typedef struct mapentry_s
{
    char *mapname;
    char *levelname;
    char *label;
    char *intertext;
    char *intertextsecret;
    char *author;
    char levelpic[9];
    char nextmap[9];
    char nextsecret[9];
    char music[9];
    char skytexture[9];
    char endpic[9];
    char exitpic[9];
    char enterpic[9];
    char interbackdrop[9];
    char intermusic[9];
    int partime;
    boolean nointermission;
    int numbossactions;
    bossaction_t *bossactions;
} mapentry_t;

typedef struct
{
    unsigned int mapcount;
    mapentry_t *maps;
} umapinfo_t;

extern umapinfo_t U_mapinfo;
extern umapinfo_t default_mapinfo;

extern boolean EpiCustom;
mapentry_t *G_LookupMapinfo(int episode, int map);

boolean U_CheckField(char *str);

void U_ParseMapDefInfo(int lumpnum);

void U_ParseMapInfo(int lumpnum);

#endif
