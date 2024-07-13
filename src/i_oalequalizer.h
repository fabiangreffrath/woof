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

#ifndef __I_OALEQUALIZER__
#define __I_OALEQUALIZER__

typedef enum {
    EQ_PRESET_OFF,
    EQ_PRESET_CLASSICAL,
    EQ_PRESET_ROCK,
    EQ_PRESET_VOCAL,
    NUM_EQ_PRESETS
} eq_preset_t;

extern eq_preset_t snd_equalizer;
extern int snd_eq_preamp;
extern int snd_eq_low_gain;
extern int snd_eq_low_cutoff;
extern int snd_eq_mid1_gain;
extern int snd_eq_mid1_center;
extern int snd_eq_mid1_width;
extern int snd_eq_mid2_gain;
extern int snd_eq_mid2_center;
extern int snd_eq_mid2_width;
extern int snd_eq_high_gain;
extern int snd_eq_high_cutoff;

void I_OAL_InitEqualizer(void);

void I_OAL_SetEqualizer(void);

void I_OAL_EqualizerPreset(void);

#endif
