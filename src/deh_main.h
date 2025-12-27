//
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2025 Guilherme Miranda
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
//
// Dehacked entrypoint and common code
//

#ifndef DEH_MAIN_H
#define DEH_MAIN_H

#include "doomtype.h"
#include "info.h"
#include "sha1.h"

void DEH_Init(void);        // [crispy] un-static
char *CleanString(char *s); // [crispy] un-static

void DEH_ParseCommandLine(void);
int DEH_LoadFile(const char *filename);
void DEH_AutoLoadPatches(const char *path);
void DEH_LoadLump(int lumpnum);
void DEH_LoadLumpByName(const char *name);

boolean DEH_ParseAssignment(char *line, char **variable_name, char **value);

void DEH_Checksum(sha1_digest_t digest);

char **DEH_GetFileNames(void);

extern boolean deh_apply_cheats;

typedef struct bex_bitflags_s bex_bitflags_t;

struct bex_bitflags_s
{
    const char *flag;
    int bits;
};

// Extensions
extern void DEH_InitTables(void);
extern void DEH_FreeTables(void);
extern int DEH_ParseBexBitFlags(int ivalue, char *value, const bex_bitflags_t flags[], int len);
extern void DEH_PostProcess(void);
extern boolean DEH_CheckSafeState(statenum_t state);

#endif /* #ifndef DEH_MAIN_H */
