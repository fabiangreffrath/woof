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
//  Declarative tables for the options menu.
//

#include "am_map.h"
#include "config.h"
#include "d_main.h"
#include "doomtype.h"
#include "g_input.h"
#include "hu_command.h"
#include "hu_crosshair.h"
#include "i_gamepad.h"
#include "i_gyro.h"
#include "i_oalequalizer.h"
#include "i_oalsound.h"
#include "i_video.h"
#include "m_input.h"
#include "mn_container.h"
#include "mn_internal.h"
#include "mn_options.h"
#include "r_data.h"
#include "r_draw.h"
#include "r_plane.h"
#include "r_sky.h"

#define MN_RESET \
    {.label = "", .type = MN_ITEM_RESET, .flags = MN_ITEM_DIRECT_COORDS}

#define MN_SUBMENU(lbl, menu) \
    {.label = (lbl), .type = MN_ITEM_SUBMENU, .submenu = (menu)}

#define MN_MENU(lbl, menu)        \
    {.label = (lbl),              \
     .type = MN_ITEM_SUBMENU,     \
     .submenu = (menu),           \
     .flags = MN_ITEM_HIDE_VALUE}

#define MN_INPUT(lbl, id, grp) \
    {.label = (lbl),           \
     .cfg = #id,               \
     .type = MN_ITEM_INPUT,    \
     .input_id = (id),         \
     .group = (grp)}

#define MN_TOGGLE_ACT(lbl, key, act, desc) \
    {.label = (lbl),                       \
     .cfg = (key),                         \
     .type = MN_ITEM_TOGGLE,               \
     .action = (act),                      \
     .description = (desc)}

#define MN_NUMBER_ACT(lbl, key, sfx, act, desc) \
    {.label = (lbl),                            \
     .cfg = (key),                              \
     .type = MN_ITEM_NUMBER,                    \
     .suffix = (sfx),                           \
     .action = (act),                           \
     .description = (desc)}

#define MN_CHOICE_ACT_DEFER(lbl, key, strs, size, act, desc) \
    {.label = (lbl),                                         \
     .cfg = (key),                                           \
     .type = MN_ITEM_CHOICE,                                 \
     .choices = (strs),                                      \
     .num_choices = arrlen(strs),                            \
     .choice_size = (size),                                  \
     .action = (act),                                        \
     .flags = MN_ITEM_DEFER_ACTION,                          \
     .description = (desc)}

#define MN_SLIDER(lbl, key, size, desc) \
    {.label = (lbl),                    \
     .cfg = (key),                      \
     .type = MN_ITEM_SLIDER,            \
     .slider_size = (size),             \
     .description = (desc)}
#define MN_SLIDER_STR(lbl, key, strs, size, desc) \
    {.label = (lbl),                              \
     .cfg = (key),                                \
     .type = MN_ITEM_SLIDER,                      \
     .choices = (strs),                           \
     .num_choices = arrlen(strs),                 \
     .slider_size = (size),                       \
     .description = (desc)}
#define MN_SLIDER_ACT(lbl, key, size, act, desc) \
    {.label = (lbl),                             \
     .cfg = (key),                               \
     .type = MN_ITEM_SLIDER,                     \
     .slider_size = (size),                      \
     .action = (act),                            \
     .description = (desc)}
#define MN_SLIDER_ACT_SUFFIX(lbl, key, size, act, sfx, desc) \
    {.label = (lbl),                                         \
     .cfg = (key),                                           \
     .type = MN_ITEM_SLIDER,                                 \
     .slider_size = (size),                                  \
     .suffix = (sfx),                                        \
     .action = (act),                                        \
     .description = (desc)}
#define MN_SLIDER_ACT_STR(lbl, key, strs, size, act, desc) \
    {.label = (lbl),                                       \
     .cfg = (key),                                         \
     .type = MN_ITEM_SLIDER,                               \
     .choices = (strs),                                    \
     .num_choices = arrlen(strs),                          \
     .slider_size = (size),                                \
     .action = (act),                                      \
     .description = (desc)}
#define MN_SLIDER_ACT_STR_DEFER(lbl, key, strs, size, act, desc) \
    {.label = (lbl),                                             \
     .cfg = (key),                                               \
     .type = MN_ITEM_SLIDER,                                     \
     .choices = (strs),                                          \
     .num_choices = arrlen(strs),                                \
     .slider_size = (size),                                      \
     .action = (act),                                            \
     .flags = MN_ITEM_DEFER_ACTION,                              \
     .description = (desc)}
#define MN_SLIDER_ACT_DYN(lbl, key, getstrs, size, act, desc) \
    {.label = (lbl),                                          \
     .cfg = (key),                                            \
     .type = MN_ITEM_SLIDER,                                  \
     .getchoices = (getstrs),                                 \
     .slider_size = (size),                                   \
     .action = (act),                                         \
     .description = (desc)}
#define MN_SLIDER_ACT_DYN_DEFER(lbl, key, getstrs, size, act, desc) \
    {.label = (lbl),                                                \
     .cfg = (key),                                                  \
     .type = MN_ITEM_SLIDER,                                        \
     .getchoices = (getstrs),                                       \
     .slider_size = (size),                                         \
     .action = (act),                                               \
     .flags = MN_ITEM_DEFER_ACTION,                                 \
     .description = (desc)}


static const char *window_mode_strings[] = {"Windowed", "Borderless"};
static const char *widescreen_strings[] = {
    "Original", "Auto", "16:10", "16:9", "21:9", "32:9"
};
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
                  MN_OptionsUpdateFullscreen,
                  "Window Mode:\n\nWindowed: Runs in a resizable window.\n\nBorderless: Fullscreen borderless window."),
    MN_TOGGLE_ACT("Vertical Sync", "use_vsync", I_ToggleVsync,
                  "Vertical Sync:\n\nSynchronizes frame rate with monitor refresh rate to prevent tearing."),
    MN_END
};

mn_page_t display_pages[] = {
    {.items = display_items}
};

mn_menu_t display_menu = {
    .title_patch = "",
    .title_text = "Display",
    .background = "FLOOR4_6",
    .flags = MN_MENU_HINTS | MN_MENU_CENTER_LAYOUT,
    .layout = {.max_width = MN_PX(500)},
    .pages = display_pages,
    .num_pages = arrlen(display_pages)
};

