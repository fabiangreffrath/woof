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

#include "i_oalcommon.h"
#include "i_oalequalizer.h"
#include "i_sound.h"
#include "m_config.h"
#include "mn_menu.h"

#define EQF(T, ptr) ((ALFUNC(T, ptr)) != NULL)

typedef enum
{
    EQ_PRESET_OFF,
    EQ_PRESET_CLASSICAL,
    EQ_PRESET_ROCK,
    EQ_PRESET_VOCAL,
    EQ_PRESET_CUSTOM,
    NUM_EQ_PRESETS
} eq_preset_t;

static eq_preset_t snd_equalizer, default_equalizer;
static int snd_eq_preamp,      default_preamp;
static int snd_eq_low_gain,    default_low_gain;
static int snd_eq_low_cutoff,  default_low_cutoff;
static int snd_eq_mid1_gain,   default_mid1_gain;
static int snd_eq_mid1_center, default_mid1_center;
static int snd_eq_mid1_width,  default_mid1_width;
static int snd_eq_mid2_gain,   default_mid2_gain;
static int snd_eq_mid2_center, default_mid2_center;
static int snd_eq_mid2_width,  default_mid2_width;
static int snd_eq_high_gain,   default_high_gain;
static int snd_eq_high_cutoff, default_high_cutoff;

static boolean initialized;

static ALuint uiEffectSlot;
static ALuint uiEffect;
static ALuint uiFilter;

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

static void BackupCustomPreset(void)
{
    snd_eq_preamp      = default_preamp;
    snd_eq_low_gain    = default_low_gain;
    snd_eq_low_cutoff  = default_low_cutoff;
    snd_eq_mid1_gain   = default_mid1_gain;
    snd_eq_mid1_center = default_mid1_center;
    snd_eq_mid1_width  = default_mid1_width;
    snd_eq_mid2_gain   = default_mid2_gain;
    snd_eq_mid2_center = default_mid2_center;
    snd_eq_mid2_width  = default_mid2_width;
    snd_eq_high_gain   = default_high_gain;
    snd_eq_high_cutoff = default_high_cutoff;
}

static void RestoreCustomPreset(void)
{
    default_preamp      = snd_eq_preamp;
    default_low_gain    = snd_eq_low_gain;
    default_low_cutoff  = snd_eq_low_cutoff;
    default_mid1_gain   = snd_eq_mid1_gain;
    default_mid1_center = snd_eq_mid1_center;
    default_mid1_width  = snd_eq_mid1_width;
    default_mid2_gain   = snd_eq_mid2_gain;
    default_mid2_center = snd_eq_mid2_center;
    default_mid2_width  = snd_eq_mid2_width;
    default_high_gain   = snd_eq_high_gain;
    default_high_cutoff = snd_eq_high_cutoff;
}

static void StopEffects(void)
{
    for (int i = 0; i < MAX_CHANNELS; i++)
    {
        alSourceStop(oal->sources[i]);
    }
}

static void UnloadEffects(void)
{
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
}

void I_OAL_ShutdownEqualizer(void)
{
    RestoreCustomPreset();

    if (initialized)
    {
        UnloadEffects();
        initialized = false;
    }
}

boolean I_OAL_EqualizerInitialized(void)
{
    return initialized;
}

boolean I_OAL_CustomEqualizer(void)
{
    return (initialized && default_equalizer == EQ_PRESET_CUSTOM);
}

