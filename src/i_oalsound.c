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

#include "doomstat.h"
#include "i_oalsound.h"
#include "r_data.h"

#define OAL_ROLLOFF_FACTOR 1
#define OAL_SPEED_OF_SOUND 343.3f
#define OAL_MAP_UNITS_PER_METER (128.0f / 3.0f) // 128 map units per 3 meters.
#define OAL_SOURCE_RADIUS 32.0f
#define OAL_DEFAULT_PITCH 1.0f
#define OAL_NUM_ATTRIBS 5

#define VOL_TO_GAIN(x) ((ALfloat)(x) / 127)

// C doesn't allow casting between function and non-function pointer types, so
// with C99 we need to use a union to reinterpret the pointer type. Pre-C99
// still needs to use a normal cast and live with the warning (C++ is fine with
// a regular reinterpret_cast).
#if __STDC_VERSION__ >= 199901L
#define FUNCTION_CAST(T, ptr) (union{void *p; T f;}){ptr}.f
#else
#define FUNCTION_CAST(T, ptr) (T)(ptr)
#endif

int snd_resampler;
boolean snd_hrtf;
int snd_absorption;
int snd_doppler;

static const char *oal_resamplers[] = {
    "Nearest", "Linear", "Cubic"
};

typedef struct oal_system_s
{
    ALCdevice *device;
    ALCcontext *context;
    ALuint *sources;
    boolean *active;
    ALuint *buffers;
    int num_buffers;
    int num_buffers_mem;
    boolean SOFT_source_spatialize;
    boolean EXT_EFX;
    boolean EXT_SOURCE_RADIUS;
    ALfloat absorption;
} oal_system_t;

static oal_system_t *oal;
static LPALDEFERUPDATESSOFT alDeferUpdatesSOFT;
static LPALPROCESSUPDATESSOFT alProcessUpdatesSOFT;

void I_OAL_DeferUpdates(void)
{
    if (!oal)
    {
        return;
    }

    alDeferUpdatesSOFT();
}

void I_OAL_ProcessUpdates(void)
{
    if (!oal)
    {
        return;
    }

    alProcessUpdatesSOFT();
}

static void AL_APIENTRY wrap_DeferUpdatesSOFT(void)
{
    alcSuspendContext(alcGetCurrentContext());
}

static void AL_APIENTRY wrap_ProcessUpdatesSOFT(void)
{
    alcProcessContext(alcGetCurrentContext());
}

static void InitDeferred(void)
{
    // Deferred updates allow values to be updated simultaneously.
    // https://openal-soft.org/openal-extensions/SOFT_deferred_updates.txt

    if (alIsExtensionPresent("AL_SOFT_deferred_updates") == AL_TRUE)
    {
        alDeferUpdatesSOFT = FUNCTION_CAST(LPALDEFERUPDATESSOFT,
                                           alGetProcAddress("alDeferUpdatesSOFT"));
        alProcessUpdatesSOFT = FUNCTION_CAST(LPALPROCESSUPDATESSOFT,
                                             alGetProcAddress("alProcessUpdatesSOFT"));

        if (alDeferUpdatesSOFT && alProcessUpdatesSOFT)
        {
            return;
        }
    }

    alDeferUpdatesSOFT = FUNCTION_CAST(LPALDEFERUPDATESSOFT, &wrap_DeferUpdatesSOFT);
    alProcessUpdatesSOFT = FUNCTION_CAST(LPALPROCESSUPDATESSOFT, &wrap_ProcessUpdatesSOFT);
}

void I_OAL_ShutdownSound(void)
{
    if (!oal)
    {
        return;
    }

    if (oal->sources)
    {
        alDeleteSources(MAX_CHANNELS, oal->sources);
        free(oal->sources);
    }

    if (oal->active)
    {
        free(oal->active);
    }

    if (oal->buffers)
    {
        if (oal->num_buffers > 0)
        {
            alDeleteBuffers(oal->num_buffers, oal->buffers);
        }
        free(oal->buffers);
    }

    alcMakeContextCurrent(NULL);
    if (oal->context)
    {
        alcDestroyContext(oal->context);
    }

    if (oal->device)
    {
        alcCloseDevice(oal->device);
    }

    free(oal);
    oal = NULL;
}