static mn_item_t video_general_items[] = {
    MN_SUBMENU("Display Options", &display_menu),
    MN_SLIDER_ACT_DYN_DEFER(
        "Resolution Scale", "resolution_scale", MN_GetResolutionScaleStrings,
        11, MN_OptionsUpdateResolutionScale,
        "Resolution Scale:\n\nScales the internal rendering resolution.\nLower "
        "values improve performance."),
    MN_TOGGLE_ACT(
        "  Dynamic Resolution", "dynamic_resolution", MN_OptionsResetVideo,
        "Dynamic Resolution:\n\nAutomatically adjusts resolution "
        "scale to maintain target FPS."),
    MN_CHOICE_ACT(
        "Pixel Scaling", "smooth_scaling", pixel_scaling_strings, 8,
        MN_OptionsResetVideo,
        "Pixel Scaling:\n\nSharp: Preserves details, but may look uneven.\n\n"
        "Smooth: Even pixels, but may look softer."),
    MN_CHOICE_ACT_DEFER(
        "Aspect Ratio", "widescreen", widescreen_strings, 8, MN_OptionsResetVideo,
        "Aspect Ratio:\n\nAuto: Detects from resolution."),
    MN_TOGGLE_ACT(
        "Uncapped FPS", "uncapped", MN_OptionsUpdateFPSLimit,
        "Uncapped FPS:\n\nRemoves the original frame rate limit 35 FPS."),
    MN_NUMBER_ACT(
        "Target FPS", "fpslimit", NULL, MN_OptionsUpdateFPSLimit,
        "Target FPS:\n\nSets the maximum frame rate limit.\n\n"
        "Target frame rate for Dynamic Resolution scaling."),
    MN_GAP(12),
    MN_TITLE("Preferences"),
    MN_SLIDER_ACT(
        "Field of View", "fov", 11, MN_OptionsUpdateFOV,
        "Field of View:\n\nAdjusts the horizontal viewing angle."),
    MN_SLIDER_ACT_STR(
        "Gamma Correction", "gamma2", gamma_strings, 8, V_ResetPalette,
        "Gamma Correction:\n\nAdjusts overall brightness.\nNegative values "
        "darken, positive values brighten."),
    MN_SLIDER(
        "Extra Lighting", "extra_level_brightness", 8,
        "Extra Lighting:\n\nAdds constant light level to all sectors.\n"
        "Useful for dark maps."),
    MN_RESET,
    MN_END
};

static const char *fuzzmode_strings[] = {
    "Blocky", "Refraction", "Shadow", "Original"
};
static const char *screen_wipe_strings[] = {"Off", "Melt", "Crossfade", "Fizzle"};
static const char *invul_mode_strings[] = {"Vanilla", "MBF", "Gray"};
static const char *sky_projection_strings[] = {"Vanilla", "Linear", "Cylindrical"};
static const char *palette_changes_strings[] = {"Off", "On", "Reduced"};

static mn_item_t video_graphics_items[] = {
    MN_TOGGLE("Voxels", "voxels_rendering", NULL),
    MN_GAP(6),
    MN_TITLE("Sprites"),
    MN_TOGGLE("Translucency", "translucency", NULL),
    MN_TOGGLE("Mirrored Corpses", "flipcorpses", NULL),
    MN_TOGGLE_ACT("Colored Blood", "colored_blood", D_SetBloodColor,
                  "Colored Blood:\n\nEnables monster-specific blood colors."),
    MN_GAP(6),
    MN_TITLE("Effects"),
    MN_CHOICE("Screen Wipe", "screen_melt", screen_wipe_strings, 10, NULL),
    MN_CHOICE_ACT(
        "Partial Invisibility", "fuzzmode", fuzzmode_strings, 10,
        R_SetFuzzColumnMode,
        "Partial Invisibility:\n\nBlocky: Classic blocky effect.\n\n"
        "Refraction: Distorted glass-like effect.\n\n"
        "Shadow: Dark silhouette effect.\n\n"
        "Original: Vanilla behavior."),
    MN_CHOICE_ACT(
        "Invulnerability", "invul_mode", invul_mode_strings, 10, R_InvulMode,
        "Invulnerability:\n\nVanilla: Original inverse colors.\n\n"
        "MBF: Dosen't affect skies.\n\nGray: Grayscale effect."),
    MN_CHOICE("Screen Flashes", "palette_changes", palette_changes_strings, 10,
              NULL),
    MN_TOGGLE("Swirling Flats", "r_swirl", NULL),
    MN_GAP(6),
    MN_TITLE("Skies"),
    MN_TOGGLE_ACT(
        "Vertical Stretch", "stretchsky", R_UpdateStretchSkies,
        "Vertical Stretch:\n\nStretches sky textures to fill vertical space."),
    MN_CHOICE_ACT(
        "Projection", "sky_projection", sky_projection_strings, 10, R_InitPlanes,
        "Projection:\n\nVanilla: Original flat sky.\n\n"
        "Linear: Perspective-corrected.\n\n"
        "Cylindrical: Panoramic cylindrical mapping."),
    MN_RESET,
    MN_END
};

static const char *sound_module_strings[] = {
    "Standard", "OpenAL 3D",
#if defined(HAVE_AL_BUFFER_CALLBACK)
    "PC Speaker"
#endif
};

static const char *extra_music_strings[] = {"Off", "Remix", "Original"};
static const char *equalizer_preset_strings[] = {
    "Off", "Classical", "Rock", "Vocal", "Custom"
};
static const char *percent_strings[] = {
    "Off", "10%", "20%", "30%", "40%", "50%", "60%", "70%", "80%", "90%", "100%"
};

static mn_item_t audio_general_items[] = {
    MN_SLIDER_ACT("Sound Volume", "sfx_volume", 8, MN_OptionsUpdateSfxVolume,
                  "Sound Volume:\n\nAdjusts the volume of sound effects."),
    MN_SLIDER_ACT("Music Volume", "music_volume", 8,
                  MN_OptionsUpdateMusicVolume,
                  "Music Volume:\n\nAdjusts the volume of background music."),
    MN_GAP(6),
    MN_TITLE("Output"),
    MN_CHOICE_ACT(
        "Sound Mode", "snd_module", sound_module_strings, 10,
        MN_OptionsSetSoundModule,
        "Sound Mode:\n\nStandard: Basic stereo output.\n\n"
        "OpenAL 3D: 3D positional audio.\n\n"
        "PC Speaker: Emulates PC speaker beeps."),
    MN_TOGGLE_ACT(
        "  Headphones Mode", "snd_hrtf", MN_OptionsSetSoundModule,
        "Headphones Mode:\n\nEnables HRTF for better 3D audio on headphones."),
    // !!! TODO Should be On/Off
    MN_SLIDER_ACT_STR_DEFER(
        "  Doppler Effect", "snd_doppler", percent_strings, 6,
        MN_OptionsSetSoundModule,
        "Doppler Effect:\n\nSimulates pitch shift from moving sound sources."),
    MN_CHOICE_ACT("Extra Soundtrack", "extra_music", extra_music_strings, 8,
                  MN_OptionsRestartMusic, NULL),
    MN_GAP(20),
    {.label = "MIDI Player",
     .cfg = "midi_player_menu",
     .type = MN_ITEM_CHOICE,
     .getchoices = MN_GetMidiPlayerStrings,
     .choice_size = 18,
     .action = MN_OptionsSetMidiPlayer,
     .flags = MN_ITEM_DEFER_ACTION | MN_ITEM_TWO_LINES,
     .description = "MIDI Player:\n\nSelects the MIDI playback device."},
    MN_RESET,
    MN_END
};