void I_OAL_InitEqualizer(void)
{
    BackupCustomPreset();
    snd_equalizer = default_equalizer;

    if (!oal || !oal->EXT_EFX || !oal->device || !oal->sources)
    {
        return;
    }

    // Check the actual number of Auxiliary Sends available on each Source.

    ALCint iSends = 0;
    alcGetIntegerv(oal->device, ALC_MAX_AUXILIARY_SENDS, 1, &iSends);

    if (iSends < 1)
    {
        return;
    }

    initialized = (
        EQF(LPALGENAUXILIARYEFFECTSLOTS, alGenAuxiliaryEffectSlots)
        && EQF(LPALDELETEAUXILIARYEFFECTSLOTS, alDeleteAuxiliaryEffectSlots)
        && EQF(LPALISAUXILIARYEFFECTSLOT, alIsAuxiliaryEffectSlot)
        && EQF(LPALGENEFFECTS, alGenEffects)
        && EQF(LPALDELETEEFFECTS, alDeleteEffects)
        && EQF(LPALISEFFECT, alIsEffect)
        && EQF(LPALEFFECTI, alEffecti)
        && EQF(LPALEFFECTF, alEffectf)
        && EQF(LPALAUXILIARYEFFECTSLOTI, alAuxiliaryEffectSloti)
        && EQF(LPALGENFILTERS, alGenFilters)
        && EQF(LPALDELETEFILTERS, alDeleteFilters)
        && EQF(LPALISFILTER, alIsFilter)
        && EQF(LPALFILTERI, alFilteri)
        && EQF(LPALFILTERF, alFilterf)
    );

    if (initialized)
    {
        I_OAL_EqualizerPreset();
    }
}

void I_OAL_SetEqualizer(void)
{
    if (!initialized)
    {
        return;
    }

    // Unload all effects first.

    StopEffects();
    UnloadEffects();

    if (default_equalizer == EQ_PRESET_OFF)
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

    #define EQ_GAIN(db) ((ALfloat)clampf(DB_TO_GAIN(db), 0.251f, 3.981f))
    #define LP_GAIN(db) ((ALfloat)clampf(DB_TO_GAIN(db), 0.063f, 1.0f))
    #define OCTAVE(x) ((ALfloat)clampf((x) / 100.0f, 0.01f, 1.0f))

    // Low
    alEffectf(uiEffect, AL_EQUALIZER_LOW_GAIN, EQ_GAIN(default_low_gain));
    alEffectf(uiEffect, AL_EQUALIZER_LOW_CUTOFF, (ALfloat)default_low_cutoff);

    // Mid 1
    alEffectf(uiEffect, AL_EQUALIZER_MID1_GAIN, EQ_GAIN(default_mid1_gain));
    alEffectf(uiEffect, AL_EQUALIZER_MID1_CENTER, (ALfloat)default_mid1_center);
    alEffectf(uiEffect, AL_EQUALIZER_MID1_WIDTH, OCTAVE(default_mid1_width));

    // Mid 2
    alEffectf(uiEffect, AL_EQUALIZER_MID2_GAIN, EQ_GAIN(default_mid2_gain));
    alEffectf(uiEffect, AL_EQUALIZER_MID2_CENTER, (ALfloat)default_mid2_center);
    alEffectf(uiEffect, AL_EQUALIZER_MID2_WIDTH, OCTAVE(default_mid2_width));

    // High
    alEffectf(uiEffect, AL_EQUALIZER_HIGH_GAIN, EQ_GAIN(default_high_gain));
    alEffectf(uiEffect, AL_EQUALIZER_HIGH_CUTOFF, (ALfloat)default_high_cutoff);

    alAuxiliaryEffectSloti(uiEffectSlot, AL_EFFECTSLOT_EFFECT, uiEffect);

    for (int i = 0; i < MAX_CHANNELS; i++)
    {
        // Mute the dry path.
        alFilterf(uiFilter, AL_LOWPASS_GAIN, 0.0f);
        alSourcei(oal->sources[i], AL_DIRECT_FILTER, uiFilter);

        // Keep the wet path.
        alFilterf(uiFilter, AL_LOWPASS_GAIN, LP_GAIN(default_preamp));
        alSource3i(oal->sources[i], AL_AUXILIARY_SEND_FILTER, uiEffectSlot, 0,
                   uiFilter);
    }
}

