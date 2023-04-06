//
// Copyright(C) 2023 Roman Fomin
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
//

#ifdef HAVE_MODPLUG

#include <libmodplug/modplug.h>

#include "doomtype.h"
#include "i_sound.h"
#include "i_oalmusic.h"

typedef struct
{
    ModPlugFile *file;

    void *data;
    int size;
} modplug_t;

static modplug_t modplug;

static ModPlug_Settings settings;

static uint32_t MOD_Callback(byte *buffer, uint32_t buffer_samples)
{
    int filled;

    filled = ModPlug_Read(modplug.file, buffer, buffer_samples * 4);

    return filled / 4;
}

static boolean I_MOD_InitMusic(int device)
{
    ModPlug_GetSettings(&settings);
    settings.mFlags = MODPLUG_ENABLE_OVERSAMPLING;
    settings.mChannels = 2;
    settings.mBits = 16;
    settings.mFrequency = SND_SAMPLERATE;
    settings.mResamplingMode = MODPLUG_RESAMPLE_FIR;
    settings.mReverbDepth = 0;
    settings.mReverbDelay = 100;
    settings.mBassAmount = 0;
    settings.mBassRange = 50;
    settings.mSurroundDepth = 0;
    settings.mSurroundDelay = 10;
    settings.mLoopCount = -1;
    ModPlug_SetSettings(&settings);

    return true;
}

static int current_volume;

static void I_MOD_SetMusicVolume(int volume)
{
    current_volume = volume;
    ModPlug_SetMasterVolume(modplug.file, volume * 256 / 15);
}

static void I_MOD_PauseSong(void *handle)
{
    ModPlug_SetMasterVolume(modplug.file, 0);
}

static void I_MOD_ResumeSong(void *handle)
{
    I_MOD_SetMusicVolume(current_volume);
}

static void *I_MOD_RegisterSong(void *data, int len)
{
    ModPlugFile *file;

    modplug.data = data;
    modplug.size = len;

    file = ModPlug_Load(data, len);
    if (file)
    {
        ModPlug_Unload(file);
        return (void *)1;
    }

    return NULL;
}

static void I_MOD_PlaySong(void *handle, boolean looping)
{
    settings.mLoopCount = looping ? -1 : 0;
    ModPlug_SetSettings(&settings);
    modplug.file = ModPlug_Load(modplug.data, modplug.size);
    I_OAL_HookMusic(MOD_Callback);
}

static void I_MOD_StopSong(void *handle)
{
    I_OAL_HookMusic(NULL);
}

static void I_MOD_UnRegisterSong(void *handle)
{
    if (modplug.file)
    {
        ModPlug_Unload(modplug.file);
        modplug.file = NULL;
    }
}

static void I_MOD_ShutdownMusic(void)
{
    I_MOD_StopSong(NULL);
    I_MOD_UnRegisterSong(NULL);
}

static int I_MOD_DeviceList(const char *devices[], int size, int *current_device)
{
    *current_device = 0;
    return 0;
}

music_module_t music_mod_module =
{
    I_MOD_InitMusic,
    I_MOD_ShutdownMusic,
    I_MOD_SetMusicVolume,
    I_MOD_PauseSong,
    I_MOD_ResumeSong,
    I_MOD_RegisterSong,
    I_MOD_PlaySong,
    I_MOD_StopSong,
    I_MOD_UnRegisterSong,
    I_MOD_DeviceList,
};

#endif // HAVE_MODPLUG