static mn_item_t equalizer_items[] = {
    MN_CHOICE_ACT("Preset", "snd_equalizer",
        equalizer_preset_strings, 8, I_OAL_EqualizerPreset,
        "Preset:\n\nOff: No equalization.\n\nClassical: Enhanced dynamics.\n\n"
        "Rock: Boosted lows and highs.\n\nVocal: Enhanced midrange.\n\n"
        "Custom: Manual settings below."),
    MN_SLIDER_ACT("  Preamp", "snd_eq_preamp", 8, I_OAL_EqualizerPreset,
                  "Preamp:\n\nOverall gain adjustment for all frequencies."),
    MN_SLIDER_ACT("  Low Gain", "snd_eq_low_gain", 8, I_OAL_EqualizerPreset,
                  "Low Gain:\n\nAdjusts bass frequencies (20-250 Hz)."),
    MN_SLIDER_ACT(
        "  Mid 1 Gain", "snd_eq_mid1_gain", 8, I_OAL_EqualizerPreset,
        "Mid 1 Gain:\n\nAdjusts lower mid frequencies (250-1000 Hz)."),
    MN_SLIDER_ACT("  Mid 2 Gain", "snd_eq_mid2_gain", 8, I_OAL_EqualizerPreset,
                  "Mid 2 Gain:\n\nAdjusts upper mid frequencies (1-4 kHz)."),
    MN_SLIDER_ACT("  High Gain", "snd_eq_high_gain", 8, I_OAL_EqualizerPreset,
                  "High Gain:\n\nAdjusts treble frequencies (4-20 kHz)."),
    MN_NUMBER_ACT("  Low Cutoff", "snd_eq_low_cutoff", "Hz",
                  I_OAL_EqualizerPreset,
                  "Low Cutoff:\n\nFrequency boundary for low band."),
    MN_NUMBER_ACT("  Mid 1 Center", "snd_eq_mid1_center", "Hz",
                  I_OAL_EqualizerPreset,
                  "Mid 1 Center:\n\nCenter frequency for lower mid band."),
    MN_NUMBER_ACT("  Mid 2 Center", "snd_eq_mid2_center", "Hz",
                  I_OAL_EqualizerPreset,
                  "Mid 2 Center:\n\nCenter frequency for upper mid band."),
    MN_NUMBER_ACT("  High Cutoff", "snd_eq_high_cutoff", "Hz",
                  I_OAL_EqualizerPreset,
                  "High Cutoff:\n\nFrequency boundary for high band."),
    MN_RESET,
    MN_END
};

mn_page_t equalizer_pages[] = {
    {.items = equalizer_items}
};

mn_menu_t equalizer_menu = {
    .title_patch = "",
    .title_text = "Equalizer",
    .background = "FLOOR4_6",
    .flags = MN_MENU_HINTS | MN_MENU_CENTER_LAYOUT,
    .layout = {.max_width = MN_PX(500)},
    .pages = equalizer_pages,
    .num_pages = arrlen(equalizer_pages)
};

static const char *native_midi_reset_strings[] = {
    "No SysEx", "GM", "GS", "XG"
};

static mn_item_t audio_advanced_items[] = {
    MN_TITLE("Sound"),
    MN_TOGGLE("Pitch-Shifting", "pitched_sounds", NULL),
    MN_TOGGLE("Disable Cutoffs", "full_sounds", NULL),
    MN_SLIDER("SFX Channels", "snd_channels", 8, NULL),
    MN_TOGGLE_ACT("Output Limiter", "snd_limiter", MN_OptionsSetSoundModule,
        "Output Limiter:\n\nPrevents audio clipping by limiting peak volume."),
    {.label = "Resampler",
     .cfg = "snd_resampler",
     .getchoices = MN_GetResamplerStrings,
     .choice_size = 18,
     .action = I_OAL_SetResampler,
     .flags = MN_ITEM_DEFER_ACTION,
     .description = "Resampler:\n\nSelects the sample rate conversion algorithm."},
    MN_SUBMENU("Equalizer", &equalizer_menu),
    MN_GAP(6),
    MN_TITLE("Music"),
    MN_TOGGLE_ACT("Auto Gain", "auto_gain", MN_OptionsResetAutoGain,
        "Auto Gain:\n\nAutomatically normalizes music volume across tracks."),
#if defined(HAVE_FLUIDSYNTH)
    MN_SLIDER_ACT_SUFFIX("  FluidSynth Gain", "fl_gain", 8,
                         MN_OptionsUpdateMusicVolume, "dB",
                         "FluidSynth Gain:\n\nAdjusts FluidSynth synthesizer "
                         "volume in decibels."),
#endif
    MN_SLIDER_ACT_SUFFIX("  OPL3 Gain", "opl_gain", 8,
        MN_OptionsUpdateMusicVolume, "dB",
        "OPL3 Gain:\n\nAdjusts OPL3 FM synthesis volume in decibels."),
    MN_CHOICE_ACT(
        "Native MIDI Reset Type", "midi_reset_type", native_midi_reset_strings,
        8, MN_OptionsSetMidiPlayer,
        "Native MIDI Reset Type:\n\nNo SysEx: No reset sent.\n\n"
        "GM: General MIDI reset.\n\nGS: Roland GS reset.\n\nXG: Yamaha XG reset."),
    MN_RESET,
    MN_END
};

static mn_page_t video_pages[] = {
    {.tab = "General",  .items = video_general_items  },
    {.tab = "Graphics", .items = video_graphics_items }
};

mn_menu_t video_menu = {
    .title_patch = "",
    .title_text = "Video",
    .background = "FLOOR4_6",
    .flags = MN_MENU_HINTS | MN_MENU_CENTER_LAYOUT,
    .layout = {.max_width = MN_PX(500)},
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
    .flags = MN_MENU_HINTS | MN_MENU_CENTER_LAYOUT,
    .layout = {.max_width = MN_PX(500)},
    .pages = audio_pages,
    .num_pages = arrlen(audio_pages),
};

