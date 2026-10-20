//
//  Copyright (C) 2026 Roman Fomin
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
// DESCRIPTION:
//  Declarative tables for the new options menu.
//

#include "config.h"
#include "mn_container.h"
#include "mn_menu.h"
#include "mn_options.h"

#define MN_RESET(lbl) {.label = (lbl), .type = MN_ITEM_RESET}
#define MN_END        {.type = MN_ITEM_END}

#define MN_TITLE(txt) {.label = (txt), .type = MN_ITEM_TITLE}
#define MN_TOGGLE(lbl, key) \
    {.label = (lbl), .cfg = (key), .type = MN_ITEM_TOGGLE}
#define MN_NUMBER(lbl, key, suffix) \
    {.label = (lbl), .cfg = (key), .type = MN_ITEM_NUMBER, .suffix = (suffix)}
#define MN_CHOICE(lbl, key, strs) \
    {.label = (lbl), .cfg = (key), .type = MN_ITEM_CHOICE, .choices = (strs)}
#define MN_CHOICE_W(lbl, key, strs, w) \
    {.label = (lbl), .cfg = (key), .type = MN_ITEM_CHOICE, .choices = (strs), .choice_width = (w)}
#define MN_ACTION(lbl, act) \
    {.label = (lbl), .type = MN_ITEM_ACTION, .action = (act)}

#define MN_SLIDER(lbl, key, strs, size)    \
    {.label = (lbl),                       \
     .cfg = (key),                         \
     .type = MN_ITEM_SLIDER,               \
     .choices = (strs),                    \
     .slider_size = (size)}
#define MN_TOGGLE_ACT(lbl, key, act)       \
    {.label = (lbl),                       \
     .cfg = (key),                         \
     .type = MN_ITEM_TOGGLE,               \
     .action = (act)}
#define MN_NUMBER_ACT(lbl, key, sfx, act)  \
    {.label = (lbl),                       \
     .cfg = (key),                         \
     .type = MN_ITEM_NUMBER,               \
     .suffix = (sfx),                      \
     .action = (act)}
#define MN_CHOICE_ACT(lbl, key, strs, act) \
    {.label = (lbl),                       \
     .cfg = (key),                         \
     .type = MN_ITEM_CHOICE,               \
     .choices = (strs),                    \
     .action = (act)}
#define MN_CHOICE_ACT_W(lbl, key, strs, w, act) \
    {.label = (lbl),                            \
     .cfg = (key),                              \
     .type = MN_ITEM_CHOICE,                    \
     .choices = (strs),                         \
     .choice_width = (w),                       \
     .action = (act)}
#define MN_SLIDER_ACT(lbl, key, strs, size, act) \
    {.label = (lbl),                             \
     .cfg = (key),                               \
     .type = MN_ITEM_SLIDER,                     \
     .choices = (strs),                          \
     .slider_size = (size),                      \
     .action = (act)}

static const char *sound_module_strings[] = {
    "Standard", "OpenAL 3D",
#if defined(HAVE_AL_BUFFER_CALLBACK)
    "PC Speaker"
#endif
};

static const char *extra_music_strings[] = {"Off", "Remix", "Original"};

static const char *midi_reset_strings[] = {"No SysEx", "GM", "GS", "XG"};

static const char *equalizer_preset_strings[] = {"Off", "Classical", "Rock",
                                                 "Vocal", "Custom"};

static const char *percent_strings[] = {"Off", "10%", "20%", "30%",
                                        "40%", "50%", "60%", "70%",
                                        "80%", "90%", "100%"};