void I_OAL_EqualizerPreset(void)
{
    if (!initialized)
    {
        return;
    }

    struct
    {
        int *var;
        int val[NUM_EQ_PRESETS - 1];
    } eq_presets[] =
    {   // Preamp               Off, Classical, Rock, Vocal
        {&default_preamp,      {    0,    -4,    -5,    -4}}, // -24 to 0

        // Low
        {&default_low_gain,    {    0,     4,     0,    -2}}, // -12 to 12
        {&default_low_cutoff,  {  200,   125,   200,   125}}, // 50 to 800

        // Mid 1
        {&default_mid1_gain,   {    0,     1,     3,     3}}, // -12 to 12
        {&default_mid1_center, {  500,   200,   250,   650}}, // 200 to 3000
        {&default_mid1_width,  {  100,   100,   100,   100}}, // 1 to 100

        // Mid 2
        {&default_mid2_gain,   {    0,     0,     1,     3}}, // -12 to 12
        {&default_mid2_center, { 3000,  3000,  3000,  1550}}, // 1000 to 8000
        {&default_mid2_width,  {  100,   100,   100,   100}}, // 1 to 100

        // High
        {&default_high_gain,   {    0,     2,     5,     1}}, // -12 to 12
        {&default_high_cutoff, { 6000,  8000,  6000, 10000}}, // 4000 to 16000
    };

    if (default_equalizer == EQ_PRESET_CUSTOM
        && snd_equalizer != EQ_PRESET_CUSTOM)
    {
        RestoreCustomPreset();
    }
    else if (snd_equalizer == EQ_PRESET_CUSTOM)
    {
        BackupCustomPreset();
    }

    if (default_equalizer < NUM_EQ_PRESETS - 1)
    {
        for (int i = 0; i < arrlen(eq_presets); i++)
        {
            *eq_presets[i].var = eq_presets[i].val[default_equalizer];
        }
    }

    snd_equalizer = default_equalizer;
    I_OAL_SetEqualizer();
    MN_UpdateEqualizerItems();
}

#define BIND_NUM_EQ(name, default_name, v, a, b, help) \
    M_BindNum(#name, &default_name, &name, (v), (a), (b), ss_eq, wad_no, help)

void I_BindEqualizerVariables(void)
{
    BIND_NUM_EQ(snd_equalizer, default_equalizer,
        EQ_PRESET_OFF, EQ_PRESET_OFF, EQ_PRESET_CUSTOM,
        "Equalizer preset (0 = Off; 1 = Classical; 2 = Rock; 3 = Vocal; "
        "4 = Custom)");

    // Preamp
    BIND_NUM_EQ(snd_eq_preamp, default_preamp, 0, -24, 0,
        "Equalizer preamp gain [dB]");

    // Low
    BIND_NUM_EQ(snd_eq_low_gain, default_low_gain, 0, -12, 12,
        "Equalizer low frequency range gain [dB]");
    BIND_NUM_EQ(snd_eq_low_cutoff, default_low_cutoff, 200, 50, 800,
        "Equalizer low cut-off frequency [Hz]");

    // Mid 1
    BIND_NUM_EQ(snd_eq_mid1_gain, default_mid1_gain, 0, -12, 12,
        "Equalizer mid1 frequency range gain [dB]");
    BIND_NUM_EQ(snd_eq_mid1_center, default_mid1_center, 500, 200, 3000,
        "Equalizer mid1 center frequency [Hz]");
    BIND_NUM_EQ(snd_eq_mid1_width, default_mid1_width, 100, 1, 100,
        "Equalizer mid1 bandwidth [octave] (1 = 0.01; 100 = 1.0)");

    // Mid 2
    BIND_NUM_EQ(snd_eq_mid2_gain, default_mid2_gain, 0, -12, 12,
        "Equalizer mid2 frequency range gain [dB]");
    BIND_NUM_EQ(snd_eq_mid2_center, default_mid2_center, 3000, 1000, 8000,
        "Equalizer mid2 center frequency [Hz]");
    BIND_NUM_EQ(snd_eq_mid2_width, default_mid2_width, 100, 1, 100,
        "Equalizer mid2 bandwidth [octave] (1 = 0.01; 100 = 1.0)");

    // High
    BIND_NUM_EQ(snd_eq_high_gain, default_high_gain, 0, -12, 12,
        "Equalizer high frequency range gain [dB]");
    BIND_NUM_EQ(snd_eq_high_cutoff, default_high_cutoff, 6000, 4000, 16000,
        "Equalizer high cut-off frequency [Hz]");
}