static mn_item_t input_bindings_action_items[] = {
    MN_INPUT("Fire", input_fire, m_scrn),
    MN_INPUT("Forward", input_forward, m_scrn),
    MN_INPUT("Backward", input_backward, m_scrn),
    MN_INPUT("Strafe Left", input_strafeleft, m_scrn),
    MN_INPUT("Strafe Right", input_straferight, m_scrn),
    MN_INPUT("Use", input_use, m_scrn),
    MN_INPUT("Run", input_speed, m_scrn),
    MN_INPUT("Strafe", input_strafe, m_scrn),
    MN_INPUT("Turn Left", input_turnleft, m_scrn),
    MN_INPUT("Turn Right", input_turnright, m_scrn),
    MN_INPUT("180 Turn", input_reverse, m_scrn),
    MN_INPUT("Gyro", input_gyro, m_gyro),
    MN_GAP(6),
    MN_INPUT("Autorun", input_autorun, m_scrn),
    MN_INPUT("Free Look", input_freelook, m_scrn),
    MN_INPUT("Vertmouse", input_novert, m_scrn),
    MN_RESET,
    MN_END
};

static mn_item_t input_bindings_weapon_items[] = {
    MN_INPUT("Fist", input_weapon1, m_scrn),
    MN_INPUT("Pistol", input_weapon2, m_scrn),
    MN_INPUT("Shotgun", input_weapon3, m_scrn),
    MN_INPUT("Chaingun", input_weapon4, m_scrn),
    MN_INPUT("Rocket", input_weapon5, m_scrn),
    MN_INPUT("Plasma", input_weapon6, m_scrn),
    MN_INPUT("BFG", input_weapon7, m_scrn),
    MN_INPUT("Chainsaw", input_weapon8, m_scrn),
    MN_INPUT("SSG", input_weapon9, m_scrn),
    MN_INPUT("Best", input_weapontoggle, m_scrn),
    MN_INPUT("Last", input_lastweapon, m_scrn),
    MN_GAP(6),
    MN_INPUT("Previous", input_prevweapon, m_scrn),
    MN_INPUT("Next", input_nextweapon, m_scrn),
    MN_RESET,
    MN_END
};

static mn_item_t input_bindings_misc_items[] = {
    MN_INPUT("Rewind", input_rewind, m_scrn),
    MN_INPUT("Reload Map/Demo", input_menu_reloadlevel, m_scrn),
    MN_INPUT("Next Map", input_menu_nextlevel, m_scrn),
    MN_INPUT("Previous Map", input_menu_prevlevel, m_scrn),
    MN_INPUT("Show Stats/Time", input_hud_timestats, m_scrn),
    MN_GAP(6),
    MN_INPUT("Fast-FWD Demo", input_demo_fforward, m_scrn),
    MN_INPUT("Finish Demo", input_demo_quit, m_scrn),
    MN_INPUT("Join Demo", input_demo_join, m_scrn),
    MN_INPUT("Increase Speed", input_speed_up, m_scrn),
    MN_INPUT("Decrease Speed", input_speed_down, m_scrn),
    MN_INPUT("Default Speed", input_speed_default, m_scrn),
    MN_GAP(6),
    MN_INPUT("Begin Chat", input_chat, m_scrn),
    MN_INPUT("Show Netgame Stats", input_netgame_stats, m_scrn),
    MN_RESET,
    MN_END
};

static mn_item_t input_bindings_func_items[] = {
    MN_INPUT("Pause", input_pause, m_scrn),
    MN_INPUT("Save", input_savegame, m_scrn),
    MN_INPUT("Load", input_loadgame, m_scrn),
    MN_INPUT("Volume", input_soundvolume, m_scrn),
    MN_INPUT("Hud", input_hud, m_scrn),
    MN_INPUT("Quicksave", input_quicksave, m_scrn),
    MN_INPUT("End Game", input_endgame, m_scrn),
    MN_INPUT("Messages", input_messages, m_scrn),
    MN_INPUT("Quickload", input_quickload, m_scrn),
    MN_INPUT("Quit", input_quit, m_scrn),
    MN_INPUT("Gamma Fix", input_gamma, m_scrn),
    MN_INPUT("Spy", input_spy, m_scrn),
    MN_INPUT("Screenshot", input_screenshot, m_scrn),
    MN_INPUT("Clean Screenshot", input_clean_screenshot, m_scrn),
    MN_INPUT("Larger View", input_zoomin, m_scrn),
    MN_INPUT("Smaller View", input_zoomout, m_scrn),
    MN_RESET,
    MN_END
};

static mn_item_t input_bindings_automap_items[] = {
    MN_INPUT("Toggle Automap", input_map, m_map),
    MN_INPUT("Follow", input_map_follow, m_map),
    MN_INPUT("Overlay", input_map_overlay, m_map),
    MN_INPUT("Rotate", input_map_rotate, m_map),
    MN_INPUT("Minimap", input_map_mini, m_map),
    MN_GAP(6),
    MN_INPUT("Zoom In", input_map_zoomin, m_map),
    MN_INPUT("Zoom Out", input_map_zoomout, m_map),
    MN_INPUT("Shift Up", input_map_up, m_map),
    MN_INPUT("Shift Down", input_map_down, m_map),
    MN_INPUT("Shift Left", input_map_left, m_map),
    MN_INPUT("Shift Right", input_map_right, m_map),
    MN_INPUT("Mark Place", input_map_mark, m_map),
    MN_INPUT("Clear Last Mark", input_map_clear, m_map),
    MN_INPUT("Full/Zoom", input_map_gobig, m_map),
    MN_INPUT("Grid", input_map_grid, m_map),
    MN_RESET,
    MN_END
};

static mn_item_t input_bindings_cheat_items[] = {
    MN_INPUT("Fake Archvile Jump", input_avj, m_scrn),
    MN_INPUT("God Mode/Resurrect", input_iddqd, m_scrn),
    MN_INPUT("Ammo & Keys", input_idkfa, m_scrn),
    MN_INPUT("Ammo", input_idfa, m_scrn),
    MN_INPUT("No Clipping", input_idclip, m_scrn),
    MN_INPUT("Health", input_idbeholdh, m_scrn),
    MN_INPUT("Armor", input_idbeholdm, m_scrn),
    MN_INPUT("Invulnerability", input_idbeholdv, m_scrn),
    MN_INPUT("Berserk", input_idbeholds, m_scrn),
    MN_INPUT("Partial Invisibility", input_idbeholdi, m_scrn),
    MN_INPUT("Radiation Suit", input_idbeholdr, m_scrn),
    MN_INPUT("Reveal Map", input_iddt, m_scrn),
    MN_INPUT("Light Amplification", input_idbeholdl, m_scrn),
    MN_INPUT("No Target", input_notarget, m_scrn),
    MN_INPUT("Freeze", input_freeze, m_scrn),
    MN_RESET,
    MN_END
};

