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

#include <stdio.h>
#include <stdlib.h>

#include "alext.h"

#include "i_oalsound.h"

char *snd_resampler;

typedef struct oal_system_s
{
    ALCdevice *device;
    ALCint *attribs;
    ALCcontext *context;
    ALuint *sources;
    ALuint *buffers;
    int num_buffers;
    int num_buffers_mem;
    boolean initialized;
} oal_system_t;

static oal_system_t oal;

static boolean OpenDevice(void)
{
    const ALCchar *name;
    ALCint srate = -1;

    oal.device = alcOpenDevice(NULL);
    if (oal.device)
    {
        if (alcIsExtensionPresent(oal.device, "ALC_ENUMERATE_ALL_EXT") != AL_FALSE)
            name = alcGetString(oal.device, ALC_ALL_DEVICES_SPECIFIER);
        else
            name = alcGetString(oal.device, ALC_DEVICE_SPECIFIER);
    }
    else
    {
        printf("Could not open a device.\n");
        return false;
    }

    alcGetIntegerv(oal.device, ALC_FREQUENCY, 1, &srate);
    printf("Using '%s' @ %d Hz.\n", name, srate);

    return true;
}

static void CloseDevice(void)
{
    if (oal.device)
    {
        alcCloseDevice(oal.device);
        oal.device = NULL;
    }
}

static void FreeAttributes(void)
{
    if (oal.attribs)
    {
        free(oal.attribs);
        oal.attribs = NULL;
    }
}

static boolean CreateContext(void)
{
    oal.context = alcCreateContext(oal.device, oal.attribs);
    if (!oal.context || alcMakeContextCurrent(oal.context) == ALC_FALSE)
    {
        fprintf(stderr, "CreateContext: Error making context.\n");
        return false;
    }

    FreeAttributes();

    return true;
}

static void DestroyContext(void)
{
    alcMakeContextCurrent(NULL);
    if (oal.context)
    {
        alcDestroyContext(oal.context);
        oal.context = NULL;
    }
}

static boolean GenerateSources(void)
{
    oal.sources = malloc(sizeof(*oal.sources) * MAX_CHANNELS);
    if (oal.sources == NULL)
    {
        fprintf(stderr, "GenerateSources: Error allocating sources.\n");
        return false;
    }

    alGetError();
    alGenSources(MAX_CHANNELS, oal.sources);
    if (alGetError() != AL_NO_ERROR)
    {
        fprintf(stderr, "GenerateSources: Error making sources.\n");
        return false;
    }

    return true;
}

static void DeleteSources(void)
{
    if (oal.sources)
    {
        alDeleteSources(MAX_CHANNELS, oal.sources);
        oal.sources = NULL;
    }
}

static void SetSourceParams2D(void)
{
    int i;

    // Emulate 2D panning (https://github.com/kcat/openal-soft/issues/194).
    for (i = 0; i < MAX_CHANNELS; i++)
    {
        alSourcef(oal.sources[i], AL_ROLLOFF_FACTOR, 0.0f);
        alSourcei(oal.sources[i], AL_SOURCE_RELATIVE, AL_TRUE);
    }
}

// C doesn't allow casting between function and non-function pointer types, so
// with C99 we need to use a union to reinterpret the pointer type. Pre-C99
// still needs to use a normal cast and live with the warning (C++ is fine with
// a regular reinterpret_cast).
#if __STDC_VERSION__ >= 199901L
#define FUNCTION_CAST(T, ptr) (union{void *p; T f;}){ptr}.f
#else
#define FUNCTION_CAST(T, ptr) (T)(ptr)
#endif

