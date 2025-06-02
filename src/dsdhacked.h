//
//  Copyright(C) 2021 Roman Fomin
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
//      DSDHacked support

#ifndef __DSDHACKED__
#define __DSDHACKED__

#include "doomtype.h"
#include "info.h"

void dsdh_InitTables(void);
void dsdh_FreeTables(void);

void dsdh_EnsureStatesCapacity(int limit);
void dsdh_EnsureSFXCapacity(int limit);
void dsdh_EnsureMobjInfoCapacity(int limit);
int dsdh_GetDehSpriteIndex(const char *key);
int dsdh_GetOriginalSpriteIndex(const char *key);
int dsdh_GetDehSFXIndex(const char *key, size_t length);
int dsdh_GetDehMusicIndex(const char *key, int length);
int dsdh_GetOriginalSFXIndex(const char *key);
int dsdh_GetNewSFXIndex(void);
int dsdh_GetNewMobjInfoIndex(void);

extern byte *defined_codeptr_args;
extern union actionf_u *deh_codeptr;
extern statenum_t *seenstate_tab;

#endif
