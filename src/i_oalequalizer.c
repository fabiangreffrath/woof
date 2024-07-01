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
//      OpenAL equalizer.
//

#include "al.h"
#include "alc.h"
#include "alext.h"

#include <math.h>

#include "i_oalcommon.h"
#include "i_oalequalizer.h"
#include "i_sound.h"

eq_preset_t snd_equalizer;
int snd_eq_preamp;
int snd_eq_low_gain;
int snd_eq_low_cutoff;
int snd_eq_mid1_gain;
int snd_eq_mid1_center;
int snd_eq_mid1_width;
int snd_eq_mid2_gain;
int snd_eq_mid2_center;
int snd_eq_mid2_width;
int snd_eq_high_gain;
int snd_eq_high_cutoff;

static LPALGENAUXILIARYEFFECTSLOTS alGenAuxiliaryEffectSlots;
static LPALDELETEAUXILIARYEFFECTSLOTS alDeleteAuxiliaryEffectSlots;
static LPALISAUXILIARYEFFECTSLOT alIsAuxiliaryEffectSlot;
static LPALGENEFFECTS alGenEffects;
static LPALDELETEEFFECTS alDeleteEffects;
static LPALISEFFECT alIsEffect;
static LPALEFFECTI alEffecti;
static LPALEFFECTF alEffectf;
static LPALAUXILIARYEFFECTSLOTI alAuxiliaryEffectSloti;
static LPALGENFILTERS alGenFilters;
static LPALDELETEFILTERS alDeleteFilters;
static LPALISFILTER alIsFilter;
static LPALFILTERI alFilteri;
static LPALFILTERF alFilterf;

void I_OAL_InitEqualizer(void)
{
    ALCint iSends = 0;

    if (!oal || !oal->EXT_EFX)
    {
        return;
    }

    // Check the actual number of Auxiliary Sends available on each Source.

    alcGetIntegerv(oal->device, ALC_MAX_AUXILIARY_SENDS, 1, &iSends);

    if (iSends < 1)
    {
        return;
    }

    ALFUNC(LPALGENAUXILIARYEFFECTSLOTS, alGenAuxiliaryEffectSlots);
    ALFUNC(LPALDELETEAUXILIARYEFFECTSLOTS, alDeleteAuxiliaryEffectSlots);
    ALFUNC(LPALISAUXILIARYEFFECTSLOT, alIsAuxiliaryEffectSlot);
    ALFUNC(LPALGENEFFECTS, alGenEffects);
    ALFUNC(LPALDELETEEFFECTS, alDeleteEffects);
    ALFUNC(LPALISEFFECT, alIsEffect);
    ALFUNC(LPALEFFECTI, alEffecti);
    ALFUNC(LPALEFFECTF, alEffectf);
    ALFUNC(LPALAUXILIARYEFFECTSLOTI, alAuxiliaryEffectSloti);
    ALFUNC(LPALGENFILTERS, alGenFilters);
    ALFUNC(LPALDELETEFILTERS, alDeleteFilters);
    ALFUNC(LPALISFILTER, alIsFilter);
    ALFUNC(LPALFILTERI, alFilteri);
    ALFUNC(LPALFILTERF, alFilterf);
}