static mn_item_t general_items[] = {
    MN_TITLE("Video"),
    MN_SLIDER_ACT("Resolution Scale", "resolution_scale", NULL, 11,
                  MN_OptionsResetVideo),
    MN_TOGGLE_ACT("Dynamic Resolution", "dynamic_resolution",
                  MN_OptionsResetVideo),
    MN_CHOICE_ACT("Widescreen", "widescreen", widescreen_strings,
                  MN_OptionsResetVideo),
    MN_TOGGLE_ACT("Fullscreen", "fullscreen", MN_OptionsUpdateFullscreen),

    MN_TITLE("Performance"),
    MN_TOGGLE_ACT("Uncapped FPS", "uncapped", MN_OptionsUpdateFPSLimit),
    MN_NUMBER_ACT("Target FPS", "fpslimit", NULL, MN_OptionsUpdateFPSLimit),
    MN_TOGGLE_ACT("VSync", "use_vsync", MN_OptionsUpdateVSync),

    MN_TITLE("Display"),
    MN_SLIDER_ACT("FOV", "fov", NULL, 11, MN_OptionsUpdateFOV),
    MN_SLIDER_ACT("Gamma Correction", "gamma2", gamma_strings, 8,
                  MN_OptionsResetPalette),
    MN_SLIDER("Extra Lighting", "extra_level_brightness", NULL, 8),

    MN_TITLE("Sound"),
    MN_SLIDER_ACT("Sound Volume", "sfx_volume", NULL, 8,
                  MN_OptionsUpdateSfxVolume),
    MN_SLIDER_ACT("Music Volume", "music_volume", NULL, 8,
                  MN_OptionsUpdateMusicVolume),
    MN_CHOICE_ACT_W("Sound Module", "snd_module", sound_module_strings, 8,
                    MN_OptionsSetSoundModule),
    MN_TOGGLE_ACT("Headphones Mode", "snd_hrtf", MN_OptionsSetSoundModule),
    MN_CHOICE_ACT_W("Extra Soundtrack", "extra_music", extra_music_strings, 6,
                    MN_OptionsRestartMusic),

    MN_TITLE("Menu"),
    MN_ACTION("Classic Options", MN_OptionsOpenClassic),
    MN_RESET("Reset to Defaults"),
    MN_END
};

static mn_item_t sfx_items[] = {
    MN_SLIDER("SFX Channels", "snd_channels", NULL, 8),
    MN_TOGGLE_ACT("Output Limiter", "snd_limiter", MN_OptionsSetSoundModule),
    MN_TOGGLE("Pitch-Shifting", "pitched_sounds"),
    MN_TOGGLE("Disable Cutoffs", "full_sounds"),
    MN_SLIDER_ACT("Doppler Effect", "snd_doppler", percent_strings, 8,
                  MN_OptionsSetSoundModule),
    MN_RESET("Reset to Defaults"),
    MN_END
};

static mn_item_t music_items[] = {
    MN_TOGGLE_ACT("Auto Gain", "auto_gain", MN_OptionsRestartMusic),
    MN_SLIDER_ACT("OPL3 Gain", "opl_gain", NULL, 8,
                  MN_OptionsUpdateMusicVolume),
    MN_SLIDER("OPL3 Number of Chips", "num_opl_chips", NULL, 4),
    MN_TOGGLE("OPL3 Reverse Stereo", "opl_stereo_correct"),
    MN_CHOICE_ACT_W("Native MIDI Reset Type", "midi_reset_type",
                    midi_reset_strings, 6, MN_OptionsRestartMusic),
    MN_RESET("Reset to Defaults"),
    MN_END
};

static mn_item_t equalizer_items[] = {
    MN_CHOICE_ACT_W("Preset", "snd_equalizer", equalizer_preset_strings, 5,
                    MN_OptionsEqualizerPreset),
    MN_SLIDER_ACT("Preamp", "snd_eq_preamp", NULL, 8,
                  MN_OptionsEqualizerPreset),
    MN_SLIDER_ACT("Low Gain", "snd_eq_low_gain", NULL, 8,
                  MN_OptionsEqualizerPreset),
    MN_SLIDER_ACT("Mid 1 Gain", "snd_eq_mid1_gain", NULL, 8,
                  MN_OptionsEqualizerPreset),
    MN_SLIDER_ACT("Mid 2 Gain", "snd_eq_mid2_gain", NULL, 8,
                  MN_OptionsEqualizerPreset),
    MN_SLIDER_ACT("High Gain", "snd_eq_high_gain", NULL, 8,
                  MN_OptionsEqualizerPreset),
    MN_NUMBER_ACT("Low Cutoff", "snd_eq_low_cutoff", "Hz",
                  MN_OptionsEqualizerPreset),
    MN_NUMBER_ACT("Mid 1 Center", "snd_eq_mid1_center", "Hz",
                  MN_OptionsEqualizerPreset),
    MN_NUMBER_ACT("Mid 2 Center", "snd_eq_mid2_center", "Hz",
                  MN_OptionsEqualizerPreset),
    MN_NUMBER_ACT("High Cutoff", "snd_eq_high_cutoff", "Hz",
                  MN_OptionsEqualizerPreset),
    MN_RESET("Reset to Defaults"),
    MN_END
};

static mn_page_t options_pages[] = {
    {.tab = "General",   .items = general_items  },
    {.tab = "Sound",     .items = sfx_items      },
    {.tab = "Music",     .items = music_items    },
    {.tab = "Equalizer", .items = equalizer_items},
};

mn_menu_t options_menu = {
    .title_patch = "M_OPTTTL",
    .title_text = "OPTIONS",
    .background = "FLOOR4_6",
    .pages = options_pages,
    .num_pages = arrlen(options_pages),
};
