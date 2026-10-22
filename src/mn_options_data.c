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
#include "d_main.h"
#include "doomtype.h"
#include "i_oalsound.h"
#include "mn_container.h"
#include "mn_options.h"
#include "r_data.h"
#include "r_draw.h"
#include "r_plane.h"
#include "r_sky.h"

#define MN_RESET(lbl) {.label = (lbl), .type = MN_ITEM_RESET}
#define MN_END        {.type = MN_ITEM_END}
#define MN_GAP(gap)   {.type = MN_ITEM_SKIP, .height = (gap)}
#define MN_TITLE(txt) {.label = (txt), .type = MN_ITEM_TITLE}
#define MN_ACTION(lbl, act) \
    {.label = (lbl), .type = MN_ITEM_ACTION, .action = (act)}

#define MN_SUBMENU(lbl, menu) \
    {.label = (lbl), .type = MN_ITEM_SUBMENU, .submenu = (menu)}
#define MN_SUBMENU_STUB(lbl) \
    {.label = (lbl), .type = MN_ITEM_SUBMENU, .flags = MN_ITEM_DISABLED}

#define MN_TOGGLE(lbl, key) \
    {.label = (lbl), .cfg = (key), .type = MN_ITEM_TOGGLE}
#define MN_TOGGLE_ACT(lbl, key, act) \
    {.label = (lbl), .cfg = (key), .type = MN_ITEM_TOGGLE, .action = (act)}

#define MN_NUMBER(lbl, key, sfx) \
    {.label = (lbl), .cfg = (key), .type = MN_ITEM_NUMBER, .suffix = (sfx)}
#define MN_NUMBER_ACT(lbl, key, sfx, act) \
    {.label = (lbl),                      \
     .cfg = (key),                        \
     .type = MN_ITEM_NUMBER,              \
     .suffix = (sfx),                     \
     .action = (act)}

#define MN_CHOICE(lbl, key, strs, width) \
    {.label = (lbl),                     \
     .cfg = (key),                       \
     .type = MN_ITEM_CHOICE,             \
     .choices = (strs),                  \
     .num_choices = arrlen(strs),        \
     .choice_width = (width)}
#define MN_CHOICE_ACT(lbl, key, strs, width, act) \
    {.label = (lbl),                              \
     .cfg = (key),                                \
     .type = MN_ITEM_CHOICE,                      \
     .choices = (strs),                           \
     .num_choices = arrlen(strs),                 \
     .choice_width = (width),                     \
     .action = (act)}
#define MN_CHOICE_ACT_DEFER(lbl, key, strs, width, act) \
    {.label = (lbl),                                    \
     .cfg = (key),                                      \
     .type = MN_ITEM_CHOICE,                            \
     .choices = (strs),                                 \
     .num_choices = arrlen(strs),                       \
     .choice_width = (width),                           \
     .action = (act),                                   \
     .flags = MN_ITEM_DEFER_ACTION}
#define MN_CHOICE_ACT_DYN_DEFER(lbl, key, getstrs, width, act) \
    {.label = (lbl),                                           \
     .cfg = (key),                                             \
     .type = MN_ITEM_CHOICE,                                   \
     .getchoices = (getstrs),                                  \
     .choice_width = (width),                                  \
     .action = (act),                                          \
     .flags = MN_ITEM_DEFER_ACTION}

#define MN_SLIDER(lbl, key, size) \
    {.label = (lbl),              \
     .cfg = (key),                \
     .type = MN_ITEM_SLIDER,      \
     .slider_size = (size)}
#define MN_SLIDER_ACT(lbl, key, size, act) \
    {.label = (lbl),                       \
     .cfg = (key),                         \
     .type = MN_ITEM_SLIDER,               \
     .slider_size = (size),                \
     .action = (act)}
#define MN_SLIDER_ACT_DEFER(lbl, key, size, act) \
    {.label = (lbl),                             \
     .cfg = (key),                               \
     .type = MN_ITEM_SLIDER,                     \
     .slider_size = (size),                      \
     .action = (act),                            \
     .flags = MN_ITEM_DEFER_ACTION}
