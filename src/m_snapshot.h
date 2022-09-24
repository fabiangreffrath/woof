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
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 
//  02111-1307, USA.
//
// DESCRIPTION:
//      Savegame snapshots
//

#ifndef __M_SNAPSHOT__
#define __M_SNAPSHOT__

const int M_SnapshotDataSize (void);
void M_ResetSnapshot (int i);
boolean M_ReadSnapshot (int i, FILE *fp);
void M_WriteSnapshot (byte *p);
boolean M_DrawSnapshot (int i, int x, int y, int w, int h);

void M_ReadSavegameTime (int i, char *name);
char *M_GetSavegameTime (int i);

#endif