static mn_page_t input_bindings_pages[] = {
    {.tab = "Action",  .items = input_bindings_action_items  },
    {.tab = "Weapon",  .items = input_bindings_weapon_items  },
    {.tab = "Misc",    .items = input_bindings_misc_items    },
    {.tab = "Func",    .items = input_bindings_func_items    },
    {.tab = "Automap", .items = input_bindings_automap_items },
    {.tab = "Cheats",  .items = input_bindings_cheat_items   }
};

mn_menu_t input_bindings_menu = {
    .title_patch = "",
    .title_text = "Input Bindings",
    .background = "FLOOR4_6",
    .flags = MN_MENU_HINTS | MN_MENU_CENTER_LAYOUT,
    .layout = {.max_width = MN_PX(500)},
    .pages = input_bindings_pages,
    .num_pages = arrlen(input_bindings_pages),
};

static const char *freelook_strings[] = {"Off", "Auto-Aim", "Direct Aim"};
#define FREELOOK_DESC                         \
    "Freelook:\n\nOff: No vertical look.\n\n" \
    "Auto-Aim: Auto-aims at enemies.\n\n"     \
    "Direct Aim: Full manual vertical aiming."

static mn_item_t mouse_keyboard_items[] = {
    MN_SUBMENU("Input Bindings", &input_bindings_menu),
    MN_GAP(6),
    MN_TITLE("Mouse"),
    MN_SLIDER_ACT("Turn Sensitivity", "mouse_sensitivity", 11,
                  G_UpdateMouseVariables,
                  "Turn Sensitivity:\n\nHorizontal mouse turning speed."),
    MN_SLIDER_ACT("Look Sensitivity", "mouse_sensitivity_y_look", 11,
                  G_UpdateMouseVariables,
                  "Look Sensitivity:\n\nVertical mouse looking speed."),
    MN_SLIDER_ACT(
        "Move Sensitivity", "mouse_sensitivity_y", 11, G_UpdateMouseVariables,
        "Move Sensitivity:\n\nVertical mouse movement speed (forward/back)."),
    MN_SLIDER_ACT("Strafe Sensitivity", "mouse_sensitivity_strafe", 11,
                  G_UpdateMouseVariables,
                  "Strafe Sensitivity:\n\nHorizontal mouse strafing speed."),
    MN_GAP(6),
    MN_CHOICE_ACT("Freelook", "freelook_mode", freelook_strings, 8,
                  MN_OptionsUpdateFreeLookMode, FREELOOK_DESC),
    MN_TOGGLE_ACT("Invert Look", "mouse_y_invert", G_UpdateMouseVariables,
                  "Invert Look:\n\nInverts vertical mouse look direction."),
    MN_TOGGLE("Double-Click to \"Use\"", "dclick_use", NULL),
    MN_RESET,
    MN_END};

static const char *layout_strings[] = {
    "Off",
    "Default",
    "Southpaw",
    "Legacy",
    "Legacy Southpaw",
    "Flick Stick",
    "Flick Stick Southpaw",
};

static const char *movement_type_strings[] = {"Normalized", "Faster Diagonals"};

static const char *curve_strings[] = {
    "",       "",    "",    "",        "",    "",    "",
    "",       "",    "", // Dummy values, start at 1.0.
    "Linear", "1.1", "1.2", "1.3",     "1.4", "1.5", "1.6",
    "1.7",    "1.8", "1.9", "Squared", "2.1", "2.2", "2.3",
    "2.4",    "2.5", "2.6", "2.7",     "2.8", "2.9", "Cubed"
};

static mn_item_t advanced_gamepad_items[] = {
    MN_CHOICE_ACT(
        "Movement Type", "joy_movement_type", movement_type_strings, 14,
        I_ResetGamepad,
        "Movement Type:\n\nNormalized: Consistent speed in all "
        "directions.\nFaster Diagonals: Original Doom diagonal speed boost."),
    MN_SLIDER_ACT_DYN(
        "Forward Sensitivity", "joy_forward_sensitivity",
        MN_GetMovementSensitivityStrings, 8, I_ResetGamepad,
        "Forward Sensitivity:\n\nLeft stick forward movement sensitivity."),
    MN_SLIDER_ACT_DYN(
        "Strafe Sensitivity", "joy_strafe_sensitivity",
        MN_GetMovementSensitivityStrings, 8, I_ResetGamepad,
        "Strafe Sensitivity:\n\nLeft stick strafe movement sensitivity."),
    MN_SLIDER_ACT("Extra Turn Speed", "joy_outer_turn_speed", 11,
                  MN_OptionsUpdateGamepad,
                  "Extra Turn Speed:\n\nAdditional turn speed when stick is at "
                  "outer edge."),
    MN_SLIDER_ACT_DYN(
        "  Extra Ramp Time", "joy_outer_ramp_time", MN_GetMsTimeStrings, 8,
        I_ResetGamepad,
        "Extra Ramp Time:\n\nTime to reach extra turn speed at outer edge."),
    MN_SLIDER_ACT_STR("Response Curve", "joy_camera_curve", curve_strings, 8,
                      I_ResetGamepad,
                      "Response Curve:\n\nLinear: Direct stick-to-turn "
                      "mapping.\nSquared/Cubed: Exponential curves for "
                      "precision.\n1.1-2.9: Custom exponential curves."),
    MN_RESET,
    MN_END
};

mn_page_t advanced_gamepad_pages[] = {
    {.items = advanced_gamepad_items}
};

mn_menu_t advanced_gamepad_menu = {
    .title_patch = "",
    .title_text = "Advanced Gamepad",
    .background = "FLOOR4_6",
    .flags = MN_MENU_HINTS | MN_MENU_CENTER_LAYOUT,
    .layout = {.max_width = MN_PX(500)},
    .pages = advanced_gamepad_pages,
    .num_pages = arrlen(advanced_gamepad_pages)
};

