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

#include "al.h"
#include "alc.h"
#include "alext.h"
#include "efx.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "i_oalsound.h"
#include "i_printf.h"
#include "i_sndfile.h"
#include "i_sound.h"
#include "m_fixed.h"
#include "sounds.h"
#include "w_wad.h"
#include "z_zone.h"

#define OAL_ROLLOFF_FACTOR 1
#define OAL_SPEED_OF_SOUND 343.3f
// 128 map units per 3 meters (https://doomwiki.org/wiki/Map_unit).
#define OAL_MAP_UNITS_PER_METER (128.0f / 3.0f)
#define OAL_SOURCE_RADIUS 32.0f
#define OAL_DEFAULT_PITCH 1.0f
#define OAL_NUM_ATTRIBS 5

#define DMXHDRSIZE 8
#define DMXPADSIZE 16

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

boolean oal_use_doppler;

static const char *oal_resamplers[] = {
    "Nearest", "Linear", "Cubic"
};

typedef struct oal_system_s
{
    ALCdevice *device;
    ALCcontext *context;
    ALuint *sources;
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

void I_OAL_ShutdownModule(void)
{
    int i;

    if (!oal)
    {
        return;
    }

    for (i = 0; i < MAX_CHANNELS; ++i)
    {
        alSourcei(oal->sources[i], AL_BUFFER, 0);
    }

    for (i = 0; i < num_sfx; ++i)
    {
        if (S_sfx[i].cached)
        {
            alDeleteBuffers(1, &S_sfx[i].buffer);
            S_sfx[i].cached = false;
            S_sfx[i].lumpnum = -1;
        }
    }
}

void I_OAL_ShutdownSound(void)
{
    int i;

    for (i = 0; i < MAX_CHANNELS; ++i)
    {
        I_OAL_StopSound(i);
    }
    I_OAL_ShutdownModule();

    if (!oal)
    {
        return;
    }

    if (oal->sources)
    {
        alDeleteSources(MAX_CHANNELS, oal->sources);
        free(oal->sources);
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
        I_Printf(VB_WARNING, " Resampler info not available!");
        return;
    }

    alGetStringiSOFT = FUNCTION_CAST(LPALGETSTRINGISOFT,
                                     alGetProcAddress("alGetStringiSOFT"));

    if (!alGetStringiSOFT)
    {
        I_Printf(VB_WARNING, " alGetStringiSOFT() is not available.");
        return;
    }

    num_resamplers = alGetInteger(AL_NUM_RESAMPLERS_SOFT);
    def_resampler = alGetInteger(AL_DEFAULT_RESAMPLER_SOFT);

    if (!num_resamplers)
    {
        I_Printf(VB_WARNING, " No resamplers found!");
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
        I_Printf(VB_WARNING, " Failed to find resampler: '%s'.", resampler_name);
        return;
    }

    for (i = 0; i < MAX_CHANNELS; i++)
    {
        alSourcei(sources[i], AL_SOURCE_RESAMPLER_SOFT, def_resampler);
    }

    I_Printf(VB_DEBUG, " Using '%s' resampler.",
           alGetStringiSOFT(AL_RESAMPLER_NAME_SOFT, def_resampler));
}

void I_OAL_ResetSource2D(int channel)
{
    if (!oal)
    {
        return;
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

    alSourcei(oal->sources[channel], AL_ROLLOFF_FACTOR, 0);
    alSourcei(oal->sources[channel], AL_SOURCE_RELATIVE, AL_TRUE);
}

void I_OAL_ResetSource3D(int channel, boolean point_source)
{
    if (!oal)
    {
        return;
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

void I_OAL_UpdateSourceParams(int channel, const ALfloat *position,
                              const ALfloat *velocity)
{
    if (!oal)
    {
        return;
    }

    alSourcefv(oal->sources[channel], AL_POSITION, position);
    alSourcefv(oal->sources[channel], AL_VELOCITY, velocity);
}

void I_OAL_UpdateListenerParams(const ALfloat *position, const ALfloat *velocity,
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

static void UpdateUserSoundSettings(void)
{
    SetResampler(oal->sources);

    if (snd_module == SND_MODULE_3D)
    {
        oal->absorption = (ALfloat)snd_absorption / 2.0f;
        alDopplerFactor((ALfloat)snd_doppler * snd_doppler / 100.0f);
        oal_use_doppler = (snd_doppler > 0);
    }
    else
    {
        oal->absorption = 0.0f;
        alDopplerFactor(0.0f);
        oal_use_doppler = false;
    }
}

static void ResetParams(void)
{
    const ALint default_orientation[] = {0, 0, -1, 0, 1, 0};
    int i;

    // Source parameters.
    for (i = 0; i < MAX_CHANNELS; i++)
    {
        I_OAL_ResetSource2D(i);
        alSource3i(oal->sources[i], AL_DIRECTION, 0, 0, 0);
        alSourcei(oal->sources[i], AL_MAX_DISTANCE, S_ATTENUATOR);
        alSourcei(oal->sources[i], AL_REFERENCE_DISTANCE, S_CLOSE_DIST >> FRACBITS);
    }
    // Spatialization is required even for 2D panning emulation.
    if (oal->SOFT_source_spatialize)
    {
        for (i = 0; i < MAX_CHANNELS; i++)
        {
            alSourcei(oal->sources[i], AL_SOURCE_SPATIALIZE_SOFT, AL_TRUE);
        }
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

    UpdateUserSoundSettings();
}

static void PrintDeviceInfo(ALCdevice *device)
{
    const ALCchar *name;
    ALCint srate = -1;

    if (alcIsExtensionPresent(device, "ALC_ENUMERATE_ALL_EXT") == ALC_TRUE)
        name = alcGetString(device, ALC_ALL_DEVICES_SPECIFIER);
    else
        name = alcGetString(device, ALC_DEVICE_SPECIFIER);

    alcGetIntegerv(device, ALC_FREQUENCY, 1, &srate);
    I_Printf(VB_INFO, " Using '%s' @ %d Hz.", name, srate);
}

static void GetAttribs(ALCint *attribs)
{
    const boolean use_3d = (snd_module == SND_MODULE_3D);
    int i = 0;

    memset(attribs, 0, sizeof(*attribs) * OAL_NUM_ATTRIBS);

    if (alcIsExtensionPresent(oal->device, "ALC_SOFT_HRTF") == ALC_TRUE)
    {
        attribs[i++] = ALC_HRTF_SOFT;
        attribs[i++] = use_3d ? (snd_hrtf ? ALC_TRUE : ALC_FALSE) : ALC_FALSE;
    }

#ifdef ALC_OUTPUT_MODE_SOFT
    if (alcIsExtensionPresent(oal->device, "ALC_SOFT_output_mode") == ALC_TRUE)
    {
        attribs[i++] = ALC_OUTPUT_MODE_SOFT;
        attribs[i++] = use_3d ? (snd_hrtf ? ALC_STEREO_HRTF_SOFT : ALC_ANY_SOFT) :
                                ALC_STEREO_BASIC_SOFT;
    }
#endif
}

boolean I_OAL_InitSound(void)
{
    ALCint attribs[OAL_NUM_ATTRIBS];

    if (oal)
    {
        I_OAL_ShutdownSound();
    }

    oal = calloc(1, sizeof(*oal));
    oal->device = alcOpenDevice(NULL);
    if (!oal->device)
    {
        I_Printf(VB_ERROR, "I_OAL_InitSound: Failed to open device.");
        free(oal);
        oal = NULL;
        return false;
    }

    GetAttribs(attribs);
    oal->context = alcCreateContext(oal->device, attribs);
    if (!oal->context || !alcMakeContextCurrent(oal->context))
    {
        I_Printf(VB_ERROR, "I_OAL_InitSound: Error creating context.");
        I_OAL_ShutdownSound();
        return false;
    }
    PrintDeviceInfo(oal->device);

    oal->sources = malloc(sizeof(*oal->sources) * MAX_CHANNELS);
    alGetError();
    alGenSources(MAX_CHANNELS, oal->sources);
    if (!oal->sources || alGetError() != AL_NO_ERROR)
    {
        I_Printf(VB_ERROR, "I_OAL_InitSound: Error creating sources.");
        I_OAL_ShutdownSound();
        return false;
    }

    oal->SOFT_source_spatialize = (alIsExtensionPresent("AL_SOFT_source_spatialize") == AL_TRUE);
    oal->EXT_EFX = (alcIsExtensionPresent(oal->device, "ALC_EXT_EFX") == ALC_TRUE);
    oal->EXT_SOURCE_RADIUS = (alIsExtensionPresent("AL_EXT_SOURCE_RADIUS") == AL_TRUE);
    InitDeferred();
    ResetParams();

    return true;
}

boolean I_OAL_ReinitSound(void)
{
    LPALCRESETDEVICESOFT alcResetDeviceSOFT = NULL;
    ALCint attribs[OAL_NUM_ATTRIBS];

    if (!oal)
    {
        I_Printf(VB_ERROR, "I_OAL_ReinitSound: OpenAL not initialized.");
        return false;
    }

    if (alcIsExtensionPresent(oal->device, "ALC_SOFT_HRTF") != ALC_TRUE)
    {
        I_Printf(VB_ERROR, "I_OAL_ReinitSound: Extension not present.");
        return false;
    }

    alcResetDeviceSOFT = FUNCTION_CAST(LPALCRESETDEVICESOFT,
                                       alGetProcAddress("alcResetDeviceSOFT"));

    if (!alcResetDeviceSOFT)
    {
        I_Printf(VB_ERROR, "I_OAL_ReinitSound: Function address not found.");
        return false;
    }

    GetAttribs(attribs);

    if (alcResetDeviceSOFT(oal->device, attribs) != ALC_TRUE)
    {
        I_Printf(VB_ERROR, "I_OAL_ReinitSound: Error resetting device.");
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

    // alcResetDeviceSOFT() is part of the ALC_SOFT_HRTF extension.
    return (alcIsExtensionPresent(oal->device, "ALC_SOFT_HRTF") == ALC_TRUE);
}

//
// IsPaddedSound
//
// DMX sounds use 16 bytes of padding before and after the real sound. The
// padding bytes are equal to the first or last real byte, respectively.
// Reference: https://www.doomworld.com/forum/post/949486
//
static boolean IsPaddedSound(const byte *data, int size)
{
    const int sound_end = size - DMXPADSIZE;
    int i;

    for (i = 0; i < DMXPADSIZE; i++)
    {
        // Check padding before sound.
        if (data[i] != data[DMXPADSIZE])
        {
            return false;
        }

        // Check padding after sound.
        if (data[sound_end + i] != data[sound_end - 1])
        {
            return false;
        }
    }

    return true;
}

static void FadeInMono8(byte *data, ALsizei size, ALsizei freq)
{
    const int fadelen = freq * FADETIME / 1000000;
    int i;

    if (data[0] == 128 || size < fadelen)
    {
        return;
    }

    for (i = 0; i < fadelen; i++)
    {
        data[i] = (data[i] - 128) * i / fadelen + 128;
    }
}

boolean I_OAL_CacheSound(sfxinfo_t *sfx)
{
    int lumpnum;
    byte *lumpdata = NULL, *wavdata = NULL;

    if (!oal)
    {
        return false;
    }

    lumpnum = I_GetSfxLumpNum(sfx);

    if (lumpnum < 0)
    {
        return false;
    }

    // haleyjd 06/03/06: rewrote again to make sound data properly freeable
    while (sfx->cached == false)
    {
        byte *sampledata;
        int lumplen;

        ALsizei size, freq;
        ALenum format;
        ALuint buffer;

        // haleyjd: this should always be called (if lump is already loaded,
        // W_CacheLumpNum handles that for us).
        lumpdata = (byte *)W_CacheLumpNum(lumpnum, PU_STATIC);

        lumplen = W_LumpLength(lumpnum);

        // Check the header, and ensure this is a valid sound
        if (lumplen > DMXHDRSIZE && lumpdata[0] == 0x03 && lumpdata[1] == 0x00)
        {
            freq = (lumpdata[3] <<  8) |  lumpdata[2];
            size = (lumpdata[7] << 24) | (lumpdata[6] << 16) |
                   (lumpdata[5] <<  8) |  lumpdata[4];

            // Don't play sounds that think they're longer than they really are,
            // only contain padding, or are shorter than the padding size.
            if (size > lumplen - DMXHDRSIZE || size <= DMXPADSIZE * 2)
            {
                break;
            }

            sampledata = lumpdata + DMXHDRSIZE;

            if (IsPaddedSound(sampledata, size))
            {
                // Ignore DMX padding.
                sampledata += DMXPADSIZE;
                size -= DMXPADSIZE * 2;
            }

            // All Doom sounds are 8-bit
            format = AL_FORMAT_MONO8;

            // Fade in sounds that start at a non-zero amplitude to prevent clicking.
            FadeInMono8(sampledata, size, freq);
        }
        else
        {
            size = lumplen;

            if (I_SND_LoadFile(lumpdata, &format, &wavdata, &size, &freq) == false)
            {
                I_Printf(VB_WARNING, " I_OAL_CacheSound: %s", lumpinfo[lumpnum].name);
                break;
            }

            sampledata = wavdata;
        }

        alGetError();
        alGenBuffers(1, &buffer);
        if (alGetError() != AL_NO_ERROR)
        {
            I_Printf(VB_ERROR, "I_OAL_CacheSound: Error creating buffers.");
            break;
        }
        alBufferData(buffer, format, sampledata, size, freq);
        if (alGetError() != AL_NO_ERROR)
        {
            I_Printf(VB_ERROR, "I_OAL_CacheSound: Error buffering data.");
            break;
        }

        sfx->buffer = buffer;
        sfx->cached = true;
    }

    // don't need original lump data any more
    if (lumpdata)
    {
        Z_Free(lumpdata);
    }
    if (wavdata)
    {
        free(wavdata);
    }

    if (sfx->cached == false)
    {
      sfx->lumpnum = -2; // [FG] don't try again
      return false;
    }

    return true;
}

boolean I_OAL_StartSound(int channel, sfxinfo_t *sfx, int pitch)
{
    if (!oal)
    {
        return false;
    }

    alSourcef(oal->sources[channel], AL_PITCH,
              pitch == NORM_PITCH ? OAL_DEFAULT_PITCH : steptable[pitch]);

    alSourcei(oal->sources[channel], AL_BUFFER, sfx->buffer);

    alGetError();
    alSourcePlay(oal->sources[channel]);
    if (alGetError() != AL_NO_ERROR)
    {
        I_Printf(VB_ERROR, "I_OAL_StartSound: Error playing source.");
        return false;
    }

    return true;
}

void I_OAL_StopSound(int channel)
{
    if (!oal)
    {
        return;
    }

    alSourceStop(oal->sources[channel]);
}

boolean I_OAL_SoundIsPlaying(int channel)
{
    ALint state;

    if (!oal)
    {
        return false;
    }

    alGetSourcei(oal->sources[channel], AL_SOURCE_STATE, &state);

    return (state == AL_PLAYING);
}

void I_OAL_SetVolume(int channel, int volume)
{
    if (!oal)
    {
        return;
    }

    alSourcef(oal->sources[channel], AL_GAIN, VOL_TO_GAIN(volume));
}

void I_OAL_SetPan(int channel, int separation)
{
    ALfloat pan;

    if (!oal)
    {
        return;
    }

    // Emulate 2D panning (https://github.com/kcat/openal-soft/issues/194).
    // This works by sliding the sound source along the x-axis while inverting
    // the circular shape of the sound field along the z-axis. The end result
    // is perceived to move in a straight line along the x-axis only (panning).
    pan = (ALfloat)separation / 255.0f - 0.5f;
    alSource3f(oal->sources[channel], AL_POSITION, pan, 0.0f, -sqrtf(1.0f - pan * pan));
}
