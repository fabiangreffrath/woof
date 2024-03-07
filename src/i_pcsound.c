//
// Copyright(C) 2005-2014 Simon Howard
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
//    PC speaker interface.
//

#include "SDL.h"
#include "al.h"
#include "alext.h"

#include "doomstat.h"
#include "doomtype.h"
#include "i_oalsound.h"
#include "i_printf.h"
#include "i_sound.h"
#include "m_fixed.h"
#include "m_misc.h"
#include "p_mobj.h"
#include "sounds.h"
#include "w_wad.h"
#include "z_zone.h"

// C doesn't allow casting between function and non-function pointer types, so
// with C99 we need to use a union to reinterpret the pointer type. Pre-C99
// still needs to use a normal cast and live with the warning (C++ is fine with
// a regular reinterpret_cast).
#if __STDC_VERSION__ >= 199901L
#  define FUNCTION_CAST(T, ptr) (union{void *p; T f;}){ptr}.f
#else
#  define FUNCTION_CAST(T, ptr) (T)(ptr)
#endif

static LPALBUFFERCALLBACKSOFT alBufferCallbackSOFT;
static ALuint callback_buffer;
static ALuint callback_source;

#define SQUARE_WAVE_AMP 0x2000

static SDL_mutex *sound_lock;
static int mixing_freq;

// Currently playing sound
// current_remaining is the number of remaining samples that must be played
// before we invoke the callback to get the next frequency.

static int current_remaining;
static int current_freq;

static int phase_offset = 0;

#define TIMER_FREQ 1193181 /* hz */

static uint8_t *current_sound_lump = NULL;
static uint8_t *current_sound_pos = NULL;
static unsigned int current_sound_remaining = 0;
static int current_sound_handle = 0;

static const uint16_t divisors[] =
{
    0,    6818, 6628, 6449, 6279, 6087, 5906, 5736, 5575, 5423, 5279, 5120,
    4971, 4830, 4697, 4554, 4435, 4307, 4186, 4058, 3950, 3836, 3728, 3615,
    3519, 3418, 3323, 3224, 3131, 3043, 2960, 2875, 2794, 2711, 2633, 2560,
    2485, 2415, 2348, 2281, 2213, 2153, 2089, 2032, 1975, 1918, 1864, 1810,
    1757, 1709, 1659, 1612, 1565, 1521, 1478, 1435, 1395, 1355, 1316, 1280,
    1242, 1207, 1173, 1140, 1107, 1075, 1045, 1015, 986,  959,  931,  905,
    879,  854,  829,  806,  783,  760,  739,  718,  697,  677,  658,  640,
    621,  604,  586,  570,  553,  538,  522,  507,  493,  479,  465,  452,
    439,  427,  415,  403,  391,  380,  369,  359,  348,  339,  329,  319,
    310,  302,  293,  285,  276,  269,  261,  253,  246,  239,  232,  226,
    219,  213,  207,  201,  195,  190,  184,  179,
};

static void GetFreq(int *duration, int *freq)
{
    unsigned int tone;

    *duration = 1000 / 140;

    if (current_sound_lump != NULL && current_sound_remaining > 0)
    {
        // Read the next tone

        tone = *current_sound_pos;

        // Use the tone -> frequency lookup table.  See pcspkr10.zip
        // for a full discussion of this.
        // Check we don't overflow the frequency table.

        if (tone < arrlen(divisors) && divisors[tone] != 0)
        {
            *freq = (int)(TIMER_FREQ / divisors[tone]);
        }
        else
        {
            *freq = 0;
        }

        ++current_sound_pos;
        --current_sound_remaining;
    }
    else
    {
        *freq = 0;
    }
}

static int GetLumpNum(sfxinfo_t *sfx)
{
    if (sfx->lumpnum == -1)
    {
        char namebuf[9];
        M_snprintf(namebuf, sizeof(namebuf), "dp%s", sfx->name);
        sfx->lumpnum = W_CheckNumForName(namebuf);
    }

    return sfx->lumpnum;
}