static void SetResampler(ALuint *sources)
{
    const char *resampler_name = oal_resamplers[snd_resampler];
    LPALGETSTRINGISOFT alGetStringiSOFT = NULL;
    ALint i, num_resamplers, def_resampler;

    if (alIsExtensionPresent("AL_SOFT_source_resampler") != AL_TRUE)
    {
        printf(" Resampler info not available!\n");
        return;
    }

    alGetStringiSOFT = FUNCTION_CAST(LPALGETSTRINGISOFT,
                                     alGetProcAddress("alGetStringiSOFT"));

    if (!alGetStringiSOFT)
    {
        fprintf(stderr, " alGetStringiSOFT() is not available.\n");
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
        if (!strcasecmp(resampler_name, alGetStringiSOFT(AL_RESAMPLER_NAME_SOFT, i)))
        {
            def_resampler = i;
            break;
        }
    }
    if (i == num_resamplers)
    {
        printf(" Failed to find resampler: '%s'.\n", resampler_name);
        return;
    }

    for (i = 0; i < MAX_CHANNELS; i++)
    {
        alSourcei(sources[i], AL_SOURCE_RESAMPLER_SOFT, def_resampler);
    }
/*
    printf(" Using '%s' resampler.\n",
           alGetStringiSOFT(AL_RESAMPLER_NAME_SOFT, def_resampler));
*/
}

void I_OAL_ResetSource2D(int channel)
{
    if (!oal)
    {
        return;
    }

    // Don't reset while sound is playing.
    if (oal->active[channel])
    {
        return;
    }

    if (oal->SOFT_source_spatialize)
    {
        alSourcei(oal->sources[channel], AL_SOURCE_SPATIALIZE_SOFT, AL_FALSE);
    }

    if (oal->EXT_EFX)
    {
        alSourcef(oal->sources[channel], AL_AIR_ABSORPTION_FACTOR, 0.0f);
    }

    if (oal->EXT_SOURCE_RADIUS)
    {
        alSourcef(oal->sources[channel], AL_SOURCE_RADIUS, 0.0f);
    }

    alSource3f(oal->sources[channel], AL_POSITION, 0.0f, 0.0f, 0.0f);
    alSource3f(oal->sources[channel], AL_VELOCITY, 0.0f, 0.0f, 0.0f);
    alSource3i(oal->sources[channel], AL_DIRECTION, 0, 0, 0);

    alSourcei(oal->sources[channel], AL_ROLLOFF_FACTOR, 0);
    alSourcei(oal->sources[channel], AL_SOURCE_RELATIVE, AL_TRUE);
}

void I_OAL_ResetSource3D(int channel, boolean point_source)
{
    if (!oal)
    {
        return;
    }

    // Don't reset while sound is playing.
    if (oal->active[channel])
    {
        return;
    }

    if (oal->SOFT_source_spatialize)
    {
        alSourcei(oal->sources[channel], AL_SOURCE_SPATIALIZE_SOFT, AL_TRUE);
    }

    if (oal->EXT_EFX)
    {
        alSourcef(oal->sources[channel], AL_AIR_ABSORPTION_FACTOR, oal->absorption);
    }

    if (oal->EXT_SOURCE_RADIUS)
    {
        alSourcef(oal->sources[channel], AL_SOURCE_RADIUS,
                  point_source ? 0.0f : OAL_SOURCE_RADIUS);
    }

    alSourcei(oal->sources[channel], AL_ROLLOFF_FACTOR, OAL_ROLLOFF_FACTOR);
    alSourcei(oal->sources[channel], AL_SOURCE_RELATIVE, AL_FALSE);
}

void I_OAL_AdjustSource3D(int channel, const ALfloat *position,
                          const ALfloat *velocity, const ALfloat *direction)
{
    if (!oal)
    {
        return;
    }

    alSourcefv(oal->sources[channel], AL_POSITION, position);
    alSourcefv(oal->sources[channel], AL_VELOCITY, velocity);
    alSourcefv(oal->sources[channel], AL_DIRECTION, direction);
}

void I_OAL_AdjustListener3D(const ALfloat *position, const ALfloat *velocity,
                            const ALfloat *orientation)
{
    if (!oal)
    {
        return;
    }

    alListenerfv(AL_POSITION, position);
    alListenerfv(AL_VELOCITY, velocity);
    alListenerfv(AL_ORIENTATION, orientation);
}

void I_OAL_UpdateUserSoundSettings(void)
{
    if (!oal)
    {
        return;
    }

    // Source parameters.
    SetResampler(oal->sources);
    oal->absorption = (ALfloat)snd_absorption / 2.0f;

    // Context state parameters.
    alDopplerFactor((ALfloat)snd_doppler * snd_doppler / 100.0f);
}

