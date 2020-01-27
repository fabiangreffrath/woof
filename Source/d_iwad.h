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

// "128 IWAD search directories should be enough for anybody".

#define MAX_IWAD_DIRS 128

extern char *iwad_dirs[MAX_IWAD_DIRS];
extern int num_iwad_dirs;

void BuildIWADDirList(void);
char *D_FindWADByName(const char *filename);
char *D_TryFindWADByName(const char *filename);

#endif

