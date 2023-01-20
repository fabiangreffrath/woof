//
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2023 Fabian Greffrath
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
//	PC speaker interface.
//

// This file is an amalgamation of Chocolate Doom's pcsound/pcsound_sdl.c
// and src/i_pcsound.c files.
//
// Instead of mixing the sounds directly into the SDL_Mixer stream,
// the entire lump gets synthetized as regular wave file.

#include "doomtype.h"

#include "i_pcsound.h"
#include "i_sound.h"

#define TIMER_FREQ 1193181 /* hz */

#define SQUARE_WAVE_AMP 0x2000

static uint8_t *current_sound_pos = NULL;
static unsigned int current_sound_remaining = 0;

static const uint16_t divisors[] = {
    0,
    6818, 6628, 6449, 6279, 6087, 5906, 5736, 5575,
    5423, 5279, 5120, 4971, 4830, 4697, 4554, 4435,
    4307, 4186, 4058, 3950, 3836, 3728, 3615, 3519,
    3418, 3323, 3224, 3131, 3043, 2960, 2875, 2794,
    2711, 2633, 2560, 2485, 2415, 2348, 2281, 2213,
    2153, 2089, 2032, 1975, 1918, 1864, 1810, 1757,
    1709, 1659, 1612, 1565, 1521, 1478, 1435, 1395,
    1355, 1316, 1280, 1242, 1207, 1173, 1140, 1107,
    1075, 1045, 1015,  986,  959,  931,  905,  879,
     854,  829,  806,  783,  760,  739,  718,  697,
     677,  658,  640,  621,  604,  586,  570,  553,
     538,  522,  507,  493,  479,  465,  452,  439,
     427,  415,  403,  391,  380,  369,  359,  348,
     339,  329,  319,  310,  302,  293,  285,  276,
     269,  261,  253,  246,  239,  232,  226,  219,
     213,  207,  201,  195,  190,  184,  179,
};

static void PCSCallbackFunc(int *duration, int *freq)
{
    unsigned int tone;

    *duration = 1000 / 140;

    if (current_sound_remaining > 0)
    {
        // Read the next tone

        tone = *current_sound_pos;

        // Use the tone -> frequency lookup table.  See pcspkr10.zip
        // for a full discussion of this.
        // Check we don't overflow the frequency table.

        if (tone < arrlen(divisors) && divisors[tone] != 0)
        {
            *freq = (int) (TIMER_FREQ / divisors[tone]);
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

static boolean CachePCSLump(uint8_t *current_sound_lump, int lumplen)
{
    int headerlen;

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
        "posact",
        "bgact",
        "dmact",
        "dmpain",
        "popain",
        "sawidl",
    };

    for (i=0; i<arrlen(disabled_sounds); ++i)
    {
        if (!strcasecmp(sfxinfo->name, disabled_sounds[i]))
        {
            return true;
        }
    }

    return false;
}

// Output sound format

static int mixing_freq;

// Currently playing sound
// current_remaining is the number of remaining samples that must be played
// before we invoke the callback to get the next frequency.

static int current_remaining;
static int current_freq;

static int phase_offset = 0;

// Mixer function that does the PC speaker emulation

static void PCSound_Mix_Callback(int chan, void *stream, int len, void *udata)
{
    Sint16 *leftptr;
    Sint16 *rightptr;
    Sint16 this_value;
    int frequency;
    int i;
    int nsamples;

    // Number of samples is quadrupled, because of 16-bit and stereo

    nsamples = len / 4;

    leftptr = (Sint16 *) stream;
    rightptr = ((Sint16 *) stream) + 1;
    
    // Fill the output buffer

    for (i=0; i<nsamples; ++i)
    {
        // Has this sound expired? If so, invoke the callback to get 
        // the next frequency.

        while (current_remaining == 0) 
        {
            // Get the next frequency to play

            PCSCallbackFunc(&current_remaining, &frequency);

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

        *leftptr += this_value;
        *rightptr += this_value;

        leftptr += 2;
        rightptr += 2;
    }
}

void *Load_PCSound(sfxinfo_t *sfx, void *data, SDL_AudioSpec *sample, Uint8 **wavdata, Uint32 *samplelen)
{
    Uint32 wavlen;
    short *local_wavdata;

    mixing_freq = snd_samplerate;

    current_sound_pos = NULL;
    current_sound_remaining = 0;

    current_remaining = 0;
    current_freq = 0;
    phase_offset = 0;

    if (IsDisabledSound(sfx))
    {
        return NULL;
    }

    if (!CachePCSLump(data, *samplelen))
    {
        return NULL;
    }

    wavlen = current_sound_remaining * mixing_freq * 2 * sizeof(short) / 140;
    // ensure that the new buffer length is a multiple of sample size
    wavlen = (wavlen + 3) & (Uint32)~3;

    local_wavdata = SDL_malloc(wavlen);

    if (!local_wavdata)
    {
        return NULL;
    }

    PCSound_Mix_Callback(0, local_wavdata, wavlen, NULL);

    sample->channels = 2;
    sample->freq = mixing_freq;
    sample->format = AUDIO_S16;

    *wavdata = (Uint8 *)local_wavdata;
    *samplelen = wavlen;

    return *wavdata;
}
