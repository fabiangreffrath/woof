//
//  Copyright (C) 2022 Fabian Greffrath
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
//      Savegame snapshots
//

#ifndef __MN_SNAPSHOT__
#define __MN_SNAPSHOT__

#include <stdio.h>

#include "doomtype.h"

const int MN_SnapshotDataSize(void);
void MN_ResetSnapshot(int i);
boolean MN_ReadSnapshot(int i, FILE *fp);
void MN_WriteSnapshot(byte *p);
boolean MN_DrawSnapshot(int i, int x, int y, int w, int h);

void MN_ReadSavegameTime(int i, char *name);
char *MN_GetSavegameTime(int i);

#endif