static mn_item_t gamepad_items[] = {
    {.label = "Device",
     .cfg = "joy_device",
     .type = MN_ITEM_CHOICE,
     .getchoices = MN_GetGamepadDeviceStrings,
     .choice_size = 16,
     .action = MN_OptionsUpdateGamepadDevice,
     .flags = MN_ITEM_DEFER_ACTION | MN_ITEM_TWO_LINES,
     .description = "Device:\n\nSelects the active gamepad device."},
    MN_GAP(6),
    MN_SUBMENU("Input Bindings", &input_bindings_menu),
    MN_CHOICE_ACT("Stick Layout", "joy_stick_layout", layout_strings, 16,
                  MN_OptionsUpdateGamepad, NULL),
    MN_SLIDER_ACT_DYN(
        "  Flick Stick Time", "joy_flick_time", MN_GetMsTimeStrings, 8,
        I_ResetGamepad,
        "Flick Stick Time:\n\nTime window for flick stick gesture detection."),
    MN_SLIDER_ACT("Vibration", "joy_rumble", 8, MN_OptionsUpdateRumble,
                  "Vibration:\n\nController vibration intensity."),
    MN_GAP(6),
    MN_TITLE("Movement/Camera"),
    MN_SLIDER_ACT("Turn Speed", "joy_turn_speed", 11, I_ResetGamepad,
                  "Turn Speed:\n\nHorizontal camera rotation speed."),
    MN_SLIDER_ACT("Look Speed", "joy_look_speed", 11, I_ResetGamepad,
                  "Look Speed:\n\nVertical camera rotation speed."),
    MN_CHOICE_ACT("Freelook", "freelook_mode", freelook_strings, 8,
                  MN_OptionsUpdateFreeLookMode, FREELOOK_DESC),
    MN_TOGGLE_ACT("Invert Look", "joy_invert_look", I_ResetGamepad,
                  "Invert Look:\n\nInverts vertical camera look direction."),
    MN_SLIDER_ACT_SUFFIX(
        "Movement Deadzone", "joy_movement_inner_deadzone", 8, I_ResetGamepad,
        "%", "Movement Deadzone:\n\nInner deadzone for movement stick."),
    MN_SLIDER_ACT_SUFFIX(
        "Camera Deadzone", "joy_camera_inner_deadzone", 8, I_ResetGamepad, "%",
        "Camera Deadzone:\n\nInner deadzone for camera stick."),
    MN_SUBMENU("Advanced Options", &advanced_gamepad_menu),
    MN_RESET,
    MN_END
};

static const char *gyro_space_strings[] = {"Local", "Player"};

static mn_item_t advanced_gyro_items[] = {
    MN_CHOICE_ACT("Gyro Orientation", "gyro_space",
        gyro_space_strings, 8, I_ResetGamepad,
        "Gyro Orientation:\n\nLocal: Relative to controller orientation.\n\n"
        "Player: Relative to player view direction."),
    // !!! TODO Local Roll
    MN_SLIDER_ACT_DYN("Gyro Steadying", "gyro_smooth_threshold",
        MN_GetGyroSensitivityStrings, 11, MN_OptionsUpdateGyroSteadying,
        "Gyro Steadying:\n\nSmooths small gyro movements for stability."),
    // !!! TODO Gyro Smoothing
    MN_SLIDER_ACT_DYN("Gyro Acceleration", "gyro_acceleration",
        MN_GetGyroAccelStrings, 11, MN_OptionsUpdateGyroAcceleration,
        "Gyro Acceleration:\n\nIncreases sensitivity for fast movements."),
    MN_SLIDER_ACT("  Lower Threshold", "gyro_accel_min_threshold", 11, I_ResetGamepad,
        "Lower Threshold:\n\nMinimum speed for acceleration to begin."),
    MN_SLIDER_ACT("  Upper Threshold", "gyro_accel_max_threshold", 11, I_ResetGamepad,
        "Upper Threshold:\n\nMaximum speed for full acceleration."),
    MN_RESET, MN_END
};

mn_page_t advanced_gyro_pages[] = {
    {.items = advanced_gyro_items}
};

mn_menu_t advanced_gyro_menu = {
    .title_patch = "",
    .title_text = "Advanced Gyro",
    .background = "FLOOR4_6",
    .flags = MN_MENU_HINTS | MN_MENU_CENTER_LAYOUT,
    .layout = {.max_width = MN_PX(500)},
    .pages = advanced_gyro_pages,
    .num_pages = arrlen(advanced_gyro_pages)
};

static const char *gyro_action_strings[] = {
    "None",
    "Disable Gyro",
    "Enable Gyro",
    "Invert Gyro",
    "Reset Camera",
    "Reset / Disable Gyro",
    "Reset / Enable Gyro",
    "Reset / Invert Gyro"
};

static mn_item_t gyro_items[] = {
    MN_TOGGLE_ACT("Gyro Aiming", "gyro_enable", MN_OptionsUpdateGyroAiming,
        "Gyro Aiming:\n\nEnables motion-controlled aiming using gyroscope."),
    MN_CHOICE_ACT("Gyro Button", "gyro_button_action",
                  gyro_action_strings, 16, I_ResetGamepad, NULL),
    MN_CHOICE_ACT("Camera Stick", "gyro_stick_action",
                  gyro_action_strings, 16, I_ResetGamepad, NULL),
    MN_SUBMENU("Advanced Options", &advanced_gyro_menu),
    MN_SLIDER_ACT_DYN("Turn Sensitivity", "gyro_turn_sensitivity",
        MN_GetGyroSensitivityStrings, 11, I_ResetGamepad,
        "Turn Sensitivity:\n\nHorizontal gyro aiming sensitivity."),
    MN_SLIDER_ACT_DYN("Look Sensitivity", "gyro_look_sensitivity",
        MN_GetGyroSensitivityStrings, 11, I_ResetGamepad,
        "Look Sensitivity:\n\nVertical gyro aiming sensitivity."),
    MN_CHOICE_ACT("Freelook", "freelook_mode", freelook_strings, 8,
        MN_OptionsUpdateFreeLookMode, FREELOOK_DESC),
    {.label = "Calibrate Gyro",
     .type = MN_ITEM_ACTION,
     .action = I_UpdateGyroCalibrationState,
     .description = "Calibrate Gyro:\n\nCalibrates the gyroscope. Place "
                    "controller on a flat surface."},
    MN_RESET,
    MN_END
};

#define MN_WEAPON_SLOT(i, key)              \
    {.label = slot_labels[i],               \
     .cfg = (key),                          \
     .type = MN_ITEM_CHOICE,                \
     .getchoices = MN_GetWeaponSlotStrings, \
     .choice_size = 8,                      \
     .action = MN_OptionsUpdateWeaponSlots}

