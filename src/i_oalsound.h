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

#include "alext.h"

#include "doomtype.h"
#include "i_sound.h"

#define OAL_CULL_DISTANCE 1200 // Distance where sound is culled.
#define OAL_MAX_DISTANCE 1000 // Distance where volume no longer attenuates.
#define OAL_REF_DISTANCE 200 // Distance where volume is no longer clamped.

void I_OAL_DeferUpdates(void);

void I_OAL_ProcessUpdates(void);

void I_OAL_ShutdownSound(void);

void I_OAL_ResetSource2D(int channel);

void I_OAL_ResetSource3D(int channel, boolean point_source);

void I_OAL_AdjustSource3D(int channel, const ALfloat *position,
                          const ALfloat *velocity, const ALfloat *direction);

void I_OAL_AdjustListener3D(const ALfloat *position, const ALfloat *velocity,
                            const ALfloat *orientation);

void I_OAL_UpdateUserSoundSettings(void);

boolean I_OAL_InitSound(boolean use_3d);

boolean I_OAL_ReinitSound(boolean use_3d);

boolean I_OAL_AllowReinitSound(void);

boolean I_OAL_CacheSound(ALuint *buffer, ALenum format, const byte *data,
                         ALsizei size, ALsizei freq);

boolean I_OAL_StartSound(int channel, ALuint buffer, int pitch);

void I_OAL_StopSound(int channel);

boolean I_OAL_SoundIsPlaying(int channel);

void I_OAL_SetVolume(int channel, int volume);

void I_OAL_SetPan(int channel, int separation);

#endif