static boolean CachePCSLump(sfxinfo_t *sfxinfo)
{
    int lumpnum, lumplen;
    int headerlen;

    // Free the current sound lump back to the cache

    if (current_sound_lump != NULL)
    {
        Z_Free(current_sound_lump);
        current_sound_lump = NULL;
    }

    lumpnum = GetLumpNum(sfxinfo);
    if (lumpnum < 0)
    {
        return false;
    }

    // Load from WAD

    current_sound_lump = W_CacheLumpNum(lumpnum, PU_STATIC);
    lumplen = W_LumpLength(lumpnum);

    // Read header

    if (current_sound_lump[0] != 0x00 || current_sound_lump[1] != 0x00)
    {
        return false;
    }

    headerlen = (current_sound_lump[3] << 8) | current_sound_lump[2];

    if (headerlen > lumplen - 4)
    {
        return false;
    }

    // Header checks out ok

    current_sound_remaining = headerlen;
    current_sound_pos = current_sound_lump + 4;

    return true;
}

// These Doom PC speaker sounds are not played - this can be seen in the
// Heretic source code, where there are remnants of this left over
// from Doom.

static boolean IsDisabledSound(sfxinfo_t *sfxinfo)
{
    int i;
    const char *disabled_sounds[] = {
        "posact", "bgact", "dmact", "dmpain", "popain", "sawidl",
    };

    for (i = 0; i < arrlen(disabled_sounds); ++i)
    {
        if (!strcmp(sfxinfo->name, disabled_sounds[i]))
        {
            return true;
        }
    }

    return false;
}

// Mixer function that does the PC speaker emulation

static ALsizei AL_APIENTRY BufferCallback(void *userptr, void *data,
                                          ALsizei size)
{
    int16_t *leftptr;
    int16_t *rightptr;
    int16_t this_value;
    int frequency;
    int i;
    int nsamples;

    // Number of samples is quadrupled, because of 16-bit and stereo

    nsamples = size / 4;

    leftptr = (int16_t *)data;
    rightptr = ((int16_t *)data) + 1;

    // Fill the output buffer

    for (i = 0; i < nsamples; ++i)
    {
        // Has this sound expired? If so, invoke the callback to get the next
        // frequency.

        while (current_remaining == 0)
        {
            // Get the next frequency to play

            GetFreq(&current_remaining, &frequency);

            if (current_freq != frequency)
            {
                current_freq = frequency;
                phase_offset = 0;
            }

            current_remaining = (current_remaining * mixing_freq) / 1000;
        }

        // Set the value for this sample.

        if (current_freq == 0)
        {
            // Silence

            this_value = 0;
        }
        else
        {
            int frac;

            // Determine whether we are at a peak or trough in the current
            // sound.  Multiply by 2 so that frac % 2 will give 0 or 1
            // depending on whether we are at a peak or trough.

            frac = (phase_offset * current_freq * 2) / mixing_freq;

            if ((frac % 2) == 0)
            {
                this_value = SQUARE_WAVE_AMP;
            }
            else
            {
                this_value = -SQUARE_WAVE_AMP;
            }

            ++phase_offset;
        }

        --current_remaining;

        // Use the same value for the left and right channels.

        *leftptr = this_value;
        *rightptr = this_value;

        leftptr += 2;
        rightptr += 2;
    }

    return size;
}

static void RegisterCallback(void)
{
    if (!alIsExtensionPresent("AL_SOFT_callback_buffer"))
    {
        I_Printf(VB_ERROR,
                 "RegisterCallback: AL_SOFT_callback_buffer not found.");
        return;
    }
    alBufferCallbackSOFT = FUNCTION_CAST(
        LPALBUFFERCALLBACKSOFT, alGetProcAddress("alBufferCallbackSOFT"));

    alGenBuffers(1, &callback_buffer);
    alGenSources(1, &callback_source);
    alBufferCallbackSOFT(callback_buffer, AL_FORMAT_STEREO16, SND_SAMPLERATE,
                         BufferCallback, NULL);
    alSourcei(callback_source, AL_BUFFER, callback_buffer);
    if (alGetError() != AL_NO_ERROR)
    {
        I_Printf(VB_ERROR, "RegisterCallback: Failed to set callback.");
        return;
    }

    alSource3i(callback_source, AL_POSITION, 0, 0, -1);
    alSourcei(callback_source, AL_SOURCE_RELATIVE, AL_TRUE);
    alSourcei(callback_source, AL_ROLLOFF_FACTOR, 0);
    alSourcePlay(callback_source);
    if (alGetError() != AL_NO_ERROR)
    {
        I_Printf(VB_ERROR, "RegisterCallback: Failed start playback.");
        return;
    }
}