static const char *weapon_slots_activation_strings[] = {
    "Off", "Hold \"Last\"", "Always On"
};
static const char *weapon_slots_selection_strings[] = {
    "D-Pad", "Face Buttons", "1-4 Keys"
};

static mn_item_t slots_items[] = {
    MN_CHOICE_ACT(
        "Enable Slots", "weapon_slots_activation",
        weapon_slots_activation_strings, 12,
        MN_OptionsUpdateWeaponSlotActivation,
        "Enable Slots:\n\nOff: Weapon slots disabled.\n\n"
        "Hold \"Last\": Activate by holding Last Weapon key.\n\n"
        "Always On: Slots always active."),
    MN_CHOICE_ACT(
        "Select Slots", "weapon_slots_selection",
        weapon_slots_selection_strings, 12,
        MN_OptionsUpdateWeaponSlotSelection,
        "Select Slots:\n\nD-Pad: Use D-Pad for slot selection.\n\n"
        "Face Buttons: Use face buttons.\n\n"
        "1-4 Keys: Use number keys 1-4."),
    MN_GAP(6),
    MN_WEAPON_SLOT(0, "weapon_slots_1_1"),
    MN_WEAPON_SLOT(1, "weapon_slots_1_2"),
    MN_WEAPON_SLOT(2, "weapon_slots_1_3"),
    MN_GAP(6),
    MN_WEAPON_SLOT(3, "weapon_slots_2_1"),
    MN_WEAPON_SLOT(4, "weapon_slots_2_2"),
    MN_WEAPON_SLOT(5, "weapon_slots_2_3"),
    MN_GAP(6),
    MN_WEAPON_SLOT(6, "weapon_slots_3_1"),
    MN_WEAPON_SLOT(7, "weapon_slots_3_2"),
    MN_WEAPON_SLOT(8, "weapon_slots_3_3"),
    MN_GAP(6),
    MN_WEAPON_SLOT(9, "weapon_slots_4_1"),
    MN_WEAPON_SLOT(10, "weapon_slots_4_2"),
    MN_WEAPON_SLOT(11, "weapon_slots_4_3"),
    MN_RESET,
    MN_END
};

static mn_page_t controls_pages[] = {
    {.tab = "Mouse / Keyboard", .items = mouse_keyboard_items},
    {.tab = "Gamepad", .items = gamepad_items},
    {.tab = "Gyro", .items = gyro_items, .draw = MN_DrawGyroPage},
    {.tab = "Slots", .items = slots_items},
};

mn_menu_t controls_menu = {
    .title_patch = "",
    .title_text = "Controls",
    .background = "FLOOR4_6",
    .flags = MN_MENU_HINTS | MN_MENU_CENTER_LAYOUT,
    .layout = {.max_width = MN_PX(500)},
    .pages = controls_pages,
    .num_pages = arrlen(controls_pages),
};

static const char *hud_anchoring_strings[] = {
    "Wide", "4:3", "16:9", "21:9"
};
static const char *show_widgets_strings[] = {"Off", "Automap", "HUD", "Always"};
static const char *show_adv_widgets_strings[] = {
    "Off", "Automap", "HUD", "Always", "Advanced"
};
static const char *stats_format_strings[] = {
    "Ratio", "Boolean", "Percent", "Remaining", "Count"
};

static mn_item_t statusbar_items[] = {
    MN_SLIDER_ACT_DYN(
        "HUD Layout", "screenblocks",
        MN_GetScreenSizeStrings, 6, MN_OptionsViewSize,
        "HUD Layout:\n\nChoose alternative HUD."),
    MN_CHOICE_ACT(
        "HUD Anchoring", "hud_anchoring",
        hud_anchoring_strings, 6, I_UpdateHudAnchoring,
        "HUD Anchoring:\n\nWide: Anchors to screen edges."),
    MN_GAP(6),
    MN_TITLE("Widgets"),
    MN_CHOICE_ACT("Show Level Stats", "hud_level_stats", show_widgets_strings,
        8, MN_OptionsUpdateStatsFormatItem, NULL),
    MN_CHOICE("Show Level Time", "hud_level_time", show_widgets_strings, 8, NULL),
    MN_CHOICE("Show Player Coords", "hud_player_coords",
              show_adv_widgets_strings, 8, NULL),
    MN_TOGGLE_ACT(
        "Show Command History", "hud_command_history", HU_ResetCommandHistory,
        "Show Command History:\n\nDisplays recent commands on screen."),
    MN_TOGGLE("Show Use-Button Timer", "hud_time_use", NULL),
    MN_GAP(6),
    MN_TITLE("Widget Appearance"),
    MN_CHOICE("Use Doom Font", "hud_widget_font", show_widgets_strings, 8, NULL),
    MN_CHOICE("Level Stats Format", "hud_stats_format", stats_format_strings, 8, NULL),
    MN_GAP(6),
    MN_TITLE("Status Bar"),
    MN_TOGGLE("Colored Numbers", "sts_colored_numbers", NULL),
    MN_TOGGLE("Solid Background", "st_solidbackground", NULL),
    MN_RESET,
    MN_END
};

static const char *crosshair_target_strings[] = {"Off", "Highlight", "Health"};
static const char *hudcolor_strings[] = {
    "BRICK",  "TAN",    "GRAY",  "GREEN", "BROWN",  "GOLD",  "RED", "BLUE",
    "ORANGE", "YELLOW", "BLUE2", "BLACK", "PURPLE", "WHITE", "NONE"
};

static mn_item_t crosshair_items[] = {
    {.label = "Crosshair",
     .cfg = "hud_crosshair",
     .type = MN_ITEM_CHOICE,
     .choices = crosshair_strings,
     .num_choices = HU_CROSSHAIRS,
     .choice_size = 12,
     .action = MN_OptionsUpdateCrosshairItems,
     .flags = MN_ITEM_DRAW_CROSSHAIR},
    MN_TOGGLE("Color By Player Health", "hud_crosshair_health", NULL),
    MN_CHOICE_ACT(
        "Color By Target", "hud_crosshair_target", crosshair_target_strings, 8,
        MN_OptionsUpdateCrosshairItems,
        "Color By Target:\n\nOff: No target coloring.\n\n"
        "Highlight: Colors crosshair on target.\n\n"
        "Health: Colors by target health percentage."),
    MN_TOGGLE("Lock On Target", "hud_crosshair_lockon", NULL),
    MN_CHOICE("Default Color", "hud_crosshair_color", hudcolor_strings, 8, NULL),
    MN_CHOICE("Highlight Color", "hud_crosshair_target_color", hudcolor_strings, 8, NULL),
    MN_RESET,
    MN_END
};

