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

#ifndef UMAPINFO_H
#define UMAPINFO_H

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
    char exitanim[9];
    char enteranim[9];
    char interbackdrop[9];
    char intermusic[9];
    int partime;
    boolean nointermission;
    bossaction_t *bossactions;
    boolean nobossactions;
} mapentry_t;

extern mapentry_t *umapinfo, *umapdef;

extern boolean EpiCustom;

mapentry_t *G_LookupMapinfo(int episode, int map);

boolean U_CheckField(char *str);

void U_ParseMapDefInfo(int lumpnum);

void U_ParseMapInfo(int lumpnum);

boolean U_IsSecretMap(int episode, int map);

#endif
