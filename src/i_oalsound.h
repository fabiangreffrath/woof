//
// Copyright(C) 2023 Roman Fomin
// Copyright(C) 2023 ceski
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
//      System interface for OpenAL sound.
//

#ifndef __I_OALSOUND__
#define __I_OALSOUND__

#include "doomtype.h"
#include "i_sound.h"

boolean I_OAL_InitSound(void);
boolean I_OAL_CacheSound(ALuint *buffer, ALenum format, const byte *data,
                         ALsizei size, ALsizei freq);
void I_OAL_UpdateSoundParams2D(int channel, int volume, int separation);
boolean I_OAL_StartSound(int channel, ALuint buffer, int pitch);
void I_OAL_StopSound(int channel);
boolean I_OAL_SoundIsPlaying(int channel);
void I_OAL_ShutdownSound(void);

#endif
