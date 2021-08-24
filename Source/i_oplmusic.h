//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2021 Fabian Greffrath
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
//   System interface for music.
//

#ifndef __I_OPLMUSIC__
#define __I_OPLMUSIC__

#include "doomtype.h"

boolean I_OPL_InitMusic(void);
void I_OPL_ShutdownMusic(void);
boolean I_OPL_MusicIsPlaying(void);
void *I_OPL_RegisterSong(void *data, int len);
void I_OPL_StopSong(void *handle);
void I_OPL_UnRegisterSong(void *handle);
void I_OPL_ResumeSong(void *handle);
void I_OPL_PauseSong(void *handle);
void I_OPL_PlaySong(void *handle, boolean looping);
void I_OPL_SetMusicVolume(int volume);

#endif