static void SetResampler(void)
{
    LPALGETSTRINGISOFT alGetStringiSOFT = NULL;
    ALint i, num_resamplers, def_resampler;

    if (!alIsExtensionPresent("AL_SOFT_source_resampler"))
    {
        printf(" Resampler info not available!\n");
        return;
    }

    alGetStringiSOFT = FUNCTION_CAST(LPALGETSTRINGISOFT, alGetProcAddress("alGetStringiSOFT"));

    if (!alGetStringiSOFT)
    {
        fprintf(stderr, "SetResampler: alGetStringiSOFT() is not available.\n");
        return;
    }

    num_resamplers = alGetInteger(AL_NUM_RESAMPLERS_SOFT);
    def_resampler = alGetInteger(AL_DEFAULT_RESAMPLER_SOFT);

    if (!num_resamplers)
    {
        printf(" No resamplers found!\n");
        return;
    }

    for (i = 0; i < num_resamplers; i++)
    {
        if (!strcasecmp(snd_resampler, alGetStringiSOFT(AL_RESAMPLER_NAME_SOFT, i)))
        {
            def_resampler = i;
            break;
        }
    }
    if (i == num_resamplers)
    {
        printf(" Failed to find resampler: '%s'.\n", snd_resampler);
        return;
    }

    for (i = 0; i < MAX_CHANNELS; i++)
    {
        alSourcei(oal.sources[i], AL_SOURCE_RESAMPLER_SOFT, def_resampler);
    }

    printf(" Using '%s' resampler.\n",
           alGetStringiSOFT(AL_RESAMPLER_NAME_SOFT, def_resampler));
}

static void SetAttributes(ALCint hrtf_flag, ALCint output_mode)
{
    int i = 0;

    FreeAttributes();

    // Zero-terminated list of integer pairs.
    oal.attribs = calloc(5, sizeof(*oal.attribs));
    if (oal.attribs == NULL)
    {
        fprintf(stderr, "SetAttributes: Error allocating attributes.\n");
        return;
    }

    if (alcIsExtensionPresent(oal.device, "ALC_SOFT_HRTF") == ALC_TRUE)
    {
        oal.attribs[i++] = ALC_HRTF_SOFT;
        oal.attribs[i++] = hrtf_flag;
    }

    if (alcIsExtensionPresent(oal.device, "ALC_SOFT_output_mode") == ALC_TRUE)
    {
        oal.attribs[i++] = ALC_OUTPUT_MODE_SOFT;
        oal.attribs[i++] = output_mode;
    }
}

boolean I_OAL_InitSound(void)
{
    if (OpenDevice())
    {
        SetAttributes(ALC_FALSE, ALC_STEREO_BASIC_SOFT);
        if (CreateContext())
        {
            if (GenerateSources())
            {
                SetSourceParams2D();
                SetResampler();
                return true;
            }
            DeleteSources();
        }
        FreeAttributes();
        DestroyContext();
    }
    CloseDevice();

    return false;
}

static boolean UpdateBufferArray(ALuint buffer)
{
    if (oal.num_buffers == oal.num_buffers_mem)
    {
        ALuint *new_buffers;

        oal.num_buffers_mem += NUMSFX;

        new_buffers = realloc(oal.buffers,
                              sizeof(ALuint) * (oal.num_buffers_mem));
        if (new_buffers == NULL)
        {
            fprintf(stderr, "UpdateBufferArray: Error reallocating buffers.\n");
            return false;
        }

        oal.buffers = new_buffers;
    }

    oal.buffers[oal.num_buffers] = buffer;
    oal.num_buffers++;

    return true;
}

static boolean CreateBuffer(ALuint *buffer)
{
    alGetError();
    alGenBuffers(1, buffer);
    if (alGetError() != AL_NO_ERROR)
    {
        fprintf(stderr, "CreateBuffer: Error creating buffer.\n");
        return false;
    }
    return true;
}

static void DeleteBuffers(void)
{
    if (oal.buffers)
    {
        if (oal.num_buffers > 0)
        {
            alDeleteBuffers(oal.num_buffers, oal.buffers);
        }
        free(oal.buffers);
        oal.buffers = NULL;
    }
    oal.num_buffers = 0;
    oal.num_buffers_mem = 0;
}

static boolean BufferData(ALuint buffer, ALenum format, const byte *data,
                          ALsizei size, ALsizei freq)
{
    alGetError();
    alBufferData(buffer, format, data, size, freq);
    if (alGetError() != AL_NO_ERROR)
    {
        fprintf(stderr, "BufferData: Error buffering data.\n");
        return false;
    }
    return true;
}

