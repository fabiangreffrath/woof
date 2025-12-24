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
// Parses [MUSIC] sections in BEX files
//

#ifndef DEH_BEX_MUSIC_H
#define DEH_BEX_MUSIC_H

#include <stddef.h>
#include "sounds.h"

// DSDHacked
extern musicinfo_t *S_music;
extern int num_music;

extern void DEH_FreeSFX(void);
extern void DEH_FreeMusic(void);

extern int DEH_MusicGetIndex(const char *key, int length);

#endif
