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

#include "al.h"

#include "doomtype.h"

struct sfxinfo_s;

extern boolean oal_use_doppler;

void I_OAL_DeferUpdates(void);

void I_OAL_ProcessUpdates(void);

void I_OAL_ShutdownSound(void);

void I_OAL_ShutdownModule(void);

void I_OAL_SetResampler(void);

void I_OAL_ResetSource2D(int channel);

void I_OAL_ResetSource3D(int channel, boolean point_source);

void I_OAL_UpdateSourceParams(int channel, const ALfloat *position,
                              const ALfloat *velocity);

void I_OAL_UpdateListenerParams(const ALfloat *position,
                                const ALfloat *velocity,
                                const ALfloat *orientation);

const char **I_OAL_GetResamplerStrings(void);

boolean I_OAL_InitSound(int snd_module);

boolean I_OAL_ReinitSound(int snd_module);

boolean I_OAL_AllowReinitSound(void);

boolean I_OAL_CacheSound(struct sfxinfo_s *sfx);

boolean I_OAL_StartSound(int channel, struct sfxinfo_s *sfx, float pitch);

void I_OAL_StopSound(int channel);

void I_OAL_PauseSound(int channel);

void I_OAL_ResumeSound(int channel);

boolean I_OAL_SoundIsPlaying(int channel);

boolean I_OAL_SoundIsPaused(int channel);

void I_OAL_SetVolume(int channel, int volume);

void I_OAL_SetPan(int channel, int separation);

void I_OAL_BindSoundVariables(void);

#endif
