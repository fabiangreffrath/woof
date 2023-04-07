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

#include "i_oalmusic.h"

static ModPlugFile *file;

static ModPlug_Settings settings;

static boolean I_MOD_OpenStream(void *data, ALsizei size, ALenum *format,
                                ALsizei *freq, ALsizei *frame_size)
{
    ModPlug_GetSettings(&settings);
    settings.mFlags = MODPLUG_ENABLE_OVERSAMPLING;
    settings.mChannels = 2;
    settings.mBits = 16;
    settings.mFrequency = 44100;
    settings.mResamplingMode = MODPLUG_RESAMPLE_FIR;
    settings.mReverbDepth = 0;
    settings.mReverbDelay = 100;
    settings.mBassAmount = 0;
    settings.mBassRange = 50;
    settings.mSurroundDepth = 0;
    settings.mSurroundDelay = 10;
    settings.mLoopCount = -1;
    ModPlug_SetSettings(&settings);

    file = ModPlug_Load(data, size);

    if (file)
    {
        ModPlug_SetMasterVolume(file, 256);

        *format = AL_FORMAT_STEREO16;
        *freq = 44100;
        *frame_size = 2 * sizeof(short);

        return true;
    }

    return false;
}

static uint32_t I_MOD_FillStream(byte *buffer, uint32_t buffer_samples)
{
    int filled;

    filled = ModPlug_Read(file, buffer, buffer_samples * 4);

    return filled / 4;
}

static void I_MOD_RestartStream(void)
{
    ModPlug_Seek(file, 0);
}

static void I_MOD_CloseStream(void)
{
    if (file)
    {
        ModPlug_Unload(file);
        file = NULL;
    }
}

stream_module_t stream_mod_module =
{
    I_MOD_OpenStream,
    I_MOD_FillStream,
    I_MOD_RestartStream,
    I_MOD_CloseStream,
};

#endif // HAVE_MODPLUG