static const char *secretmessage_strings[] = {"Off", "On", "Count",};

static mn_item_t messages_items[] = {
    MN_CHOICE("Announce Revealed Secrets", "hud_secret_message",
              secretmessage_strings, 6, NULL),
    MN_TOGGLE("Announce Map Titles", "hud_map_announce", NULL),
    MN_GAP(6),
    MN_TOGGLE("Show Toggle Messages", "show_toggle_messages", NULL),
    MN_TOGGLE("Show Pickup Messages", "show_pickup_messages", NULL),
    MN_TOGGLE("Show Obituaries", "show_obituary_messages", NULL),
    MN_GAP(6),
    MN_TOGGLE("Center Messages", "message_centered", NULL),
    MN_TOGGLE("Colorize Messages", "message_colorized", NULL),
    MN_RESET,
    MN_END
};

static const char *automap_thickness_strings[] = {
    "Auto", "1", "2", "3", "4", "5", "6"
};
static const char *overlay_strings[] = {"Off", "On", "Dark"};
static const char *automap_preset_strings[] = {"Vanilla", "Crispy", "Boom", "ZDoom"};
static const char *automap_keyed_door_strings[] = {"Off", "On", "Flashing"};

static mn_item_t automap_items[] = {
    MN_TITLE("Modes"),
    MN_TOGGLE("Follow Player", "followplayer", NULL),
    MN_TOGGLE("Rotate Automap", "automaprotate", NULL),
    MN_CHOICE("Overlay Automap", "automapoverlay", overlay_strings, 6, NULL),
    MN_GAP(6),
    MN_TOGGLE("Show Minimap", "minimap", NULL),
    MN_SLIDER_ACT_STR(
        "Line Thickness", "map_line_thickness",
        automap_thickness_strings, 4, AM_ResetThickness,
        "Line Thickness:\n\nAuto: Automatic based on resolution.\n\n"
        "1-6: Fixed line thickness."),
    MN_GAP(6),
    MN_TITLE("Miscellaneous"),
    MN_CHOICE_ACT(
        "Color Preset", "mapcolor_preset", automap_preset_strings, 6,
        AM_ColorPreset, NULL),
    MN_TOGGLE("Show Found Secrets Only", "map_secret_after", NULL),
    MN_CHOICE("Color Keyed Doors", "map_keyed_door", automap_keyed_door_strings,
              6, NULL),
    MN_RESET,
    MN_END};

static mn_page_t interface_pages[] = {
    {.tab = "General", .items = statusbar_items},
    {.tab = "Crosshair", .items = crosshair_items},
    {.tab = "Messages", .items = messages_items},
    {.tab = "Automap", .items = automap_items},
};

mn_menu_t interface_menu = {
    .title_patch = "",
    .title_text = "Interface",
    .background = "FLOOR4_6",
    .flags = MN_MENU_HINTS | MN_MENU_CENTER_LAYOUT,
    .layout = {.max_width = MN_PX(500)},
    .pages = interface_pages,
    .num_pages = arrlen(interface_pages),
};

static const char *overunder_strings[] = {"Off", "Player only", "All"};

static mn_item_t gameplay_items[] = {
    MN_TOGGLE("Autosave", "autosave", NULL),
    MN_TOGGLE("On Death Action", "death_use_action", NULL),
    MN_GAP(6),
    MN_TITLE("Physics"),
    MN_CHOICE("Move Over/Under Monsters", "overunder", overunder_strings, 8, NULL),
    MN_TOGGLE_ACT(
        "Auto Strafe 50", "autostrafe50", G_UpdateSideMove,
        "Auto Strafe 50:\n\nAutomatically applies SR50 turning for maximum speed."),
    MN_TOGGLE("Improved Hit Detection", "blockmapfix", NULL),
    MN_RESET,
    MN_END
};

static const char *bobbing_pct_strings[] = {"0%", "25%", "50%", "75%", "100%"};

static const char *center_weapon_strings[] = {"Off", "Centered", "Bobbing"};

static mn_item_t weapons_items[] = {
    MN_SLIDER_STR(
        "View Bob", "view_bobbing_pct", bobbing_pct_strings, 8,
        "View Bob:\n\nAmount of camera bobbing when moving."),
    MN_SLIDER_ACT_STR(
        "Weapon Bob", "weapon_bobbing_pct", bobbing_pct_strings, 8,
        MN_OptionsUpdateCenteredWeaponItem,
        "Weapon Bob:\n\nAmount of weapon sprite bobbing when moving."),
    MN_GAP(6),
    MN_CHOICE("Weapon Alignment", "center_weapon", center_weapon_strings, 8, NULL),
    MN_TOGGLE("Hide Weapon", "hide_weapon", NULL),
    MN_TOGGLE("Weapon Recoil", "weapon_recoilpitch", NULL),
    MN_GAP(6),
    MN_TOGGLE("Weapon Carousel", "weapon_carousel", NULL),
    MN_TOGGLE("Vanilla Weapon Cycle", "doom_weapon_cycle", NULL),
    MN_TOGGLE("Use Weapon Toggles", "doom_weapon_toggles", NULL),
    MN_GAP(6),
    MN_TOGGLE("Pre-Beta BFG", "classic_bfg", NULL),
    MN_RESET,
    MN_END
};

static mn_page_t gameplay_pages[] = {
    {.tab = "General", .items = gameplay_items},
    {.tab = "Weapons", .items = weapons_items},
    {.tab = "Automap", .items = automap_items},
};

mn_menu_t gameplay_menu = {
    .title_patch = "",
    .title_text = "Gameplay",
    .background = "FLOOR4_6",
    .flags = MN_MENU_HINTS | MN_MENU_CENTER_LAYOUT,
    .layout = {.max_width = MN_PX(500)},
    .pages = gameplay_pages,
    .num_pages = arrlen(gameplay_pages),
};

static mn_item_t options_items[] = {
    MN_MENU("Video", &video_menu),
    MN_MENU("Audio", &audio_menu),
    MN_MENU("Controls", &controls_menu),
    MN_MENU("Gameplay", &gameplay_menu),
    MN_MENU("Interface", &interface_menu),
    MN_END
};

static mn_page_t options_pages[] = {
    {.items = options_items},
};

mn_menu_t options_menu = {
    .title_patch = "",
    .title_text = "",
    .background = "FLOOR4_6",
    .flags = MN_MENU_CENTER,
    .item_spacing = 3,
    .pages = options_pages,
    .num_pages = arrlen(options_pages),
};