static void ResetParams(void)
{
    const ALint default_orientation[] = {0, 0, -1, 0, 1, 0};
    int i;

    alDeferUpdatesSOFT();

    // Source paramters.
    for (i = 0; i < MAX_CHANNELS; i++)
    {
        oal->active[i] = false;
        I_OAL_ResetSource2D(i);
        alSourcei(oal->sources[i], AL_MAX_DISTANCE, OAL_MAX_DISTANCE);
        alSourcei(oal->sources[i], AL_REFERENCE_DISTANCE, OAL_REF_DISTANCE);
    }

    // Listener parameters.
    alListener3i(AL_POSITION, 0, 0, 0);
    alListener3i(AL_VELOCITY, 0, 0, 0);
    alListeneriv(AL_ORIENTATION, default_orientation);
    if (oal->EXT_EFX)
    {
        alListenerf(AL_METERS_PER_UNIT, 1.0f / OAL_MAP_UNITS_PER_METER);
    }

    // Context state parameters.
    alDistanceModel(AL_INVERSE_DISTANCE_CLAMPED); // OpenAL 1.1 Specs, 3.4.2.
    alSpeedOfSound(OAL_SPEED_OF_SOUND * OAL_MAP_UNITS_PER_METER);

    I_OAL_UpdateUserSoundSettings();

    alProcessUpdatesSOFT();
}

static boolean OpenDevice(ALCdevice **device)
{
    const ALCchar *name;
    ALCint srate = -1;

    *device = alcOpenDevice(NULL);
    if (!*device)
    {
        return false;
    }

    if (alcIsExtensionPresent(*device, "ALC_ENUMERATE_ALL_EXT") == AL_TRUE)
        name = alcGetString(*device, ALC_ALL_DEVICES_SPECIFIER);
    else
        name = alcGetString(*device, ALC_DEVICE_SPECIFIER);

    alcGetIntegerv(*device, ALC_FREQUENCY, 1, &srate);
    printf(" Using '%s' @ %d Hz.\n", name, srate);

    return true;
}

static void GetAttribs(boolean use_3d, ALCint *attribs)
{
    int i = 0;

    memset(attribs, 0, sizeof(*attribs) * OAL_NUM_ATTRIBS);

    if (alcIsExtensionPresent(oal->device, "ALC_SOFT_HRTF") == AL_TRUE)
    {
        attribs[i++] = ALC_HRTF_SOFT;
        attribs[i++] = use_3d ? (snd_hrtf ? ALC_TRUE : ALC_FALSE) : ALC_FALSE;
    }

    if (alcIsExtensionPresent(oal->device, "ALC_SOFT_output_mode") == AL_TRUE)
    {
        attribs[i++] = ALC_OUTPUT_MODE_SOFT;
        attribs[i++] = use_3d ? (snd_hrtf ? ALC_STEREO_HRTF_SOFT : ALC_ANY_SOFT) :
                                ALC_STEREO_BASIC_SOFT;
    }
}

boolean I_OAL_InitSound(boolean use_3d)
{
    ALCdevice *device;
    ALCint attribs[OAL_NUM_ATTRIBS];

    if (oal)
    {
        I_OAL_ShutdownSound();
    }

    if (!OpenDevice(&device))
    {
        fprintf(stderr, "I_OAL_InitSound: Failed to open device.\n");
        return false;
    }

    oal = calloc(1, sizeof(*oal));
    oal->device = device;
    GetAttribs(use_3d, attribs);

    oal->context = alcCreateContext(oal->device, attribs);
    if (!oal->context || !alcMakeContextCurrent(oal->context))
    {
        fprintf(stderr, "I_OAL_InitSound: Error creating context.\n");
        I_OAL_ShutdownSound();
        return false;
    }

    oal->sources = malloc(sizeof(*oal->sources) * MAX_CHANNELS);
    alGetError();
    alGenSources(MAX_CHANNELS, oal->sources);
    if (!oal->sources || alGetError() != AL_NO_ERROR)
    {
        fprintf(stderr, "I_OAL_InitSound: Error creating sources.\n");
        I_OAL_ShutdownSound();
        return false;
    }

    oal->SOFT_source_spatialize = alIsExtensionPresent("AL_SOFT_source_spatialize") == AL_TRUE;
    oal->EXT_EFX = alcIsExtensionPresent(oal->device, "ALC_EXT_EFX") == AL_TRUE;
    oal->EXT_SOURCE_RADIUS = alIsExtensionPresent("AL_EXT_SOURCE_RADIUS") == AL_TRUE;
    oal->active = malloc(sizeof(*oal->active) * MAX_CHANNELS);
    InitDeferred();
    ResetParams();

    return true;
}

boolean I_OAL_ReinitSound(boolean use_3d)
{
    LPALCRESETDEVICESOFT alcResetDeviceSOFT = NULL;
    ALCint attribs[OAL_NUM_ATTRIBS];

    if (!oal)
    {
        fprintf(stderr, "I_OAL_ReinitSound: OpenAL not initialized.\n");
        return false;
    }

    if (alcIsExtensionPresent(oal->device, "ALC_SOFT_HRTF") != AL_TRUE)
    {
        fprintf(stderr, "I_OAL_ReinitSound: Extension not present.\n");
        return false;
    }

    alcResetDeviceSOFT = FUNCTION_CAST(LPALCRESETDEVICESOFT,
                                       alGetProcAddress("alcResetDeviceSOFT"));

    if (!alcResetDeviceSOFT)
    {
        fprintf(stderr, "I_OAL_ReinitSound: Function address not found.\n");
        return false;
    }

    GetAttribs(use_3d, attribs);

    if (alcResetDeviceSOFT(oal->device, attribs) != AL_TRUE)
    {
        fprintf(stderr, "I_OAL_ReinitSound: Error resetting device.\n");
        I_OAL_ShutdownSound();
        return false;
    }

    ResetParams();

    return true;
}