#define MN_SLIDER_ACT_STR(lbl, key, strs, size, act) \
    {.label = (lbl),                                 \
     .cfg = (key),                                   \
     .type = MN_ITEM_SLIDER,                         \
     .choices = (strs),                              \
     .num_choices = arrlen(strs),                    \
     .slider_size = (size),                          \
     .action = (act)}
#define MN_SLIDER_ACT_DYN(lbl, key, getstrs, size, act) \
    {.label = (lbl),                                    \
     .cfg = (key),                                      \
     .type = MN_ITEM_SLIDER,                            \
     .choices = NULL,                                   \
     .getchoices = (getstrs),                           \
     .slider_size = (size),                             \
     .action = (act)}
#define MN_SLIDER_ACT_DYN_DEFER(lbl, key, getstrs, size, act) \
    {.label = (lbl),                                          \
     .cfg = (key),                                            \
     .type = MN_ITEM_SLIDER,                                  \
     .choices = NULL,                                         \
     .getchoices = (getstrs),                                 \
     .slider_size = (size),                                   \
     .action = (act),                                         \
     .flags = MN_ITEM_DEFER_ACTION}

static const char *sound_module_strings[] = {
    "Standard", "OpenAL 3D",
#if defined(HAVE_AL_BUFFER_CALLBACK)
    "PC Speaker"
#endif
};

static const char *window_mode_strings[] = {"Windowed", "Borderless"};

static const char *widescreen_strings[] = {"Off", "Auto", "16:10", "16:9",
                                           "21:9", "32:9"};

static const char *pixel_scaling_strings[] = {"Sharp", "Smooth"};

const char *gamma_strings[] = {
    // Darker
    "-4", "-3.6", "-3.2", "-2.8", "-2.4", "-2.0", "-1.6", "-1.2", "-0.8",

    // No gamma correction
    "0",

    // Lighter
    "0.5", "1", "1.5", "2", "2.5", "3", "3.5", "4"
};

static mn_item_t display_items[] = {
    MN_CHOICE_ACT("Window Mode", "fullscreen", window_mode_strings, 10,
                  MN_OptionsUpdateFullscreen),
    MN_TOGGLE_ACT("Vertical Sync", "use_vsync", MN_OptionsUpdateVSync),
    MN_END
};

mn_page_t display_pages[] = {
    {.items = display_items}
};

mn_menu_t display_menu = {
    .title_patch = "",
    .title_text = "Display",
    .background = "FLOOR4_6",
    .flags = MN_MENU_HINTS,
    .item_spacing = 1,
    .pages = display_pages,
    .num_pages = arrlen(display_pages)
};

static mn_item_t video_general_items[] = {
    MN_SUBMENU("Display Options", &display_menu),
    MN_SLIDER_ACT_DYN_DEFER("Resolution Scale", "resolution_scale",
                            MN_GetResolutionScaleStrings, 11,
                            MN_OptionsUpdateResolutionScale),
    MN_TOGGLE_ACT("  Dynamic Resolution", "dynamic_resolution",
                  MN_OptionsResetVideo),
    MN_CHOICE_ACT("Pixel Scaling", "smooth_scaling", pixel_scaling_strings, 6,
                  MN_OptionsResetVideo),
    MN_CHOICE_ACT_DEFER("Aspect Ratio", "widescreen", widescreen_strings, 0,
                        MN_OptionsResetVideo),
    MN_TOGGLE_ACT("Uncapped FPS", "uncapped", MN_OptionsUpdateFPSLimit),
    MN_NUMBER_ACT("Target FPS", "fpslimit", NULL, MN_OptionsUpdateFPSLimit),
    MN_GAP(12),
    MN_TITLE("Preferences"),
    MN_SLIDER_ACT("Field of View", "fov", 11, MN_OptionsUpdateFOV),
    MN_SLIDER_ACT_STR("Gamma Correction", "gamma2", gamma_strings, 8,
                      MN_OptionsResetPalette),
    MN_SLIDER("Extra Lighting", "extra_level_brightness", 8),
    MN_RESET("Reset to Defaults"),
    MN_END
};