void I_OAL_SetEqualizer(void)
{
    static ALuint uiEffectSlot = AL_INVALID;
    static ALuint uiEffect = AL_INVALID;
    static ALuint uiFilter = AL_INVALID;

    if (!alGenAuxiliaryEffectSlots ||
        !alDeleteAuxiliaryEffectSlots ||
        !alIsAuxiliaryEffectSlot ||
        !alGenEffects ||
        !alDeleteEffects ||
        !alIsEffect ||
        !alEffecti ||
        !alEffectf ||
        !alAuxiliaryEffectSloti ||
        !alGenFilters ||
        !alDeleteFilters ||
        !alIsFilter ||
        !alFilteri ||
        !alFilterf)
    {
        return;
    }

    // Unload all effects first.

    if (alIsAuxiliaryEffectSlot(uiEffectSlot))
    {
        for (int i = 0; i < MAX_CHANNELS; i++)
        {
            alSourcei(oal->sources[i], AL_DIRECT_FILTER, AL_FILTER_NULL);
            alSource3i(oal->sources[i], AL_AUXILIARY_SEND_FILTER,
                       AL_EFFECTSLOT_NULL, 0, AL_FILTER_NULL);
        }

        alDeleteAuxiliaryEffectSlots(1, &uiEffectSlot);
    }

    if (alIsEffect(uiEffect))
    {
        alDeleteEffects(1, &uiEffect);
    }

    if (alIsFilter(uiFilter))
    {
        alDeleteFilters(1, &uiFilter);
    }

    if (snd_equalizer == EQ_PRESET_OFF)
    {
        return;
    }

    // Apply equalizer effect.

    alGenAuxiliaryEffectSlots(1, &uiEffectSlot);
    alGenEffects(1, &uiEffect);
    alGenFilters(1, &uiFilter);
    alEffecti(uiEffect, AL_EFFECT_TYPE, AL_EFFECT_EQUALIZER);

    // AL_LOWPASS_GAIN actually controls overall gain for the filter it's
    // applied to (OpenAL Effects Extension Guide 1.1, pp. 16-17, 135).

    alFilteri(uiFilter, AL_FILTER_TYPE, AL_FILTER_LOWPASS);

    // Gains vary from 0.251 up to 3.981, which means from -12dB attenuation
    // up to +12dB amplification, i.e. 20*log10(gain).

    #define DB_TO_GAIN(db) powf(10.0f, (db) / 20.0f)
    #define EQ_GAIN(db) ((ALfloat)BETWEEN(0.251f, 3.981f, DB_TO_GAIN(db)))
    #define LP_GAIN(db) ((ALfloat)BETWEEN(0.063f, 1.0f, DB_TO_GAIN(db)))
    #define OCTAVE(x) ((ALfloat)BETWEEN(0.01f, 1.0f, (x) / 100.0f))

    // Low
    alEffectf(uiEffect, AL_EQUALIZER_LOW_GAIN, EQ_GAIN(snd_eq_low_gain));
    alEffectf(uiEffect, AL_EQUALIZER_LOW_CUTOFF, (ALfloat)snd_eq_low_cutoff);

    // Mid 1
    alEffectf(uiEffect, AL_EQUALIZER_MID1_GAIN, EQ_GAIN(snd_eq_mid1_gain));
    alEffectf(uiEffect, AL_EQUALIZER_MID1_CENTER, (ALfloat)snd_eq_mid1_center);
    alEffectf(uiEffect, AL_EQUALIZER_MID1_WIDTH, OCTAVE(snd_eq_mid1_width));

    // Mid 2
    alEffectf(uiEffect, AL_EQUALIZER_MID2_GAIN, EQ_GAIN(snd_eq_mid2_gain));
    alEffectf(uiEffect, AL_EQUALIZER_MID2_CENTER, (ALfloat)snd_eq_mid2_center);
    alEffectf(uiEffect, AL_EQUALIZER_MID2_WIDTH, OCTAVE(snd_eq_mid2_width));

    // High
    alEffectf(uiEffect, AL_EQUALIZER_HIGH_GAIN, EQ_GAIN(snd_eq_high_gain));
    alEffectf(uiEffect, AL_EQUALIZER_HIGH_CUTOFF, (ALfloat)snd_eq_high_cutoff);

    alAuxiliaryEffectSloti(uiEffectSlot, AL_EFFECTSLOT_EFFECT, uiEffect);

    for (int i = 0; i < MAX_CHANNELS; i++)
    {
        // Mute the dry path.
        alFilterf(uiFilter, AL_LOWPASS_GAIN, 0.0f);
        alSourcei(oal->sources[i], AL_DIRECT_FILTER, uiFilter);

        // Keep the wet path.
        alFilterf(uiFilter, AL_LOWPASS_GAIN, LP_GAIN(snd_eq_preamp));
        alSource3i(oal->sources[i], AL_AUXILIARY_SEND_FILTER, uiEffectSlot, 0,
                   uiFilter);
    }
}

void I_OAL_EqualizerPreset(void)
{
    struct
    {
        int *var;
        int val[NUM_EQ_PRESETS];
    } eq_presets[] =
    {   // Preamp               Off, Classical, Rock, Vocal
        {&snd_eq_preamp,      {    0,    -4,    -5,    -4}}, // -24 to 0

        // Low
        {&snd_eq_low_gain,    {    0,     4,     0,    -2}}, // -12 to 12
        {&snd_eq_low_cutoff,  {  200,   125,   200,   125}}, // 50 to 800

        // Mid 1
        {&snd_eq_mid1_gain,   {    0,     1,     3,     3}}, // -12 to 12
        {&snd_eq_mid1_center, {  500,   200,   250,   650}}, // 200 to 3000
        {&snd_eq_mid1_width,  {  100,   100,   100,   100}}, // 1 to 100

        // Mid 2
        {&snd_eq_mid2_gain,   {    0,     0,     1,     3}}, // -12 to 12
        {&snd_eq_mid2_center, { 3000,  3000,  3000,  1550}}, // 1000 to 8000
        {&snd_eq_mid2_width,  {  100,   100,   100,   100}}, // 1 to 100

        // High
        {&snd_eq_high_gain,   {    0,     2,     5,     1}}, // -12 to 12
        {&snd_eq_high_cutoff, { 6000,  8000,  6000, 10000}}, // 4000 to 16000
    };

    for (int i = 0; i < arrlen(eq_presets); i++)
    {
        *eq_presets[i].var = eq_presets[i].val[snd_equalizer];
    }

    I_OAL_SetEqualizer();
}