boolean I_OAL_AllowReinitSound(void)
{
    if (!oal)
    {
        return false;
    }

    return alcIsExtensionPresent(oal->device, "ALC_SOFT_HRTF") == AL_TRUE;
}

boolean I_OAL_CacheSound(ALuint *buffer, ALenum format, const byte *data,
                         ALsizei size, ALsizei freq)
{
    if (!oal)
    {
        return false;
    }

    alGetError();
    alGenBuffers(1, buffer);
    if (alGetError() != AL_NO_ERROR)
    {
        fprintf(stderr, "I_OAL_CacheSound: Error creating buffers.\n");
        return false;
    }

    alGetError();
    alBufferData(*buffer, format, data, size, freq);
    if (alGetError() != AL_NO_ERROR)
    {
        fprintf(stderr, "I_OAL_CacheSound: Error buffering data.\n");
        return false;
    }

    if (oal->num_buffers == oal->num_buffers_mem)
    {
        ALuint *new_buffers;
        oal->num_buffers_mem += NUMSFX;
        new_buffers = realloc(oal->buffers, sizeof(ALuint) * (oal->num_buffers_mem));
        if (new_buffers == NULL)
        {
            fprintf(stderr, "I_OAL_CacheSound: Error allocating buffers.\n");
            return false;
        }
        oal->buffers = new_buffers;
    }

    oal->buffers[oal->num_buffers] = *buffer;
    oal->num_buffers++;

    return true;
}

boolean I_OAL_StartSound(int channel, ALuint buffer, int pitch)
{
    if (!oal)
    {
        return false;
    }

    alDeferUpdatesSOFT();

    alGetError();
    alSourcef(oal->sources[channel], AL_PITCH,
              pitch == NORM_PITCH ? OAL_DEFAULT_PITCH : steptable[pitch]);
    if (alGetError() != AL_NO_ERROR)
    {
        fprintf(stderr, "I_OAL_StartSound: Error setting pitch.\n");
        return false;
    }

    alGetError();
    alSourcei(oal->sources[channel], AL_BUFFER, buffer);
    if (alGetError() != AL_NO_ERROR)
    {
        fprintf(stderr, "I_OAL_StartSound: Error selecting buffer.\n");
        return false;
    }

    alGetError();
    alSourcePlay(oal->sources[channel]);
    if (alGetError() != AL_NO_ERROR)
    {
        fprintf(stderr, "I_OAL_StartSound: Error playing source.\n");
        return false;
    }

    alProcessUpdatesSOFT();

    oal->active[channel] = true;

    return true;
}

void I_OAL_StopSound(int channel)
{
    if (!oal)
    {
        return;
    }

    alGetError();
    alSourceStop(oal->sources[channel]);
    if (alGetError() != AL_NO_ERROR)
    {
        fprintf(stderr, "I_OAL_StopSound: Error stopping source.\n");
    }

    oal->active[channel] = false;
}

boolean I_OAL_SoundIsPlaying(int channel)
{
    ALint state;

    if (!oal)
    {
        return false;
    }

    alGetError();
    alGetSourcei(oal->sources[channel], AL_SOURCE_STATE, &state);
    if (alGetError() != AL_NO_ERROR)
    {
        fprintf(stderr, "I_OAL_SoundIsPlaying: Error reading state.\n");
        return false;
    }

    return (state == AL_PLAYING);
}

void I_OAL_SetVolume(int channel, int volume)
{
    if (!oal)
    {
        return;
    }

    alGetError();
    alSourcef(oal->sources[channel], AL_GAIN, VOL_TO_GAIN(volume));
    if (alGetError() != AL_NO_ERROR)
    {
        fprintf(stderr, "I_OAL_SetVolume: Error setting gain.\n");
    }
}

void I_OAL_SetPan(int channel, int separation)
{
    ALfloat pan;

    if (!oal)
    {
        return;
    }

    // Emulate 2D panning (https://github.com/kcat/openal-soft/issues/194).
    pan = (ALfloat)separation / 255.0f - 0.5f;
    alGetError();
    alSource3f(oal->sources[channel], AL_POSITION, pan, 0.0f, -sqrtf(1.0f - pan * pan));
    if (alGetError() != AL_NO_ERROR)
    {
        fprintf(stderr, "I_OAL_SetPan: Error setting pan.\n");
    }
}