static void UnregisterCallback(void)
{
    alSourceStop(callback_source);
    alDeleteSources(1, &callback_source);
    alDeleteBuffers(1, &callback_buffer);
}

static void InitPCSound(void)
{
    mixing_freq = SND_SAMPLERATE;

    current_freq = 0;
    current_remaining = 0;

    RegisterCallback();

    if (!sound_lock)
    {
        sound_lock = SDL_CreateMutex();
    }
}

static boolean I_PCS_ReinitSound(void)
{
    if (!I_OAL_ReinitSound())
    {
        return false;
    }

    InitPCSound();

    return true;
}

static boolean I_PCS_InitSound(void)
{
    if (!I_OAL_InitSound())
    {
        return false;
    }

    InitPCSound();

    return true;
}

static void I_PCS_ShutdownModule(void)
{
    int i;

    for (i = 0; i < num_sfx; ++i)
    {
        S_sfx[i].lumpnum = -1;
    }

    UnregisterCallback();
}

static void I_PCS_ShutdownSound(void)
{
    I_PCS_ShutdownModule();

    I_OAL_ShutdownSound();
}

static boolean I_PCS_CacheSound(sfxinfo_t *sfx)
{
    if (IsDisabledSound(sfx))
    {
        return false;
    }

    return (GetLumpNum(sfx) != -1);
}

static boolean I_PCS_AdjustSoundParams(const mobj_t *listener,
                                       const mobj_t *source, int chanvol,
                                       int *vol, int *sep, int *pri)
{
    fixed_t adx, ady, approx_dist;

    if (!source || source == players[displayplayer].mo)
    {
        return true;
    }

    // haleyjd 08/12/04: we cannot adjust a sound for a NULL listener.
    if (!listener)
    {
        return true;
    }

    // calculate the distance to sound origin
    //  and clip it if necessary
    adx = abs(listener->x - source->x);
    ady = abs(listener->y - source->y);

    // From _GG1_ p.428. Appox. eucledian distance fast.
    approx_dist = adx + ady - ((adx < ady ? adx : ady) >> 1);

    if (approx_dist > S_CLIPPING_DIST)
    {
        return false;
    }

    // volume calculation
    if (approx_dist < S_CLOSE_DIST)
    {
        *vol = snd_SfxVolume;
    }
    else
    {
        // distance effect
        *vol = (snd_SfxVolume * ((S_CLIPPING_DIST - approx_dist) >> FRACBITS))
               / S_ATTENUATOR;
    }

    return (*vol > 0);
}

static void I_PCS_UpdateSoundParams(int channel, int volume, int separation)
{
    // adjust PC Speaker volume
    alSourcef(callback_source, AL_GAIN, (float)snd_SfxVolume / 15);
}

static boolean I_PCS_StartSound(int channel, sfxinfo_t *sfx, int pitch)
{
    boolean result;

    if (IsDisabledSound(sfx))
    {
        return false;
    }

    if (SDL_LockMutex(sound_lock) < 0)
    {
        return false;
    }

    result = CachePCSLump(sfx);

    if (result)
    {
        current_sound_handle = channel;
    }

    SDL_UnlockMutex(sound_lock);

    if (result)
    {
        return true;
    }
    else
    {
        return false;
    }
}

static void I_PCS_StopSound(int channel)
{
    if (SDL_LockMutex(sound_lock) < 0)
    {
        return;
    }

    // If this is the channel currently playing, immediately end it.

    if (current_sound_handle == channel)
    {
        current_sound_remaining = 0;
    }

    SDL_UnlockMutex(sound_lock);
}

static boolean I_PCS_SoundIsPlaying(int channel)
{
    if (channel != current_sound_handle)
    {
        return false;
    }

    return current_sound_lump != NULL && current_sound_remaining > 0;
}

const sound_module_t sound_pcs_module =
{
    I_PCS_InitSound,
    I_PCS_ReinitSound,
    I_OAL_AllowReinitSound,
    I_PCS_CacheSound,
    I_PCS_AdjustSoundParams,
    I_PCS_UpdateSoundParams,
    NULL,
    I_PCS_StartSound,
    I_PCS_StopSound,
    I_PCS_SoundIsPlaying,
    I_PCS_ShutdownSound,
    I_PCS_ShutdownModule,
    I_OAL_DeferUpdates,
    I_OAL_ProcessUpdates,
};