boolean I_OAL_CacheSound(ALuint *buffer, ALenum format, const byte *data,
                         ALsizei size, ALsizei freq)
{
    if (!oal.device)
    {
        return false;
    }

    if (!CreateBuffer(buffer))
    {
        return false;
    }

    if (!BufferData(*buffer, format, data, size, freq))
    {
        return false;
    }

    if (!UpdateBufferArray(*buffer))
    {
        return false;
    }

    return true;
}

static boolean SetSourceGain(int channel, ALfloat volume)
{
    alGetError();
    alSourcef(oal.sources[channel], AL_GAIN, volume);
    if (alGetError() != AL_NO_ERROR)
    {
        fprintf(stderr, "SetSourceGain: Error setting gain.\n");
        return false;
    }
    return true;
}

static boolean SetSourcePosition(int channel, ALfloat x, ALfloat y, ALfloat z)
{
    alGetError();
    alSource3f(oal.sources[channel], AL_POSITION, x, y, z);
    if (alGetError() != AL_NO_ERROR)
    {
        fprintf(stderr, "SetSourcePosition: Error setting position.\n");
        return false;
    }
    return true;
}

static boolean AssignBufferToSource(int channel, ALuint buffer)
{
    alGetError();
    alSourcei(oal.sources[channel], AL_BUFFER, buffer);
    if (alGetError() != AL_NO_ERROR)
    {
        fprintf(stderr, "AssignBufferToSource: Error selecting buffer.\n");
        return false;
    }
    return true;
}

static boolean SetSourcePitch(int channel, int pitch)
{
    alGetError();
    alSourcef(oal.sources[channel], AL_PITCH, steptable[pitch]);
    if (alGetError() != AL_NO_ERROR)
    {
        fprintf(stderr, "SetSourcePitch: Error setting pitch.\n");
        return false;
    }
    return true;
}

static boolean SourcePlay(int channel)
{
    alGetError();
    alSourcePlay(oal.sources[channel]);
    if (alGetError() != AL_NO_ERROR)
    {
        fprintf(stderr, "SourcePlay: Error playing source.\n");
        return false;
    }
    return true;
}

void I_OAL_UpdateSoundParams2D(int channel, int volume, int separation)
{
    ALfloat pan;

    if (!oal.device)
    {
        return;
    }

    SetSourceGain(channel, (ALfloat)volume / 127.0f);

    pan = (ALfloat)separation / 255.0f - 0.5f;

    // Emulate 2D panning (https://github.com/kcat/openal-soft/issues/194).
    SetSourcePosition(channel, pan, 0.0f, -sqrtf(1.0f - pan * pan));
}

boolean I_OAL_StartSound(int channel, ALuint buffer, int pitch)
{
    if (!oal.device)
    {
        return false;
    }

    if (!AssignBufferToSource(channel, buffer))
    {
        return false;
    }

    if (pitch != NORM_PITCH)
    {
        if (!SetSourcePitch(channel, pitch))
        {
            return false;
        }
    }

    if (!SourcePlay(channel))
    {
        return false;
    }

    return true;
}

void I_OAL_StopSound(int channel)
{
    if (!oal.device)
    {
        return;
    }

    alGetError();
    alSourceStop(oal.sources[channel]);
    if (alGetError() != AL_NO_ERROR)
    {
        fprintf(stderr, "I_OAL_StopSound: Error stopping source.\n");
    }
}

boolean I_OAL_SoundIsPlaying(int channel)
{
    ALint state;

    if (!oal.device)
    {
        return false;
    }

    alGetError();
    alGetSourcei(oal.sources[channel], AL_SOURCE_STATE, &state);
    if (alGetError() != AL_NO_ERROR)
    {
        fprintf(stderr, "I_OAL_SoundIsPlaying: Error reading state.\n");
        return false;
    }

    return state == AL_PLAYING;
}

void I_OAL_ShutdownSound(void)
{
    DeleteSources();
    DeleteBuffers();
    FreeAttributes();
    DestroyContext();
    CloseDevice();
}

const sound_module_t sound_oal_module =
{
    I_OAL_InitSound,
    I_OAL_CacheSound,
    NULL, // [ceski] Placeholder.
    NULL, // [ceski] Placeholder.
    I_OAL_StartSound,
    I_OAL_StopSound,
    I_OAL_SoundIsPlaying,
    I_OAL_ShutdownSound,
};