static const char *fuzzmode_strings[] = {
    "Blocky", "Refraction", "Shadow", "Original"
};

static const char *screen_wipe_strings[] = {"Off", "Melt", "Crossfade", "Fizzle"};

static const char *invul_mode_strings[] = {"Vanilla", "MBF", "Gray"};

// [Nugget] Sky projection
static const char *sky_projection_strings[] = {"Vanilla", "Linear", "Cylindrical"};

static const char *palette_changes_strings[] = {"Off", "On", "Reduced"};

static mn_item_t video_graphics_items[] = {
    MN_TOGGLE("Voxels", "voxels_rendering"),
    MN_GAP(6),
    MN_TITLE("Sprites"),
    MN_TOGGLE("Translucency", "translucency"),
    MN_TOGGLE("Mirrored Corpses", "flipcorpses"),
    MN_TOGGLE_ACT("Colored Blood", "colored_blood", D_SetBloodColor),
    MN_GAP(6),
    MN_TITLE("Effects"),
    MN_CHOICE("Screen Wipe", "screen_melt", screen_wipe_strings, 8),
    MN_CHOICE_ACT("Partial Invisibility", "fuzzmode", fuzzmode_strings, 8,
        R_SetFuzzColumnMode),
    MN_CHOICE_ACT("Invulnerability", "invul_mode", invul_mode_strings, 8,
        R_InvulMode),
    MN_CHOICE("Screen Flashes", "palette_changes", palette_changes_strings, 8),
    MN_TOGGLE("Swirling Flats", "r_swirl"),
    MN_GAP(6),
    MN_TITLE("Skies"),
    MN_TOGGLE_ACT("Vertical Stretch", "stretchsky", R_UpdateStretchSkies),
    MN_CHOICE_ACT("Projection", "sky_projection", sky_projection_strings, 8,
                  R_InitPlanes),
    MN_RESET("Reset to Deafults"),
    MN_END
};


static const char *extra_music_strings[] = {"Off", "Remix", "Original"};

static const char *equalizer_preset_strings[] = {"Off", "Classical", "Rock",
                                                 "Vocal", "Custom"};

static mn_item_t audio_general_items[] = {
    MN_SLIDER_ACT("Sound Volume", "sfx_volume", 8, MN_OptionsUpdateSfxVolume),
    MN_SLIDER_ACT("Music Volume", "music_volume", 8, MN_OptionsUpdateMusicVolume),
    MN_GAP(6),
    MN_TITLE("Output"),
    MN_CHOICE_ACT("Sound Mode", "snd_module", sound_module_strings, 8,
                  MN_OptionsSetSoundModule),
    MN_TOGGLE_ACT("  Headphones Mode", "snd_hrtf", MN_OptionsSetSoundModule),
    // !!! TODO should be on/off
    // MN_TOGGLE_ACT("  Doppler Effect", "snd_doppler", MN_OptionsSetSoundModule),
    MN_CHOICE_ACT("Extra Soundtrack", "extra_music", extra_music_strings, 6,
                  MN_OptionsRestartMusic),
    MN_CHOICE_ACT_DYN_DEFER("MIDI Player", "midi_player_menu",
                            MN_GetMidiPlayerStrings, 22,
                            MN_OptionsSetMidiPlayer),
    MN_RESET("Reset to Defaults"),
    MN_END
};

