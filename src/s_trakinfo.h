//
// Copyright(C) 2024 Roman Fomin
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

#ifndef S_TRAKINFO_H
#define S_TRAKINFO_H

#include "doomtype.h"

extern boolean trakinfo_found;

void S_ParseTrakInfo(int lumpnum);

typedef enum
{
    EXMUS_OFF,
    EXMUS_REMIX,
    EXMUS_ORIGINAL
} extra_music_t;

struct musicinfo_s;

void S_GetExtra(struct musicinfo_s *music, extra_music_t type);

#endif
