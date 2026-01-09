//
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2014 Fabian Greffrath
// Copyright(C) 2021 Roman Fomin
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
// Parses [SOUNDS] sections in BEX files
//

#ifndef DEH_BEX_SOUNDS_H
#define DEH_BEX_SOUNDS_H

#include <stddef.h>

#include "sounds.h"

// DSDHacked
extern sfxinfo_t* S_sfx;
extern int num_sfx;

extern void DEH_InitSFX(void);
extern void DEH_InitMusic(void);

extern void DEH_SoundsEnsureCapacity(int limit);
extern int DEH_SoundsGetIndex(const char *key, size_t length);
extern int DEH_SoundsGetOriginalIndex(const char *key);
extern int DEH_SoundsGetNewIndex(void);

#endif