static mn_item_t equalizer_items[] = {
    MN_CHOICE_ACT("Preset", "snd_equalizer", equalizer_preset_strings, 5,
                  MN_OptionsEqualizerPreset),
    MN_SLIDER_ACT("  Preamp", "snd_eq_preamp", 8, MN_OptionsEqualizerPreset),
    MN_SLIDER_ACT("  Low Gain", "snd_eq_low_gain", 8, MN_OptionsEqualizerPreset),
    MN_SLIDER_ACT("  Mid 1 Gain", "snd_eq_mid1_gain", 8, MN_OptionsEqualizerPreset),
    MN_SLIDER_ACT("  Mid 2 Gain", "snd_eq_mid2_gain", 8, MN_OptionsEqualizerPreset),
    MN_SLIDER_ACT("  High Gain", "snd_eq_high_gain", 8, MN_OptionsEqualizerPreset),
    MN_NUMBER_ACT("  Low Cutoff", "snd_eq_low_cutoff", "Hz", MN_OptionsEqualizerPreset),
    MN_NUMBER_ACT("  Mid 1 Center", "snd_eq_mid1_center", "Hz", MN_OptionsEqualizerPreset),
    MN_NUMBER_ACT("  Mid 2 Center", "snd_eq_mid2_center", "Hz", MN_OptionsEqualizerPreset),
    MN_NUMBER_ACT("  High Cutoff", "snd_eq_high_cutoff", "Hz", MN_OptionsEqualizerPreset),
    MN_RESET("Reset to Defaults"),
    MN_END
};

mn_page_t equalizer_pages[] = {
    {.items = equalizer_items}
};

mn_menu_t equalizer_menu = {
    .title_patch = "",
    .title_text = "Equalizer",
    .background = "FLOOR4_6",
    .flags = MN_MENU_HINTS,
    .item_spacing = 1,
    .pages = equalizer_pages,
    .num_pages = arrlen(equalizer_pages)
};

static mn_item_t audio_advanced_items[] = {
    MN_TITLE("Sound"),
    MN_TOGGLE("Pitch-Shifting", "pitched_sounds"),
    MN_TOGGLE("Disable Cutoffs", "full_sounds"),
    MN_SLIDER("SFX Channels", "snd_channels", 8),
    MN_TOGGLE_ACT("Output Limiter", "snd_limiter", MN_OptionsSetSoundModule),
    MN_CHOICE_ACT_DYN_DEFER("Resampler", "snd_resampler",
                            MN_GetResamplerStrings, 18,
                            I_OAL_SetResampler),
    MN_SUBMENU("Equalizer", &equalizer_menu),
    MN_GAP(6),
    MN_TITLE("Music"),
    MN_TOGGLE_ACT("Auto Gain", "auto_gain", MN_OptionsRestartMusic),
    MN_SLIDER_ACT("  FluidSynth Gain", "fl_gain", 8, MN_OptionsUpdateMusicVolume),
    MN_SLIDER_ACT("  OPL3 Gain", "opl_gain", 8, MN_OptionsUpdateMusicVolume),
    MN_RESET("Reset to Defaults"),
    MN_END
};

// static const char *free_look_strings[] = {"Off", "Auto-Aim", "Direct Aim"};

static mn_page_t video_pages[] = {
    {.tab = "General",  .items = video_general_items  },
    {.tab = "Graphics", .items = video_graphics_items }
};

mn_menu_t video_menu = {
    .title_patch = "",
    .title_text = "Video",
    .background = "FLOOR4_6",
    .flags = MN_MENU_HINTS,
    .item_spacing = 1,
    .pages = video_pages,
    .num_pages = arrlen(video_pages),
};

static mn_page_t audio_pages[] = {
    {.tab = "General",  .items = audio_general_items  },
    {.tab = "Advanced", .items = audio_advanced_items }
};

mn_menu_t audio_menu = {
    .title_patch = "",
    .title_text = "Audio",
    .background = "FLOOR4_6",
    .flags = MN_MENU_HINTS,
    .item_spacing = 1,
    .pages = audio_pages,
    .num_pages = arrlen(audio_pages),
};

static mn_item_t options_root_items[] = {
    MN_SUBMENU("Video", &video_menu),
    MN_SUBMENU("Audio", &audio_menu),
    MN_SUBMENU_STUB("Controls"),
    MN_SUBMENU_STUB("Gameplay"),
    MN_END
};

static mn_page_t options_root_pages[] = {
    {.items = options_root_items},
};

mn_menu_t options_root_menu = {
    .title_patch = "",
    .title_text = "",
    .background = "FLOOR4_6",
    .flags = MN_MENU_CENTER | MN_MENU_HIDE_VALUES,
    .item_spacing = 6,
    .pages = options_root_pages,
    .num_pages = arrlen(options_root_pages),
};
