//
//  Copyright (C) 1999 by
//  id Software, Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman
//  Copyright(C) 2020-2021 Fabian Greffrath
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
//
// DESCRIPTION:
//  Main loop menu stuff.
//  Default Config File.
//  PCX Screenshots.
//
//-----------------------------------------------------------------------------

#include "doomstat.h"
#include "doomkeys.h"
#include "m_argv.h"
#include "g_game.h"
#include "m_menu.h"
#include "am_map.h"
#include "w_wad.h"
#include "i_printf.h"
#include "i_system.h"
#include "i_sound.h"
#include "i_video.h"
#include "v_video.h"
#include "hu_obituary.h"
#include "hu_stuff.h"
#include "st_stuff.h"
#include "dstrings.h"
#include "m_misc.h"
#include "m_misc2.h"
#include "m_swap.h"
#include "s_sound.h"
#include "sounds.h"
#include "d_main.h"
#include "r_draw.h" // [FG] fuzzcolumn_mode
#include "r_sky.h" // [FG] stretchsky
#include "hu_lib.h" // HU_MAXMESSAGES
#include "net_client.h" // net_player_name
#include "i_gamepad.h"
#include "m_array.h"

#include "m_io.h"
#include <errno.h>

//
// DEFAULTS
//

static int config_help;         //jff 3/3/98
// [FG] double click acts as "use"
extern int dclick_use;
// [FG] invert vertical axis
extern int mouse_y_invert;
extern int realtic_clock_rate;         // killough 4/13/98: adjustable timer
extern int tran_filter_pct;            // killough 2/21/98
extern int showMessages;
extern int show_toggle_messages;
extern int show_pickup_messages;

extern int window_width, window_height;
extern int window_position_x, window_position_y;
extern boolean flipcorpses; // [crispy] randomly flip corpse, blood and death animation sprites
extern boolean ghost_monsters; // [crispy] resurrected pools of gore ("ghost monsters") are translucent
extern int mouse_acceleration;
extern int mouse_acceleration_threshold;
extern int show_endoom;
#if defined(HAVE_FLUIDSYNTH)
extern char *soundfont_path;
extern char *soundfont_dir;
extern boolean mus_chorus;
extern boolean mus_reverb;
extern int     mus_gain;
#endif
#if defined(_WIN32)
extern char *winmm_device;
extern int winmm_complevel;
extern int winmm_reset_type;
extern int winmm_reset_delay;
#endif
extern int opl_gain;
extern boolean demobar;
extern boolean smoothlight;
extern boolean brightmaps;
extern boolean r_swirl;
extern int death_use_action;
extern boolean palette_changes;
extern boolean screen_melt;
extern boolean hangsolid;
extern boolean blockmapfix;
extern int extra_level_brightness;

extern char *chat_macros[];  // killough 10/98

//jff 3/3/98 added min, max, and help string to all entries
//jff 4/10/98 added isstr field to specify whether value is string or int
//
// killough 11/98: entirely restructured to allow options to be modified
// from wads, and to consolidate with menu code

default_t defaults[] = {

  { //jff 3/3/98
    "config_help",
    (config_t *) &config_help, NULL,
    {1}, {0,1}, number, ss_none, wad_no,
    "1 to show help strings about each variable in config file"
  },

  //
  // Video
  //

  {
    "current_video_height",
    (config_t *) &current_video_height, NULL,
    {600}, {SCREENHEIGHT, UL}, number, ss_none, wad_no,
    "vertical resolution (600p by default)"
  },

  {
    "resolution_scale",
    (config_t *) &resolution_scale, NULL,
    {0}, {0, UL}, number, ss_gen, wad_no,
    "resolution scale menu index"
  },

  {
    "dynamic_resolution",
    (config_t *) &dynamic_resolution, NULL,
    {1}, {0, 1}, number, ss_gen, wad_no,
    "1 to enable dynamic resolution"
  },

  {
    "sdl_renderdriver",
    (config_t *) &sdl_renderdriver, NULL,
#if defined(_WIN32)
    {.s = "direct3d11"},
#else
    {.s = ""},
#endif
    {0}, string, ss_none, wad_no,
    "SDL render driver, possible values are "
#if defined(_WIN32)
    "direct3d, direct3d11, direct3d12, "
#elif defined(__APPLE__)
    "metal, "
#endif
    "opengl, opengles2, opengles, software"
  },

  {
    "correct_aspect_ratio",
    (config_t *) &use_aspect, NULL,
    {1}, {0, 1}, number, ss_none, wad_no,
    "1 to perform aspect ratio correction"
  },

  // [FG] save fullscren mode
  {
    "fullscreen",
    (config_t *) &fullscreen, NULL,
    {1}, {0, 1}, number, ss_none, wad_no,
    "1 to enable fullscreen mode"
  },

  {
    "exclusive_fullscreen",
    (config_t *) &exclusive_fullscreen, NULL,
    {0}, {0, 1}, number, ss_none, wad_no,
    "1 to enable exclusive fullscreen mode"
  },

  {
    "use_vsync",
    (config_t *) &use_vsync, NULL,
    {1}, {0,1}, number, ss_gen, wad_no,
    "1 to enable wait for vsync to avoid display tearing"
  },

  // [FG] uncapped rendering frame rate
  {
    "uncapped",
    (config_t *) &default_uncapped, (config_t *) &uncapped,
    {1}, {0, 1}, number, ss_gen, wad_no,
    "1 to enable uncapped rendering frame rate"
  },

  // framerate limit
  {
    "fpslimit",
    (config_t *) &fpslimit, NULL,
    {0}, {0, 500}, number, ss_gen, wad_no,
    "framerate limit in frames per second (< 35 = disable)"
  },

  // widescreen mode
  {
    "widescreen",
    (config_t *) &default_widescreen, NULL,
    {RATIO_AUTO}, {RATIO_ORIG, NUM_RATIOS-1}, number, ss_gen, wad_no,
    "Widescreen (0 = Off, 1 = Auto, 2 = 16:10, 3 = 16:9, 4 = 21:9, 5 = 32:9)"
  },

  {
    "fov",
    (config_t *) &custom_fov, NULL,
    {0}, {0, FOVMAX}, number, ss_gen, wad_no,
    "Field of view in degrees (0 = Auto, 40 to 140 = Custom)"
  },

  // display index
  {
    "video_display",
    (config_t *) &video_display, NULL,
    {0}, {0, UL}, number, ss_none, wad_no,
    "current video display index"
  },

  // window position
  {
    "window_position_x",
    (config_t *) &window_position_x, NULL,
    {0}, {UL, UL}, number, ss_none, wad_no,
    "window position x"
  },

  {
    "window_position_y",
    (config_t *) &window_position_y, NULL,
    {0}, {UL, UL}, number, ss_none, wad_no,
    "window position y"
  },

  // window width
  {
    "window_width",
    (config_t *) &window_width, NULL,
    {1065}, {0, UL}, number, ss_none, wad_no,
    "window width"
  },

  // window height
  {
    "window_height",
    (config_t *) &window_height, NULL,
    {600}, {0, UL}, number, ss_none, wad_no,
    "window height"
  },

  {
    "gamma2",
    (config_t *) &gamma2, NULL,
    {9}, {0,17}, number, ss_gen, wad_no,
    "custom gamma level (0 = -4, 9 = 0, 17 = 4)"
  },

  {
    "smooth_scaling",
    (config_t *) &smooth_scaling, NULL,
    {1}, {0,1}, number, ss_gen, wad_no,
    "enable smooth pixel scaling"
  },

  {
    "vga_porch_flash",
    (config_t *) &vga_porch_flash, NULL,
    {0}, {0, 1}, number, ss_none, wad_no,
    "1 to emulate VGA \"porch\" behaviour"
  },

  { // killough 10/98
    "disk_icon",
    (config_t *) &disk_icon, NULL,
    {1}, {0,1}, number, ss_gen, wad_no,
    "1 to enable flashing icon during disk IO"
  },

  {
    "show_endoom",
    (config_t *) &show_endoom, NULL,
    {0}, {0,2}, number, ss_gen, wad_no,
    "show ENDOOM screen (0 = off, 1 = on, 2 = PWAD only)"
  },

  {
    "stretchsky",
    (config_t *) &stretchsky, NULL,
    {0}, {0,1}, number, ss_gen, wad_no,
    "1 to stretch short skies"
  },

  {
    "linearsky",
    (config_t *) &linearsky, NULL,
    {0}, {0,1}, number, ss_gen, wad_no,
    "1 for linear horizontal sky scrolling "
  },

  { // phares
    "translucency",
    (config_t *) &translucency, NULL,
    {1}, {0,1}, number, ss_gen, wad_yes,
    "1 to enable translucency for some things"
  },

  { // killough 2/21/98
    "tran_filter_pct",
    (config_t *) &tran_filter_pct, NULL,
    {66}, {0,100}, number, ss_gen, wad_yes,
    "set percentage of foreground/background translucency mix"
  },

  {
    "r_swirl",
    (config_t *) &r_swirl, NULL,
    {0}, {0,1}, number, ss_gen, wad_yes,
    "1 to enable swirling animated flats"
  },

  {
    "smoothlight",
    (config_t *) &smoothlight, NULL,
    {0}, {0,1}, number, ss_gen, wad_yes,
    "1 to enable smooth diminishing lighting"
  },

  {
    "brightmaps",
    (config_t *) &brightmaps, NULL,
    {0}, {0,1}, number, ss_gen, wad_yes,
    "1 to enable brightmaps for textures and sprites"
  },

  {
    "extra_level_brightness",
    (config_t *) &extra_level_brightness, NULL,
    {0}, {0,4}, number, ss_gen, wad_no,
    "level brightness"
  },

  {
    "menu_background",
    (config_t *) &menu_background, NULL,
    {background_on}, {background_on,background_dark}, number, ss_gen, wad_no,
    "draw menu background (0 = on, 1 = off, 2 = dark)"
  },

  { // killough 10/98
    "flashing_hom",
    (config_t *) &flashing_hom, NULL,
    {1}, {0,1}, number, ss_none, wad_yes,
    "1 to enable flashing HOM indicator"
  },

  { // killough 2/21/98: default to 10
    "screenblocks",
    (config_t *) &screenblocks, NULL,
    {10}, {3,11}, number, ss_none, wad_no,
    "initial play screen size"
  },

  //
  // Sound and music
  //

  {
    "sfx_volume",
    (config_t *) &snd_SfxVolume, NULL,
    {8}, {0,15}, number, ss_none, wad_no,
    "adjust sound effects volume"
  },

  {
    "music_volume",
    (config_t *) &snd_MusicVolume, NULL,
    {8}, {0,15}, number, ss_none, wad_no,
    "adjust music volume"
  },

  { // killough 2/21/98
    "pitched_sounds",
    (config_t *) &pitched_sounds, NULL,
    {0}, {0,1}, number, ss_gen, wad_yes,
    "1 to enable variable pitch in sound effects (from id's original code)"
  },

  {
    "pitch_bend_range",
    (config_t *) &pitch_bend_range, NULL,
    {120}, {100,300}, number, ss_none, wad_yes,
    "variable pitch bend range (100 none, 120 default)"
  },

  // [FG] play sounds in full length
  {
    "full_sounds",
    (config_t *) &full_sounds, NULL,
    {0}, {0, 1}, number, ss_gen, wad_no,
    "1 to play sounds in full length"
  },

  {
    "force_flip_pan",
    (config_t *) &forceFlipPan, NULL,
    {0}, {0, 1}, number, ss_none, wad_no,
    "1 to force reversal of stereo audio channels"
  },

  { // killough
    "snd_channels",
    (config_t *) &default_numChannels, NULL,
    {MAX_CHANNELS}, {1, MAX_CHANNELS}, 0, ss_gen, wad_no,
    "number of sound effects handled simultaneously"
  },

  {
    "snd_resampler",
    (config_t *) &snd_resampler, NULL,
    {1}, {0, 2}, number, ss_gen, wad_no,
    "OpenAL resampler (0 = Nearest, 1 = Linear, 2 = Cubic)"
  },

  {
    "snd_module",
    (config_t *) &snd_module, NULL,
    {SND_MODULE_MBF}, {0, NUM_SND_MODULES - 1}, number, ss_gen, wad_no,
    "Sound module (0 = Standard, 1 = OpenAL 3D, 2 = PC Speaker Sound)"
  },

  {
    "snd_hrtf",
    (config_t *) &snd_hrtf, NULL,
    {0}, {0, 1}, number, ss_gen, wad_no,
    "[OpenAL 3D] Headphones mode (0 = No, 1 = Yes)"
  },

  {
    "snd_absorption",
    (config_t *) &snd_absorption, NULL,
    {0}, {0, 10}, number, ss_none, wad_no,
    "[OpenAL 3D] Air absorption effect (0 = Off, 10 = Max)"
  },

  {
    "snd_doppler",
    (config_t *) &snd_doppler, NULL,
    {0}, {0, 10}, number, ss_none, wad_no,
    "[OpenAL 3D] Doppler effect (0 = Off, 10 = Max)"
  },

  {
    "midi_player",
    (config_t *) &midi_player, NULL,
    {0}, {0, 2}, number, ss_none, wad_no,
    "MIDI Player backend (Native if available, FluidSynth if available, OPL Emulation)"
  },

  {
    "midi_player_menu",
    (config_t *) &midi_player_menu, NULL,
    {0}, {0, UL}, number, ss_none, wad_no,
    "MIDI Player menu index"
  },

#if defined(HAVE_FLUIDSYNTH)
  {
    "soundfont_dir",
    (config_t *) &soundfont_dir, NULL,
#if defined(_WIN32)
    {.s = "soundfonts"},
#else
    /* RedHat/Fedora/Arch */
    {.s = "/usr/share/soundfonts:"
    /* Debian/Ubuntu/OpenSUSE */
    "/usr/share/sounds/sf2:"
    "/usr/share/sounds/sf3:"
    /* AppImage */
    "../share/" PROJECT_SHORTNAME "/soundfonts"},
#endif
    {0}, string, ss_none, wad_no,
    "FluidSynth soundfont directories"
  },

  {
    "soundfont_path",
    (config_t *) &soundfont_path, NULL,
    {.s = ""}, {0}, string, ss_none, wad_no,
    "FluidSynth current soundfont path"
  },

  {
    "mus_chorus",
    (config_t *) &mus_chorus, NULL,
    {0}, {0, 1}, number, ss_none, wad_no,
    "1 to enable FluidSynth chorus"
  },

  {
    "mus_reverb",
    (config_t *) &mus_reverb, NULL,
    {0}, {0, 1}, number, ss_none, wad_no,
    "1 to enable FluidSynth reverb"
  },

  {
    "mus_gain",
    (config_t *) &mus_gain, NULL,
    {100}, {10, 1000}, number, ss_none, wad_no,
    "fine tune FluidSynth output level (default 100%)"
  },
#endif

  {
    "opl_gain",
    (config_t *) &opl_gain, NULL,
    {200}, {100, 1000}, number, ss_none, wad_no,
    "fine tune OPL emulation output level (default 200%)"
  },

#if defined(_WIN32)
  {
    "winmm_device",
    (config_t *) &winmm_device, NULL,
    {.s = ""}, {0}, string, ss_none, wad_no,
    "Native MIDI device"
  },

  {
    "winmm_complevel",
    (config_t *) &winmm_complevel, NULL,
    {1}, {0, 2}, number, ss_none, wad_no,
    "Native MIDI compatibility level (0 = Vanilla, 1 = Standard, 2 = Full)"
  },

  {
    "winmm_reset_type",
    (config_t *) &winmm_reset_type, NULL,
    {1}, {0, 3}, number, ss_none, wad_no,
    "SysEx reset for native MIDI (0 = None, 1 = GM, 2 = GS, 3 = XG)"
  },

  {
    "winmm_reset_delay",
    (config_t *) &winmm_reset_delay, NULL,
    {0}, {0, 2000}, number, ss_none, wad_no,
    "Delay after reset for native MIDI (milliseconds)"
  },
#endif

  //
  // QOL features
  //

  { // jff 3/24/98 allow default skill setting
    "default_skill",
    (config_t *) &defaultskill, NULL,
    {3}, {1,5}, number, ss_gen, wad_no,
    "selects default skill (1 = ITYTD, 2 = HNTR, 3 = HMP, 4 = UV, 5 = NM)"
  },

  { // killough 3/6/98: preserve autorun across games
    "autorun",
    (config_t *) &autorun, NULL,
    {1}, {0,1}, number, ss_none, wad_no,
    "1 to enable autorun"
  },

  {
    "realtic_clock_rate",
    (config_t *) &realtic_clock_rate, NULL,
    {100}, {10,1000}, number, ss_gen, wad_no,
    "Percentage of normal speed (35 fps) realtic clock runs at"
  },

  { // killough 2/8/98
    "max_player_corpse",
    (config_t *) &default_bodyquesize, NULL,
    {32}, {UL,UL},number, ss_none, wad_no,
    "number of dead bodies in view supported (negative value = no limit)"
  },

  {
    "death_use_action",
    (config_t *) &death_use_action, NULL,
    {0}, {0,2}, number, ss_gen, wad_no,
    "\"use\" button action on death (0 = default, 1 = load save, 2 = nothing)"
  },

  {
    "demobar",
    (config_t *) &demobar, NULL,
    {0}, {0,1}, number, ss_gen, wad_no,
    "1 to enable demo progress bar"
  },

  {
    "palette_changes",
    (config_t *) &palette_changes, NULL,
    {1}, {0,1}, number, ss_gen, wad_no,
    "0 to disable palette changes"
  },

  {
    "screen_melt",
    (config_t *) &screen_melt, NULL,
    {1}, {0,1}, number, ss_gen, wad_no,
    "0 to disable screen melt"
  },

  {
    "organize_savefiles",
    (config_t *) &organize_savefiles, NULL,
    {-1}, {-1,1}, number, ss_gen, wad_no,
    "1 to organize save files"
  },

  {
    "net_player_name",
    (config_t *) &net_player_name, NULL,
    {.s = DEFAULT_PLAYER_NAME}, {0}, string, ss_none, wad_no,
    "network setup player name"
  },

  //
  // Compatibility breaking features
  //

  {
    "autostrafe50",
    (config_t *) &autostrafe50, NULL,
    {0}, {0,1}, number, ss_comp, wad_no,
    "1 to enable auto strafe50"
  },

  {
    "strictmode",
    (config_t *) &default_strictmode, (config_t *) &strictmode,
    {0}, {0,1}, number, ss_comp, wad_no,
    "1 to enable strict mode"
  },

  {
    "hangsolid",
    (config_t *) &hangsolid, NULL,
    {0}, {0,1}, number, ss_comp, wad_no,
    "1 to walk under solid hanging bodies"
  },

  {
    "blockmapfix",
    (config_t *) &blockmapfix, NULL,
    {0}, {0,1}, number, ss_comp, wad_no,
    "1 to enable blockmap bug fix"
  },

  {
    "direct_vertical_aiming",
    (config_t *) &default_direct_vertical_aiming, (config_t *) &direct_vertical_aiming,
    {0}, {0,1}, number, ss_comp, wad_no,
    "1 to enable direct vertical aiming"
  },

  {
    "pistolstart",
    (config_t *) &default_pistolstart, (config_t *) &pistolstart,
    {0}, {0,1}, number, ss_comp, wad_no,
    "1 to enable pistol start"
  },

  //
  // Weapons options
  //

  { // phares
    "weapon_recoil",
    (config_t *) &default_weapon_recoil, (config_t *) &weapon_recoil,
    {0}, {0,1}, number, ss_weap, wad_yes,
    "1 to enable recoil from weapon fire"
  },

  {
    "weapon_recoilpitch",
    (config_t *) &weapon_recoilpitch, NULL,
    {0}, {0,1}, number, ss_weap, wad_yes,
    "1 to enable recoil pitch from weapon fire"
  },

  { // killough 7/19/98
    "classic_bfg",
    (config_t *) &default_classic_bfg, (config_t *) &classic_bfg,
    {0}, {0,1}, number, ss_weap, wad_yes,
    "1 to enable pre-beta BFG2704"
  },

  { // killough 10/98
    "doom_weapon_toggles",
    (config_t *) &doom_weapon_toggles, NULL,
    {1}, {0,1}, number, ss_weap, wad_no,
    "1 to toggle between SG/SSG and Fist/Chainsaw"
  },

  { // phares 2/25/98
    "player_bobbing",
    (config_t *) &default_player_bobbing, (config_t *) &player_bobbing,
    {1}, {0,1}, number, ss_weap, wad_no,
    "1 to enable player bobbing (view moving up/down slightly)"
  },

  {
    "cosmetic_bobbing",
    (config_t *) &cosmetic_bobbing, NULL,
    {1}, {0,2}, number, ss_weap, wad_no,
    "Player View/Weapon Bobbing (0 = off, 1 = full, 2 = 75%)"
  },

  {
    "hide_weapon",
    (config_t *) &hide_weapon, NULL,
    {0}, {0,1}, number, ss_weap, wad_no,
    "1 to hide weapon"
  },

  // [FG] centered or bobbing weapon sprite
  {
    "center_weapon",
    (config_t *) &center_weapon, NULL,
    {0}, {0,2}, number, ss_weap, wad_no,
    "1 to center the weapon sprite during attack, 2 to keep it bobbing"
  },

  {  // killough 2/8/98: weapon preferences set by user:
    "weapon_choice_1",
    (config_t *) &weapon_preferences[0][0], NULL,
    {6}, {1,9}, number, ss_weap, wad_yes,
    "first choice for weapon (best)"
  },

  {
    "weapon_choice_2",
    (config_t *) &weapon_preferences[0][1], NULL,
    {9}, {1,9}, number, ss_weap, wad_yes,
    "second choice for weapon"
  },

  {
    "weapon_choice_3",
    (config_t *) &weapon_preferences[0][2], NULL,
    {4}, {1,9}, number, ss_weap, wad_yes,
    "third choice for weapon"
  },

  {
    "weapon_choice_4",
    (config_t *) &weapon_preferences[0][3], NULL,
    {3}, {1,9}, number, ss_weap, wad_yes,
    "fourth choice for weapon"
  },

  {
    "weapon_choice_5",
    (config_t *) &weapon_preferences[0][4], NULL,
    {2}, {1,9}, number, ss_weap, wad_yes,
    "fifth choice for weapon"
  },

  {
    "weapon_choice_6",
    (config_t *) &weapon_preferences[0][5], NULL,
    {8}, {1,9}, number, ss_weap, wad_yes,
    "sixth choice for weapon"
  },

  {
    "weapon_choice_7",
    (config_t *) &weapon_preferences[0][6], NULL,
    {5}, {1,9}, number, ss_weap, wad_yes,
    "seventh choice for weapon "
  },

  {
    "weapon_choice_8",
    (config_t *) &weapon_preferences[0][7], NULL,
    {7}, {1,9}, number, ss_weap, wad_yes,
    "eighth choice for weapon"
  },

  {
    "weapon_choice_9",
    (config_t *) &weapon_preferences[0][8], NULL,
    {1}, {1,9}, number, ss_weap, wad_yes,
    "ninth choice for weapon (worst)"
  },

  //
  // Enemies
  //

  { // killough 3/1/98
    "monsters_remember",
    (config_t *) &default_monsters_remember, (config_t *) &monsters_remember,
    {1}, {0,1}, number, ss_none, wad_yes,
    "1 to enable monsters remembering enemies after killing others"
  },

  { // killough 7/19/98
    "monster_infighting",
    (config_t *) &default_monster_infighting, (config_t *) &monster_infighting,
    {1}, {0,1}, number, ss_none, wad_yes,
    "1 to enable monsters fighting against each other when provoked"
  },

  { // killough 9/8/98
    "monster_backing",
    (config_t *) &default_monster_backing, (config_t *) &monster_backing,
    {0}, {0,1}, number, ss_none, wad_yes,
    "1 to enable monsters backing away from targets"
  },

  { //killough 9/9/98:
    "monster_avoid_hazards",
    (config_t *) &default_monster_avoid_hazards, (config_t *) &monster_avoid_hazards,
    {1}, {0,1}, number, ss_none, wad_yes,
    "1 to enable monsters to intelligently avoid hazards"
  },

  {
    "monkeys",
    (config_t *) &default_monkeys, (config_t *) &monkeys,
    {0}, {0,1}, number, ss_none, wad_yes,
    "1 to enable monsters to move up/down steep stairs"
  },

  { //killough 9/9/98:
    "monster_friction",
    (config_t *) &default_monster_friction, (config_t *) &monster_friction,
    {1}, {0,1}, number, ss_none, wad_yes,
    "1 to enable monsters to be affected by friction"
  },

  { //killough 9/9/98:
    "help_friends",
    (config_t *) &default_help_friends, (config_t *) &help_friends,
    {0}, {0,1}, number, ss_none, wad_yes,
    "1 to enable monsters to help dying friends"
  },

  { // killough 7/19/98
    "player_helpers",
    (config_t *) &default_dogs, (config_t *) &dogs,
    {0}, {0,3}, number, ss_none, wad_yes,
    "number of single-player helpers"
  },

  { // killough 8/8/98
    "friend_distance",
    (config_t *) &default_distfriend, (config_t *) &distfriend,
    {128}, {0,999}, number, ss_none, wad_yes,
    "distance friends stay away"
  },

  { // killough 10/98
    "dog_jumping",
    (config_t *) &default_dog_jumping, (config_t *) &dog_jumping,
    {1}, {0,1}, number, ss_none, wad_yes,
    "1 to enable dogs to jump"
  },

  {
    "colored_blood",
    (config_t *) &colored_blood, NULL,
    {0}, {0,1}, number, ss_enem, wad_no,
    "1 to enable colored blood"
  },

  {
    "flipcorpses",
    (config_t *) &flipcorpses, NULL,
    {0}, {0,1}, number, ss_enem, wad_no,
    "1 to enable randomly mirrored death animations"
  },

  {
    "ghost_monsters",
    (config_t *) &ghost_monsters, NULL,
    {1}, {0,1}, number, ss_enem, wad_no,
    "1 to enable \"ghost monsters\" (resurrected pools of gore are translucent)"
  },

  // [FG] spectre drawing mode
  {
    "fuzzcolumn_mode",
    (config_t *) &fuzzcolumn_mode, NULL,
    {1}, {0,1}, number, ss_enem, wad_no,
    "0 original, 1 blocky"
  },

  //
  // Compatibility
  //

  // killough 10/98: compatibility vector:

  {
    "comp_zombie",
    (config_t *) &default_comp[comp_zombie], (config_t *) &comp[comp_zombie],
    {1}, {0,1}, number, ss_none, wad_yes,
    "Zombie players can exit levels"
  },

  {
    "comp_infcheat",
    (config_t *) &default_comp[comp_infcheat], (config_t *) &comp[comp_infcheat],
    {0}, {0,1}, number, ss_none, wad_yes,
    "Powerup cheats are not infinite duration"
  },

  {
    "comp_stairs",
    (config_t *) &default_comp[comp_stairs], (config_t *) &comp[comp_stairs],
    {0}, {0,1}, number, ss_none, wad_yes,
    "Build stairs exactly the same way that Doom does"
  },

  {
    "comp_telefrag",
    (config_t *) &default_comp[comp_telefrag], (config_t *) &comp[comp_telefrag],
    {0}, {0,1}, number, ss_none, wad_yes,
    "Monsters can telefrag on MAP30"
  },

  {
    "comp_dropoff",
    (config_t *) &default_comp[comp_dropoff], (config_t *) &comp[comp_dropoff],
    {0}, {0,1}, number, ss_none, wad_yes,
    "Some objects never move over tall ledges"
  },

  {
    "comp_falloff",
    (config_t *) &default_comp[comp_falloff], (config_t *) &comp[comp_falloff],
    {0}, {0,1}, number, ss_none, wad_yes,
    "Objects don't fall off ledges under their own weight"
  },

  {
    "comp_staylift",
    (config_t *) &default_comp[comp_staylift], (config_t *) &comp[comp_staylift],
    {0}, {0,1}, number, ss_none, wad_yes,
    "Monsters randomly walk off of moving lifts"
  },

  {
    "comp_doorstuck",
    (config_t *) &default_comp[comp_doorstuck], (config_t *) &comp[comp_doorstuck],
    {0}, {0,1}, number, ss_none, wad_yes,
    "Monsters get stuck on doortracks"
  },

  {
    "comp_pursuit",
    (config_t *) &default_comp[comp_pursuit], (config_t *) &comp[comp_pursuit],
    {1}, {0,1}, number, ss_none, wad_yes,
    "Monsters don't give up pursuit of targets"
  },

  {
    "comp_vile",
    (config_t *) &default_comp[comp_vile], (config_t *) &comp[comp_vile],
    {0}, {0,1}, number, ss_none, wad_yes,
    "Arch-Vile resurrects invincible ghosts"
  },

  {
    "comp_pain",
    (config_t *) &default_comp[comp_pain], (config_t *) &comp[comp_pain],
    {0}, {0,1}, number, ss_none, wad_yes,
    "Pain Elemental limited to 20 lost souls"
  },

  {
    "comp_skull",
    (config_t *) &default_comp[comp_skull], (config_t *) &comp[comp_skull],
    {0}, {0,1}, number, ss_none, wad_yes,
    "Lost souls get stuck behind walls"
  },

  {
    "comp_blazing",
    (config_t *) &default_comp[comp_blazing], (config_t *) &comp[comp_blazing],
    {0}, {0,1}, number, ss_none, wad_yes,
    "Blazing doors make double closing sounds"
  },

  {
    "comp_doorlight",
    (config_t *) &default_comp[comp_doorlight], (config_t *) &comp[comp_doorlight],
    {0}, {0,1}, number, ss_none, wad_yes,
    "Tagged doors don't trigger special lighting"
  },

  {
    "comp_god",
    (config_t *) &default_comp[comp_god], (config_t *) &comp[comp_god],
    {0}, {0,1}, number, ss_none, wad_yes,
    "God mode isn't absolute"
  },

  {
    "comp_skymap",
    (config_t *) &default_comp[comp_skymap], (config_t *) &comp[comp_skymap],
    {0}, {0,1}, number, ss_none, wad_yes,
    "Sky is unaffected by invulnerability"
  },

  {
    "comp_floors",
    (config_t *) &default_comp[comp_floors], (config_t *) &comp[comp_floors],
    {0}, {0,1}, number, ss_none, wad_yes,
    "Use exactly Doom's floor motion behavior"
  },

  {
    "comp_model",
    (config_t *) &default_comp[comp_model], (config_t *) &comp[comp_model],
    {0}, {0,1}, number, ss_none, wad_yes,
    "Use exactly Doom's linedef trigger model"
  },

  {
    "comp_zerotags",
    (config_t *) &default_comp[comp_zerotags], (config_t *) &comp[comp_zerotags],
    {0}, {0,1}, number, ss_none, wad_yes,
    "Linedef effects work with sector tag = 0"
  },

  {
    "comp_respawn",
    (config_t *) &default_comp[comp_respawn], (config_t *) &comp[comp_respawn],
    {0}, {0,1}, number, ss_none, wad_yes,
    "Creatures with no spawnpoint respawn at (0,0)"
  },

  {
    "comp_ledgeblock",
    (config_t *) &default_comp[comp_ledgeblock], (config_t *) &comp[comp_ledgeblock],
    {1}, {0,1}, number, ss_none, wad_yes,
    "Ledges block ground enemies"
  },

  {
    "comp_friendlyspawn",
    (config_t *) &default_comp[comp_friendlyspawn], (config_t *) &comp[comp_friendlyspawn],
    {1}, {0,1}, number, ss_none, wad_yes,
    "A_Spawn new thing inherits friendliness"
  },

  {
    "comp_voodooscroller",
    (config_t *) &default_comp[comp_voodooscroller], (config_t *) &comp[comp_voodooscroller],
    {0}, {0,1}, number, ss_none, wad_yes,
    "Voodoo dolls on slow scrollers move too slowly"
  },

  {
    "comp_reservedlineflag",
    (config_t *) &default_comp[comp_reservedlineflag], (config_t *) &comp[comp_reservedlineflag],
    {1}, {0,1}, number, ss_none, wad_yes,
    "ML_RESERVED clears extended flags"
  },

  // [FG] overflow emulation

  {
    "emu_spechits",
    (config_t *) &overflow[emu_spechits].enabled, NULL,
    {1}, {0,1}, number, ss_none, wad_no,
    "1 to enable SPECHITS overflow emulation"
  },

  {
    "emu_reject",
    (config_t *) &overflow[emu_reject].enabled, NULL,
    {1}, {0,1}, number, ss_none, wad_no,
    "1 to enable REJECT overflow emulation"
  },

  {
    "emu_intercepts",
    (config_t *) &overflow[emu_intercepts].enabled, NULL,
    {1}, {0,1}, number, ss_none, wad_no,
    "1 to enable INTERCEPTS overflow emulation"
  },

  {
    "emu_missedbackside",
    (config_t *) &overflow[emu_missedbackside].enabled, NULL,
    {0}, {0,1}, number, ss_none, wad_no,
    "1 to enable missed backside emulation"
  },

  {
    "emu_donut",
    (config_t *) &overflow[emu_donut].enabled, NULL,
    {1}, {0,1}, number, ss_none, wad_no,
    "1 to enable donut overrun emulation"
  },

  // default compatibility
  {
    "default_complevel",
    (config_t *) &default_complevel, NULL,
    {3}, {0,3}, number, ss_comp, wad_no,
    "0 Vanilla, 1 Boom, 2 MBF, 3 MBF21"
  },


  { // killough 4/17/98
    "traditional_menu",
    (config_t *) &traditional_menu, NULL,
    {1}, {0,1}, number, ss_none, wad_yes,
    "1 to use Doom's main menu ordering"
  },

  //
  // Controls
  //

  {
    "input_turnright",
    NULL, NULL,
    {0}, {UL,UL}, input, ss_keys, wad_no,
    "key to turn right",
    input_turnright, { {INPUT_KEY, KEY_RIGHTARROW} }
  },

  {
    "input_turnleft",
    NULL, NULL,
    {0}, {UL,UL}, input, ss_keys, wad_no,
    "key to turn left",
    input_turnleft, { {INPUT_KEY, KEY_LEFTARROW} }
  },

  {
    "input_forward",
    NULL, NULL,
    {0}, {UL,UL}, input, ss_keys, wad_no,
    "key to move forward",
    input_forward, { {INPUT_KEY, 'w'}, {INPUT_KEY, KEY_UPARROW} }
  },

  {
    "input_backward",
    NULL, NULL,
    {0}, {UL,UL}, input, ss_keys, wad_no,
    "key to move backward",
    input_backward, { {INPUT_KEY, 's'}, {INPUT_KEY, KEY_DOWNARROW} }
  },

  {
    "input_mouselook",
    NULL, NULL,
    {0}, {UL,UL}, input, ss_keys, wad_no,
    "key to toggle mouselook",
    input_mouselook, { {0, 0} }
  },

  // [FG] reload current level / go to next level
  {
    "input_menu_reloadlevel",
    NULL, NULL,
    {0}, {UL,UL}, input, ss_keys, wad_no,
    "key to restart current level",
    input_menu_reloadlevel, { {0, 0} }
  },

  {
    "input_menu_nextlevel",
    NULL, NULL,
    {0}, {UL,UL}, input, ss_keys, wad_no,
    "key to go to next level",
    input_menu_nextlevel, { {0, 0} }
  },

  {
    "input_hud_timestats",
    NULL, NULL,
    {0}, {UL,UL}, input, ss_keys, wad_no,
    "key to show level stats and time",
    input_hud_timestats, { {0, 0} }
  },

  {
    "input_demo_quit",
    NULL, NULL,
    {0}, {UL,UL}, input, ss_keys, wad_no,
    "key to finish recording demo",
    input_demo_quit, { {0, 0} }
  },

  {
    "input_demo_join",
    NULL, NULL,
    {0}, {UL,UL}, input, ss_keys, wad_no,
    "key to continue recording current demo",
    input_demo_join, { {0, 0} }
  },

  {
    "input_demo_fforward",
    NULL, NULL,
    {0}, {UL,UL}, input, ss_keys, wad_no,
    "key for fast-forward demo",
    input_demo_fforward, { {0, 0} }
  },

  {
    "input_speed_up",
    NULL, NULL,
    {0}, {UL,UL}, input, ss_keys, wad_no,
    "key to increase game speed",
    input_speed_up, { {0, 0} }
  },

  {
    "input_speed_down",
    NULL, NULL,
    {0}, {UL,UL}, input, ss_keys, wad_no,
    "key to decrease game speed",
    input_speed_down, { {0, 0} }
  },

  {
    "input_speed_default",
    NULL, NULL,
    {0}, {UL,UL}, input, ss_keys, wad_no,
    "key to reset game speed",
    input_speed_default, { {0, 0} }
  },

  {
    "input_strafeleft",
    NULL, NULL,
    {0}, {UL,UL}, input, ss_keys, wad_no,
    "key to strafe left (sideways left)",
    input_strafeleft, { {INPUT_KEY, 'a'} }
  },

  {
    "input_straferight",
    NULL, NULL,
    {0}, {UL,UL}, input, ss_keys, wad_no,
    "key to strafe right (sideways right)",
    input_straferight, { {INPUT_KEY, 'd'} }
  },

  {
    "input_fire",
    NULL, NULL,
    {0}, {UL,UL}, input, ss_keys, wad_no,
    "key to fire current weapon",
    input_fire, { {INPUT_KEY, KEY_RCTRL},
                  {INPUT_MOUSEB, MOUSE_BUTTON_LEFT},
                  {INPUT_JOYB, CONTROLLER_RIGHT_TRIGGER} }
  },

  {
    "input_use",
    NULL, NULL,
    {0}, {UL,UL}, input, ss_keys, wad_no,
    "key to open a door, use a switch",
    input_use, { {INPUT_KEY,' '},
                 {INPUT_JOYB, CONTROLLER_A} }
  },

  {
    "input_strafe",
    NULL, NULL,
    {0}, {UL,UL}, input, ss_keys, wad_no,
    "key to use with arrows to strafe",
    input_strafe, { {INPUT_KEY, KEY_RALT},
                    {INPUT_MOUSEB, MOUSE_BUTTON_RIGHT} }
  },

  {
    "input_speed",
    NULL, NULL,
    {0}, {UL,UL}, input, ss_keys, wad_no,
    "key to run (move fast)",
    input_speed, { {INPUT_KEY, KEY_RSHIFT} }
  },

  {
    "input_savegame",
    NULL, NULL,
    {0}, {UL,UL}, input, ss_keys, wad_no,
    "key to save current game",
    input_savegame, { {INPUT_KEY, KEY_F2} }
  },

  {
    "input_loadgame",
    NULL, NULL,
    {0}, {UL,UL}, input, ss_keys, wad_no,
    "key to restore from saved games",
    input_loadgame, { {INPUT_KEY, KEY_F3} }
  },

  {
    "input_soundvolume",
    NULL, NULL,
    {0}, {UL,UL}, input, ss_keys, wad_no,
    "key to bring up sound control panel",
    input_soundvolume, { {INPUT_KEY, KEY_F4} }
  },

  {
    "input_hud",
    NULL, NULL,
    {0}, {UL,UL}, input, ss_keys, wad_no,
    "key to adjust heads up display mode",
    input_hud, { {INPUT_KEY, KEY_F5} }
  },

  {
    "input_quicksave",
    NULL, NULL,
    {0}, {UL,UL}, input, ss_keys, wad_no,
    "key to to save to last slot saved",
    input_quicksave, { {INPUT_KEY, KEY_F6} }
  },

  {
    "input_endgame",
    NULL, NULL,
    {0}, {UL,UL}, input, ss_keys, wad_no,
    "key to end the game",
    input_endgame, { {INPUT_KEY, KEY_F7} }
  },

  {
    "input_messages",
    NULL, NULL,
    {0}, {UL,UL}, input, ss_keys, wad_no,
    "key to toggle message enable",
    input_messages, { {INPUT_KEY, KEY_F8} }
  },

  {
    "input_quickload",
    NULL, NULL,
    {0}, {UL,UL}, input, ss_keys, wad_no,
    "key to load from quick saved game",
    input_quickload, { {INPUT_KEY, KEY_F9} }
  },

  {
    "input_quit",
    NULL, NULL,
    {0}, {UL,UL}, input, ss_keys, wad_no,
    "key to quit game to DOS",
    input_quit, { {INPUT_KEY, KEY_F10} }
  },

  {
    "input_gamma",
    NULL, NULL,
    {0}, {UL,UL}, input, ss_keys, wad_no,
    "key to adjust screen brightness (gamma correction)",
    input_gamma, { {INPUT_KEY, KEY_F11} }
  },

  {
    "input_spy",
    NULL, NULL,
    {0}, {UL,UL}, input, ss_keys, wad_no,
    "key to view from another player's vantage",
    input_spy, { {INPUT_KEY, KEY_F12} }
  },

  {
    "input_pause",
    NULL, NULL,
    {0}, {UL,UL}, input, ss_keys, wad_no,
    "key to pause the game",
    input_pause, { {INPUT_KEY, KEY_PAUSE} }
  },

  {
    "input_autorun",
    NULL, NULL,
    {0}, {UL,UL}, input, ss_keys, wad_no,
    "key to toggle always run mode",
    input_autorun, { {INPUT_KEY, KEY_CAPSLOCK},
                     {INPUT_JOYB, CONTROLLER_LEFT_STICK} }
  },

  {
    "input_novert",
    NULL, NULL,
    {0}, {UL,UL}, input, ss_keys, wad_no,
    "key to toggle vertical mouse movement",
    input_novert, { {0, 0} }
  },

  {
    "input_chat",
    NULL, NULL,
    {0}, {UL,UL}, input, ss_keys, wad_no,
    "key to enter a chat message",
    input_chat, { {INPUT_KEY, 't'} }
  },

  {
    "input_chat_backspace",
    NULL, NULL,
    {0}, {UL,UL}, input, ss_keys, wad_no,
    "key to erase last character typed",
    input_chat_backspace, { {INPUT_KEY, KEY_BACKSPACE} }
  },

  {
    "input_chat_enter",
    NULL, NULL,
    {0}, {UL,UL}, input, ss_keys, wad_no,
    "key to select from menu or review past messages",
    input_chat_enter, { {INPUT_KEY, KEY_ENTER} }
  },

  {
    "input_map",
    NULL, NULL,
    {0}, {UL,UL}, input, ss_keys, wad_no,
    "key to toggle automap display",
    input_map, { {INPUT_KEY, KEY_TAB},
                 {INPUT_JOYB, CONTROLLER_Y} }
  },

  { // phares 3/7/98
    "input_map_right",
    NULL, NULL,
    {0}, {UL,UL}, input, ss_keys, wad_no,
    "key to shift automap right",
    input_map_right, { {INPUT_KEY, KEY_RIGHTARROW} }
  },

  {
    "input_map_left",
    NULL, NULL,
    {0}, {UL,UL}, input, ss_keys, wad_no,
    "key to shift automap left",
    input_map_left, { {INPUT_KEY, KEY_LEFTARROW} }
  },

  {
    "input_map_up",
    NULL, NULL,
    {0}, {UL,UL}, input, ss_keys, wad_no,
    "key to shift automap up",
    input_map_up, { {INPUT_KEY, KEY_UPARROW} }
  },

  {
    "input_map_down",
    NULL, NULL,
    {0}, {UL,UL}, input, ss_keys, wad_no,
    "key to shift automap down",
    input_map_down, { {INPUT_KEY, KEY_DOWNARROW} }
  },

  {
    "input_map_zoomin",
    NULL, NULL,
    {0}, {UL,UL}, input, ss_keys, wad_no,
    "key to enlarge automap",
    input_map_zoomin, { {INPUT_KEY, '='},
                        {INPUT_MOUSEB, MOUSE_BUTTON_WHEELUP} }
  },

  {
    "input_map_zoomout",
    NULL, NULL,
    {0}, {UL,UL}, input, ss_keys, wad_no,
    "key to reduce automap",
    input_map_zoomout, { {INPUT_KEY, '-'},
                         {INPUT_MOUSEB, MOUSE_BUTTON_WHEELDOWN} }
  },

  {
    "input_map_gobig",
    NULL, NULL,
    {0}, {UL,UL}, input, ss_keys, wad_no,
    "key to get max zoom for automap",
    input_map_gobig, { {INPUT_KEY, '0'} }
  },

  {
    "input_map_follow",
    NULL, NULL,
    {0}, {UL,UL}, input, ss_keys, wad_no,
    "key to toggle scrolling/moving with automap",
    input_map_follow, { {INPUT_KEY, 'f'} }
  },

  {
    "input_map_mark",
    NULL, NULL,
    {0}, {UL,UL}, input, ss_keys, wad_no,
    "key to drop a marker on automap",
    input_map_mark, { {INPUT_KEY, 'm'} }
  },

  {
    "input_map_clear",
    NULL, NULL,
    {0}, {UL,UL}, input, ss_keys, wad_no,
    "key to clear all markers on automap",
    input_map_clear, { {INPUT_KEY, 'c'} }
  },

  {
    "input_map_grid",
    NULL, NULL,
    {0}, {UL,UL}, input, ss_keys, wad_no,
    "key to toggle grid display over automap",
    input_map_grid, { {INPUT_KEY, 'g'} }
  },

  {
    "input_map_overlay",
    NULL, NULL,
    {0}, {UL,UL}, input, ss_keys, wad_no,
    "key to toggle overlay mode",
    input_map_overlay, { {INPUT_KEY, 'o'} }
  },

  {
    "input_map_rotate",
    NULL, NULL,
    {0}, {UL,UL}, input, ss_keys, wad_no,
    "key to toggle rotate mode",
    input_map_rotate, { {INPUT_KEY, 'r'} }
  },

  {
    "input_reverse",
    NULL, NULL,
    {0}, {UL,UL}, input, ss_keys, wad_no,
    "key to spin 180 instantly",
    input_reverse, { {INPUT_KEY, '/'} }
  },

  {
    "input_zoomin",
    NULL, NULL,
    {0}, {UL,UL}, input, ss_keys, wad_no,
    "key to enlarge display",
    input_zoomin, { {INPUT_KEY, '='} }
  },

  {
    "input_zoomout",
    NULL, NULL,
    {0}, {UL,UL}, input, ss_keys, wad_no,
    "key to reduce display",
    input_zoomout,  { {INPUT_KEY, '-'} }
  },

  {
    "input_iddqd",
    NULL, NULL,
    {0}, {UL,UL}, input, ss_keys, wad_no,
    "key to toggle god mode",
    input_iddqd, { {0, 0} }
  },

  {
    "input_idkfa",
    NULL, NULL,
    {0}, {UL,UL}, input, ss_keys, wad_no,
    "key to give ammo/keys",
    input_idkfa, { {0, 0} }
  },

  {
    "input_idfa",
    NULL, NULL,
    {0}, {UL,UL}, input, ss_keys, wad_no,
    "key to give ammo",
    input_idfa, { {0, 0} }
  },

  {
    "input_idclip",
    NULL, NULL,
    {0}, {UL,UL}, input, ss_keys, wad_no,
    "key to toggle no clipping mode",
    input_idclip, { {0, 0} }
  },

  {
    "input_idbeholdh",
    NULL, NULL,
    {0}, {UL,UL}, input, ss_keys, wad_no,
    "key to give health",
    input_idbeholdh, { {0, 0} }
  },

  {
    "input_idbeholdm",
    NULL, NULL,
    {0}, {UL,UL}, input, ss_keys, wad_no,
    "key to give mega armor",
    input_idbeholdm, { {0, 0} }
  },

  {
    "input_idbeholdv",
    NULL, NULL,
    {0}, {UL,UL}, input, ss_keys, wad_no,
    "key to give invulnerability",
    input_idbeholdv, { {0, 0} }
  },

  {
    "input_idbeholds",
    NULL, NULL,
    {0}, {UL,UL}, input, ss_keys, wad_no,
    "key to give berserk",
    input_idbeholds, { {0, 0} }
  },

  {
    "input_idbeholdv",
    NULL, NULL,
    {0}, {UL,UL}, input, ss_keys, wad_no,
    "key to give invulnerability",
    input_idbeholdv, { {0, 0} }
  },

  {
    "input_idbeholdi",
    NULL, NULL,
    {0}, {UL,UL}, input, ss_keys, wad_no,
    "key to give partial invisibility",
    input_idbeholdi, { {0, 0} }
  },

  {
    "input_idbeholdr",
    NULL, NULL,
    {0}, {UL,UL}, input, ss_keys, wad_no,
    "key to give radiation suit",
    input_idbeholdr, { {0, 0} }
  },

  {
    "input_idbeholdl",
    NULL, NULL,
    {0}, {UL,UL}, input, ss_keys, wad_no,
    "key to give light amplification",
    input_idbeholdl, { {0, 0} }
  },

  {
    "input_iddt",
    NULL, NULL,
    {0}, {UL,UL}, input, ss_keys, wad_no,
    "key to reveal map",
    input_iddt, { {0, 0} }
  },

  {
    "input_notarget",
    NULL, NULL,
    {0}, {UL,UL}, input, ss_keys, wad_no,
    "key to enable no target mode",
    input_notarget, { {0, 0} }
  },

  {
    "input_freeze",
    NULL, NULL,
    {0}, {UL,UL}, input, ss_keys, wad_no,
    "key to enable freeze mode",
    input_freeze, { {0, 0} }
  },

  {
    "input_avj",
    NULL, NULL,
    {0}, {UL,UL}, input, ss_keys, wad_no,
    "key to perform Fake Archvile Jump",
    input_avj, { {0, 0} }
  },

  {
    "input_chat_dest0",
    NULL, NULL,
    {0}, {UL,UL}, input, ss_keys, wad_no,
    "key to chat with player 1",
    input_chat_dest0, { {INPUT_KEY, 'g'} }
  },

  { // killough 11/98: fix 'i'/'b' reversal
    "input_chat_dest1",
    NULL, NULL,
    {0}, {UL,UL}, input, ss_keys, wad_no,
    "key to chat with player 2",
    input_chat_dest1, { {INPUT_KEY, 'i'} }
  },

  {  // killough 11/98: fix 'i'/'b' reversal
    "input_chat_dest2",
    NULL, NULL,
    {0}, {UL,UL}, input, ss_keys, wad_no,
    "key to chat with player 3",
    input_chat_dest2, { {INPUT_KEY, 'b'} }
  },

  {
    "input_chat_dest3",
    NULL, NULL,
    {0}, {UL,UL}, input, ss_keys, wad_no,
    "key to chat with player 4",
    input_chat_dest3, { {INPUT_KEY, 'r'} }
  },

  {
    "input_weapontoggle",
    NULL, NULL,
    {0}, {UL,UL}, input, ss_keys, wad_no,
    "key to toggle between two most preferred weapons with ammo",
    input_weapontoggle, { {INPUT_KEY, '0'} }
  },

  // [FG] prev/next weapon keys and buttons
  {
    "input_prevweapon",
    NULL, NULL,
    {0}, {UL,UL}, input, ss_keys, wad_no,
    "key to cycle to the previous weapon",
    input_prevweapon, { {INPUT_MOUSEB, MOUSE_BUTTON_WHEELDOWN},
                        {INPUT_JOYB, CONTROLLER_LEFT_SHOULDER} }
  },

  {
    "input_nextweapon",
    NULL, NULL,
    {0}, {UL,UL}, input, ss_keys, wad_no,
    "key to cycle to the next weapon",
    input_nextweapon, { {INPUT_MOUSEB, MOUSE_BUTTON_WHEELUP},
                        {INPUT_JOYB, CONTROLLER_RIGHT_SHOULDER} }
  },

  {
    "input_weapon1",
    NULL, NULL,
    {0}, {UL,UL}, input, ss_keys, wad_no,
    "key to switch to weapon 1 (fist/chainsaw)",
    input_weapon1, { {INPUT_KEY, '1'} }
  },

  {
    "input_weapon2",
    NULL, NULL,
    {0}, {UL,UL}, input, ss_keys, wad_no,
    "key to switch to weapon 2 (pistol)",
    input_weapon2, { {INPUT_KEY, '2'} }
  },

  {
    "input_weapon3",
    NULL, NULL,
    {0}, {UL,UL}, input, ss_keys, wad_no,
    "key to switch to weapon 3 (supershotgun/shotgun)",
    input_weapon3, { {INPUT_KEY, '3'} }
  },

  {
    "input_weapon4",
    NULL, NULL,
    {0}, {UL,UL}, input, ss_keys, wad_no,
    "key to switch to weapon 4 (chaingun)",
    input_weapon4, { {INPUT_KEY, '4'} }
  },

  {
    "input_weapon5",
    NULL, NULL,
    {0}, {UL,UL}, input, ss_keys, wad_no,
    "key to switch to weapon 5 (rocket launcher)",
    input_weapon5, { {INPUT_KEY, '5'} }
  },

  {
    "input_weapon6",
    NULL, NULL,
    {0}, {UL,UL}, input, ss_keys, wad_no,
    "key to switch to weapon 6 (plasma rifle)",
    input_weapon6, { {INPUT_KEY, '6'} }
  },

  {
    "input_weapon7",
    NULL, NULL,
    {0}, {UL,UL}, input, ss_keys, wad_no,
    "key to switch to weapon 7 (bfg9000)",
    input_weapon7, { {INPUT_KEY, '7'} }
  },

  {
    "input_weapon8",
    NULL, NULL,
    {0}, {UL,UL}, input, ss_keys, wad_no,
    "key to switch to weapon 8 (chainsaw)",
    input_weapon8, { {INPUT_KEY, '8'} }
  },

  {
    "input_weapon9",
    NULL, NULL,
    {0}, {UL,UL}, input, ss_keys, wad_no,
    "key to switch to weapon 9 (supershotgun)",
    input_weapon9, { {INPUT_KEY, '9'} }
  }, // phares

  { // killough 2/22/98: screenshot key
    "input_screenshot",
    NULL, NULL,
    {0}, {UL,UL}, input, ss_keys, wad_no,
    "key to take a screenshot (devparm independent)",
    input_screenshot, { {INPUT_KEY, KEY_PRTSCR} }
  },

  {
    "input_clean_screenshot",
    NULL, NULL,
    {0}, {UL,UL}, input, ss_keys, wad_no,
    "key to take a clean screenshot",
    input_clean_screenshot, { {0, 0} }
  },

  {
    "joy_enable",
    (config_t *) &joy_enable, NULL,
    {1}, {0, 1}, number, ss_gen, wad_no,
    "Enable game controller"
  },

  {
    "joy_layout",
    (config_t *) &joy_layout, NULL,
    {LAYOUT_DEFAULT}, {0, NUM_LAYOUTS - 1}, number, ss_gen, wad_no,
    "Analog stick layout (0 = Default, 1 = Swap, 2 = Legacy, 3 = Legacy Swap)"
  },

  {
    "joy_sensitivity_forward",
    (config_t *) &joy_sensitivity_forward, NULL,
    {50}, {0, 100}, number, ss_gen, wad_no,
    "Forward axis sensitivity"
  },

  {
    "joy_sensitivity_strafe",
    (config_t *) &joy_sensitivity_strafe, NULL,
    {50}, {0, 100}, number, ss_gen, wad_no,
    "Strafe axis sensitivity"
  },

  {
    "joy_sensitivity_turn",
    (config_t *) &joy_sensitivity_turn, NULL,
    {36}, {0, 100}, number, ss_gen, wad_no,
    "Turn axis sensitivity"
  },

  {
    "joy_sensitivity_look",
    (config_t *) &joy_sensitivity_look, NULL,
    {28}, {0, 100}, number, ss_gen, wad_no,
    "Look axis sensitivity"
  },

  {
    "joy_extra_sensitivity_turn",
    (config_t *) &joy_extra_sensitivity_turn, NULL,
    {14}, {0, 100}, number, ss_gen, wad_no,
    "Extra turn sensitivity at outer threshold (joy_threshold_camera)"
  },

  {
    "joy_extra_sensitivity_look",
    (config_t *) &joy_extra_sensitivity_look, NULL,
    {0}, {0, 100}, number, ss_gen, wad_no,
    "Extra look sensitivity at outer threshold (joy_threshold_camera)"
  },

  {
    "joy_extra_ramp_time",
    (config_t *) &joy_extra_ramp_time, NULL,
    {300}, {0,1000}, number, ss_gen, wad_no,
    "Ramp time for extra sensitivity (0 = Instant, 1000 = 1 second)"
  },

  {
    "joy_scale_diagonal_movement",
    (config_t *) &joy_scale_diagonal_movement, NULL,
    {1}, {0, 1}, number, ss_gen, wad_no,
    "Scale diagonal movement (0 = Linear, 1 = Circle to Square)"
  },

  {
    "joy_response_curve_movement",
    (config_t *) &joy_response_curve_movement, NULL,
    {0}, {0, 20}, number, ss_gen, wad_no,
    "Movement response curve (0 = Linear, 10 = Squared, 20 = Cubed)"
  },

  {
    "joy_response_curve_camera",
    (config_t *) &joy_response_curve_camera, NULL,
    {10}, {0, 20}, number, ss_gen, wad_no,
    "Camera response curve (0 = Linear, 10 = Squared, 20 = Cubed)"
  },

  {
    "joy_deadzone_type_movement",
    (config_t *) &joy_deadzone_type_movement, NULL,
    {1}, {0, 1}, number, ss_gen, wad_no,
    "Movement deadzone type (0 = Axial, 1 = Radial)"
  },

  {
    "joy_deadzone_type_camera",
    (config_t *) &joy_deadzone_type_camera, NULL,
    {1}, {0, 1}, number, ss_gen, wad_no,
    "Camera deadzone type (0 = Axial, 1 = Radial)"
  },

  {
    "joy_deadzone_movement",
    (config_t *) &joy_deadzone_movement, NULL,
    {15}, {0, 50}, number, ss_gen, wad_no,
    "Movement deadzone percent"
  },

  {
    "joy_deadzone_camera",
    (config_t *) &joy_deadzone_camera, NULL,
    {15}, {0, 50}, number, ss_gen, wad_no,
    "Camera deadzone percent"
  },

  {
    "joy_threshold_movement",
    (config_t *) &joy_threshold_movement, NULL,
    {2}, {0, 30}, number, ss_gen, wad_no,
    "Movement outer threshold percent"
  },

  {
    "joy_threshold_camera",
    (config_t *) &joy_threshold_camera, NULL,
    {2}, {0, 30}, number, ss_gen, wad_no,
    "Camera outer threshold percent"
  },

  {
    "joy_threshold_trigger",
    (config_t *) &joy_threshold_trigger, NULL,
    {12}, {0, 50}, number, ss_gen, wad_no,
    "Trigger threshold percent"
  },

  {
    "joy_invert_forward",
    (config_t *) &joy_invert_forward, NULL,
    {0}, {0, 1}, number, ss_gen, wad_no,
    "Invert forward axis"
  },

  {
    "joy_invert_strafe",
    (config_t *) &joy_invert_strafe, NULL,
    {0}, {0, 1}, number, ss_gen, wad_no,
    "Invert strafe axis"
  },

  {
    "joy_invert_turn",
    (config_t *) &joy_invert_turn, NULL,
    {0}, {0, 1}, number, ss_gen, wad_no,
    "Invert turn axis"
  },

  {
    "joy_invert_look",
    (config_t *) &joy_invert_look, NULL,
    {0}, {0, 1}, number, ss_gen, wad_no,
    "Invert look axis"
  },

  {
    "padlook",
    (config_t *) &padlook, NULL,
    {0}, {0, 1}, number, ss_gen, wad_no,
    "1 to enable padlook"
  },

  { //jff 4/3/98 allow unlimited sensitivity
    "mouse_sensitivity",
    (config_t *) &mouseSensitivity_horiz, NULL,
    {5}, {0,UL}, number, ss_gen, wad_no,
    "adjust horizontal (x) mouse sensitivity for turning"
  },

  { //jff 4/3/98 allow unlimited sensitivity
    "mouse_sensitivity_y",
    (config_t *) &mouseSensitivity_vert, NULL,
    {5}, {0,UL}, number, ss_gen, wad_no,
    "adjust vertical (y) mouse sensitivity for moving"
  },

  {
    "mouse_sensitivity_strafe",
    (config_t *) &mouseSensitivity_horiz_strafe, NULL,
    {5}, {0,UL}, number, ss_gen, wad_no,
    "adjust horizontal (x) mouse sensitivity for strafing"
  },

  {
    "mouse_sensitivity_y_look",
    (config_t *) &mouseSensitivity_vert_look, NULL,
    {5}, {0,UL}, number, ss_gen, wad_no,
    "adjust vertical (y) mouse sensitivity for looking"
  },

  {
    "mouse_acceleration",
    (config_t *) &mouse_acceleration, NULL,
    {10}, {0,40}, number, ss_gen, wad_no,
    "adjust mouse acceleration (0 = 1.0, 40 = 5.0)"
  },

  {
    "mouse_acceleration_threshold",
    (config_t *) &mouse_acceleration_threshold, NULL,
    {10}, {0,32}, number, ss_none, wad_no,
    "adjust mouse acceleration threshold"
  },

  // [FG] invert vertical axis
  {
    "mouse_y_invert",
    (config_t *) &mouse_y_invert, NULL,
    {0}, {0,1}, number, ss_gen, wad_no,
    "invert vertical axis"
  },

  {
    "novert",
    (config_t *) &novert, NULL,
    {1}, {0,1}, number, ss_none, wad_no,
    "1 to disable vertical mouse movement"
  },

  {
    "mouselook",
    (config_t *) &mouselook, NULL,
    {0}, {0,1}, number, ss_gen, wad_no,
    "1 to enable mouselook"
  },

  // [FG] double click acts as "use"
  {
    "dclick_use",
    (config_t *) &dclick_use, NULL,
    {0}, {0,1}, number, ss_gen, wad_no,
    "double click acts as \"use\""
  },

  {
    "grabmouse",
    (config_t *) &default_grabmouse, NULL,
    {1}, {0, 1}, number, ss_none, wad_no,
    "1 to grab mouse during play"
  },

  {
    "raw_input",
    (config_t *) &raw_input, NULL,
    {1}, {0, 1}, number, ss_none, wad_no,
    "Raw gamepad/mouse input for turning/looking (0 = Interpolate, 1 = Raw)"
  },

  //
  // Chat macro
  //

  {
    "chatmacro0",
    (config_t *) &chat_macros[0], NULL,
    {.s = HUSTR_CHATMACRO0}, {0}, string, ss_none, wad_yes,
    "chat string associated with 0 key"
  },

  {
    "chatmacro1",
    (config_t *) &chat_macros[1], NULL,
    {.s = HUSTR_CHATMACRO1}, {0}, string, ss_none, wad_yes,
    "chat string associated with 1 key"
  },

  {
    "chatmacro2",
    (config_t *) &chat_macros[2], NULL,
    {.s = HUSTR_CHATMACRO2}, {0}, string, ss_none, wad_yes,
    "chat string associated with 2 key"
  },

  {
    "chatmacro3",
    (config_t *) &chat_macros[3], NULL,
    {.s = HUSTR_CHATMACRO3}, {0}, string, ss_none, wad_yes,
    "chat string associated with 3 key"
  },

  {
    "chatmacro4",
    (config_t *) &chat_macros[4], NULL,
    {.s = HUSTR_CHATMACRO4}, {0}, string, ss_none, wad_yes,
    "chat string associated with 4 key"
  },

  {
    "chatmacro5",
    (config_t *) &chat_macros[5], NULL,
    {.s = HUSTR_CHATMACRO5}, {0}, string, ss_none, wad_yes,
    "chat string associated with 5 key"
  },

  {
    "chatmacro6",
    (config_t *) &chat_macros[6], NULL,
    {.s = HUSTR_CHATMACRO6}, {0}, string, ss_none, wad_yes,
    "chat string associated with 6 key"
  },

  {
    "chatmacro7",
    (config_t *) &chat_macros[7], NULL,
    {.s = HUSTR_CHATMACRO7}, {0}, string, ss_none, wad_yes,
    "chat string associated with 7 key"
  },

  {
    "chatmacro8",
    (config_t *) &chat_macros[8], NULL,
    {.s = HUSTR_CHATMACRO8}, {0}, string, ss_none, wad_yes,
    "chat string associated with 8 key"
  },

  {
    "chatmacro9",
    (config_t *) &chat_macros[9], NULL,
    {.s = HUSTR_CHATMACRO9}, {0}, string, ss_none, wad_yes,
    "chat string associated with 9 key"
  },

  //
  // Automap
  //

  //jff 1/7/98 defaults for automap colors
  //jff 4/3/98 remove -1 in lower range, 0 now disables new map features
  { // black //jff 4/6/98 new black
    "mapcolor_back",
    (config_t *) &mapcolor_back, NULL,
    {247}, {0,255}, number, ss_none, wad_yes,
    "color used as background for automap"
  },

  {  // dk gray
    "mapcolor_grid",
    (config_t *) &mapcolor_grid, NULL,
    {104}, {0,255}, number, ss_none, wad_yes,
    "color used for automap grid lines"
  },

  { // red-brown
    "mapcolor_wall",
    (config_t *) &mapcolor_wall, NULL,
    {23}, {0,255}, number, ss_none, wad_yes,
    "color used for one side walls on automap"
  },

  { // lt brown
    "mapcolor_fchg",
    (config_t *) &mapcolor_fchg, NULL,
    {55}, {0,255}, number, ss_none, wad_yes,
    "color used for lines floor height changes across"
  },

  { // orange
    "mapcolor_cchg",
    (config_t *) &mapcolor_cchg, NULL,
    {215}, {0,255}, number, ss_none, wad_yes,
    "color used for lines ceiling height changes across"
  },

  { // white
    "mapcolor_clsd",
    (config_t *) &mapcolor_clsd, NULL,
    {208}, {0,255}, number, ss_none, wad_yes,
    "color used for lines denoting closed doors, objects"
  },

  { // red
    "mapcolor_rkey",
    (config_t *) &mapcolor_rkey, NULL,
    {175}, {0,255}, number, ss_none, wad_yes,
    "color used for red key sprites"
  },

  { // blue
    "mapcolor_bkey",
    (config_t *) &mapcolor_bkey, NULL,
    {204}, {0,255}, number, ss_none, wad_yes,
    "color used for blue key sprites"
  },

  { // yellow
    "mapcolor_ykey",
    (config_t *) &mapcolor_ykey, NULL,
    {231}, {0,255}, number, ss_none, wad_yes,
    "color used for yellow key sprites"
  },

  { // red
    "mapcolor_rdor",
    (config_t *) &mapcolor_rdor, NULL,
    {175}, {0,255}, number, ss_none, wad_yes,
    "color used for closed red doors"
  },

  { // blue
    "mapcolor_bdor",
    (config_t *) &mapcolor_bdor, NULL,
    {204}, {0,255}, number, ss_none, wad_yes,
    "color used for closed blue doors"
  },

  { // yellow
    "mapcolor_ydor",
    (config_t *) &mapcolor_ydor, NULL,
    {231}, {0,255}, number, ss_none, wad_yes,
    "color used for closed yellow doors"
  },

  { // dk green
    "mapcolor_tele",
    (config_t *) &mapcolor_tele, NULL,
    {119}, {0,255}, number, ss_none, wad_yes,
    "color used for teleporter lines"
  },

  { // purple
    "mapcolor_secr",
    (config_t *) &mapcolor_secr, NULL,
    {252}, {0,255}, number, ss_none, wad_yes,
    "color used for lines around secret sectors"
  },

  { // green
    "mapcolor_revsecr",
    (config_t *) &mapcolor_revsecr, NULL,
    {112}, {0,255}, number, ss_none, wad_yes,
    "color used for lines around revealed secret sectors"
  },

  { // none
    "mapcolor_exit",
    (config_t *) &mapcolor_exit, NULL,
    {0}, {0,255}, number, ss_none, wad_yes,
    "color used for exit lines"
  },

  { // dk gray
    "mapcolor_unsn",
    (config_t *) &mapcolor_unsn, NULL,
    {104}, {0,255}, number, ss_none, wad_yes,
    "color used for lines not seen without computer map"
  },

  { // lt gray
    "mapcolor_flat",
    (config_t *) &mapcolor_flat, NULL,
    {88}, {0,255}, number, ss_none, wad_yes,
    "color used for lines with no height changes"
  },

  { // green
    "mapcolor_sprt",
    (config_t *) &mapcolor_sprt, NULL,
    {112}, {0,255}, number, ss_none, wad_yes,
    "color used as things"
  },

  { // white
    "mapcolor_hair",
    (config_t *) &mapcolor_hair, NULL,
    {208}, {0,255}, number, ss_none, wad_yes,
    "color used for dot crosshair denoting center of map"
  },

  { // white
    "mapcolor_sngl",
    (config_t *) &mapcolor_sngl, NULL,
    {208}, {0,255}, number, ss_none, wad_yes,
    "color used for the single player arrow"
  },

  { // green
    "mapcolor_ply1",
    (config_t *) &mapcolor_plyr[0], NULL,
    {112}, {0,255}, number, ss_none, wad_yes,
    "color used for the green player arrow"
  },

  { // lt gray
    "mapcolor_ply2",
    (config_t *) &mapcolor_plyr[1], NULL,
    {88}, {0,255}, number, ss_none, wad_yes,
    "color used for the gray player arrow"
  },

  { // brown
    "mapcolor_ply3",
    (config_t *) &mapcolor_plyr[2], NULL,
    {64}, {0,255}, number, ss_none, wad_yes,
    "color used for the brown player arrow"
  },

  { // red
    "mapcolor_ply4",
    (config_t *) &mapcolor_plyr[3], NULL,
    {176}, {0,255}, number, ss_none, wad_yes,
    "color used for the red player arrow"
  },

  {  // purple                     // killough 8/8/98
    "mapcolor_frnd",
    (config_t *) &mapcolor_frnd, NULL,
    {252}, {0,255}, number, ss_none, wad_yes,
    "color used for friends"
  },

  {
    "mapcolor_enemy",
    (config_t *) &mapcolor_enemy, NULL,
    {177}, {0,255}, number, ss_none, wad_yes,
    "color used for enemies"
  },

  {
    "mapcolor_item",
    (config_t *) &mapcolor_item, NULL,
    {231}, {0,255}, number, ss_none, wad_yes,
    "color used for countable items"
  },

  {
    "mapcolor_preset",
    (config_t *) &mapcolor_preset, NULL,
    {0}, {0,2}, number, ss_auto, wad_no,
    "automap color preset (0 = Boom (default), 1 = Vanilla Doom, 2 = ZDoom)"
  },

  {
    "map_point_coord",
    (config_t *) &map_point_coordinates, NULL,
    {1}, {0,1}, number, ss_auto, wad_yes,
    "1 to show automap pointer coordinates in non-follow mode"
  },

  //jff 3/9/98 add option to not show secrets til after found
  // killough change default, to avoid spoilers and preserve Doom mystery
  { // show secret after gotten
    "map_secret_after",
    (config_t *) &map_secret_after, NULL,
    {0}, {0,1}, number, ss_auto, wad_yes,
    "1 to not show secret sectors till after entered"
  },

  {
    "map_keyed_door_flash",
    (config_t *) &map_keyed_door_flash, NULL,
    {0}, {0,1}, number, ss_auto, wad_no,
    "1 to make keyed doors flash on the automap"
  },

  {
    "map_smooth_lines",
    (config_t *) &map_smooth_lines, NULL,
    {1}, {0,1}, number, ss_auto, wad_no,
    "1 to enable smooth automap lines"
  },

  {
    "followplayer",
    (config_t *) &followplayer, NULL,
    {1}, {0,1}, number, ss_auto, wad_no,
    "1 to enable automap follow player mode"
  },

  {
    "automapoverlay",
    (config_t *) &automapoverlay, NULL,
    {overlay_off}, {overlay_off,overlay_dark}, number, ss_auto, wad_no,
    "automap overlay mode (1 = on, 2 = dark)"
  },

  {
    "automaprotate",
    (config_t *) &automaprotate, NULL,
    {0}, {0,1}, number, ss_auto, wad_no,
    "1 to enable automap rotate mode"
  },

  //jff 1/7/98 end additions for automap

  //jff 2/16/98 defaults for color ranges in hud and status

  { // gold range
    "hudcolor_titl",
    (config_t *) &hudcolor_titl, NULL,
    {CR_GOLD}, {CR_BRICK,CR_NONE}, number, ss_none, wad_yes,
    "color range used for automap level title"
  },

  { // green range
    "hudcolor_xyco",
    (config_t *) &hudcolor_xyco, NULL,
    {CR_GREEN}, {CR_BRICK,CR_NONE}, number, ss_none, wad_yes,
    "color range used for automap coordinates"
  },

  //
  // Messages
  //

  {
    "show_messages",
    (config_t *) &showMessages, NULL,
    {1}, {0,1}, number, ss_none, wad_no,
    "1 to enable message display"
  },

  {
    "show_toggle_messages",
    (config_t *) &show_toggle_messages, NULL,
    {1}, {0,1}, number, ss_stat, wad_no,
    "1 to enable toggle messages"
  },

  {
    "show_pickup_messages",
    (config_t *) &show_pickup_messages, NULL,
    {1}, {0,1}, number, ss_stat, wad_no,
    "1 to enable pickup messages"
  },

  {
    "show_obituary_messages",
    (config_t *) &show_obituary_messages, NULL,
    {1}, {0,1}, number, ss_stat, wad_no,
    "1 to enable obituaries"
  },

  // "A secret is revealed!" message
  {
    "hud_secret_message",
    (config_t *) &hud_secret_message, NULL,
    {1}, {0,1}, number, ss_stat, wad_no,
    "\"A secret is revealed!\" message"
  },

  { // red range
    "hudcolor_mesg",
    (config_t *) &hudcolor_mesg, NULL,
    {CR_NONE}, {CR_BRICK,CR_NONE}, number, ss_none, wad_yes,
    "color range used for messages during play"
  },

  { // gold range
    "hudcolor_chat",
    (config_t *) &hudcolor_chat, NULL,
    {CR_GOLD}, {CR_BRICK,CR_NONE}, number, ss_none, wad_yes,
    "color range used for chat messages and entry"
  },

  {
    "hudcolor_obituary",
    (config_t *) &hudcolor_obituary, NULL,
    {CR_GRAY}, {CR_BRICK,CR_NONE}, number, ss_none, wad_yes,
    "color range used for obituaries"
  },

  { // killough 11/98
    "chat_msg_timer",
    (config_t *) &chat_msg_timer, NULL,
    {4000}, {0,UL}, 0, ss_none, wad_yes,
    "Duration of chat messages (ms)"
  },

  { // 1 line scrolling window
    "hud_msg_lines",
    (config_t *) &hud_msg_lines, NULL,
    {4}, {1,HU_MAXMESSAGES}, number, ss_none, wad_yes,
    "number of message lines"
  },

  {
    "message_colorized",
    (config_t *) &message_colorized, NULL,
    {0}, {0,1}, number, ss_stat, wad_no,
    "1 to colorize player messages"
  },

  { // killough 11/98
    "message_centered",
    (config_t *) &message_centered, NULL,
    {0}, {0,1}, number, ss_stat, wad_no,
    "1 to center messages"
  },

  { // killough 11/98
    "message_list",
    (config_t *) &message_list, NULL,
    {0}, {0,1}, number, ss_none, wad_yes,
    "1 means multiline message list is active"
  },

  { // killough 11/98
    "message_timer",
    (config_t *) &message_timer, NULL,
    {4000}, {0,UL}, 0, ss_none, wad_yes,
    "Duration of normal Doom messages (ms)"
  },

  {
    "default_verbosity",
    (config_t *) &cfg_verbosity, NULL,
    {VB_INFO}, {VB_ERROR, VB_MAX - 1}, number, ss_none, wad_no,
    "verbosity level (1 = errors only, 2 = warnings, 3 = info, 4 = debug)"
  },

  //
  // HUD
  //

  { // no color changes on status bar
    "sts_always_red",
    (config_t *) &sts_always_red, NULL,
    {1}, {0,1}, number, ss_stat, wad_yes,
    "1 to disable use of color on status bar"
  },

  {
    "sts_pct_always_gray",
    (config_t *) &sts_pct_always_gray, NULL,
    {0}, {0,1}, number, ss_stat, wad_yes,
    "1 to make percent signs on status bar always gray"
  },

  { // killough 2/28/98
    "sts_traditional_keys",
    (config_t *) &sts_traditional_keys, NULL,
    {0}, {0,1}, number, ss_stat, wad_yes,
    "1 to disable doubled card and skull key display on status bar"
  },

  {
    "st_solidbackground",
    (config_t *) &st_solidbackground, NULL,
    {0}, {0,1}, number, ss_stat, wad_yes,
    "1 for solid color status bar background in widescreen mode"
  },

  { // [Alaux]
    "smooth_counts",
    (config_t *) &smooth_counts, NULL,
    {0}, {0,1}, number, ss_stat, wad_yes,
    "1 to enable smooth health/armor counts"
  },

  { // below is red
    "health_red",
    (config_t *) &health_red, NULL,
    {25}, {0,200}, number, ss_none, wad_yes,
    "amount of health for red to yellow transition"
  },

  { // below is yellow
    "health_yellow",
    (config_t *) &health_yellow, NULL,
    {50}, {0,200}, number, ss_none, wad_yes,
    "amount of health for yellow to green transition"
  },

  { // below is green, above blue
    "health_green",
    (config_t *) &health_green, NULL,
    {100}, {0,200}, number, ss_none, wad_yes,
    "amount of health for green to blue transition"
  },

  { // below is red
    "armor_red",
    (config_t *) &armor_red, NULL,
    {25}, {0,200}, number, ss_none, wad_yes,
    "amount of armor for red to yellow transition"
  },

  { // below is yellow
    "armor_yellow",
    (config_t *) &armor_yellow, NULL,
    {50}, {0,200}, number, ss_none, wad_yes,
    "amount of armor for yellow to green transition"
  },

  { // below is green, above blue
    "armor_green",
    (config_t *) &armor_green, NULL,
    {100}, {0,200}, number, ss_none, wad_yes,
    "amount of armor for green to blue transition"
  },

  { // below 25% is red
    "ammo_red",
    (config_t *) &ammo_red, NULL,
    {25}, {0,100}, number, ss_none, wad_yes,
    "percent of ammo for red to yellow transition"
  },

  { // below 50% is yellow, above green
    "ammo_yellow",
    (config_t *) &ammo_yellow, NULL,
    {50}, {0,100}, number, ss_none, wad_yes,
    "percent of ammo for yellow to green transition"
  },

  { // 0=off, 1=small, 2=full //jff 2/16/98 HUD and status feature controls
    "hud_active",
    (config_t *) &hud_active, NULL,
    {2}, {0,2}, number, ss_stat, wad_yes,
    "0 for HUD off, 1 for HUD small, 2 for full HUD"
  },

  {  // whether hud is displayed //jff 2/23/98
    "hud_displayed",
    (config_t *) &hud_displayed, NULL,
    {0}, {0,1}, number, ss_none, wad_yes,
    "1 to enable display of HUD"
  },

  // [FG] player coords widget
  {
    "hud_player_coords",
    (config_t *) &hud_player_coords, NULL,
    {HUD_WIDGET_AUTOMAP}, {HUD_WIDGET_OFF,HUD_WIDGET_ALWAYS}, number, ss_stat, wad_no,
    "show player coords widget (1 = on Automap, 2 = on HUD, 3 = always)"
  },

  // [FG] level stats widget
  {
    "hud_level_stats",
    (config_t *) &hud_level_stats, NULL,
    {HUD_WIDGET_OFF}, {HUD_WIDGET_OFF,HUD_WIDGET_ALWAYS}, number, ss_stat, wad_no,
    "show level stats (kill, items and secrets) widget (1 = on Automap, 2 = on HUD, 3 = always)"
  },

  // [FG] level time widget
  {
    "hud_level_time",
    (config_t *) &hud_level_time, NULL,
    {HUD_WIDGET_OFF}, {HUD_WIDGET_OFF,HUD_WIDGET_ALWAYS}, number, ss_stat, wad_no,
    "show level time widget (1 = on Automap, 2 = on HUD, 3 = always)"
  },

  {
    "hud_time_use",
    (config_t *) &hud_time_use, NULL,
    {0}, {0,1}, number, ss_stat, wad_no,
    "show split time when pressing the use button"
  },

  // prefer Crispy HUD, Boom HUD without bars, or Boom HUD with bars
  {
    "hud_type",
    (config_t *) &hud_type, NULL,
    {HUD_TYPE_BOOM}, {HUD_TYPE_CRISPY,NUM_HUD_TYPES-1}, number, ss_stat, wad_no,
    "Fullscreen HUD (0 = Crispy, 1 = Boom (No Bars), 2 = Boom"
  },

  // backpack changes thresholds
  {
    "hud_backpack_thresholds",
    (config_t *) &hud_backpack_thresholds, NULL,
    {1}, {0,1}, number, ss_stat, wad_no,
    "backpack changes thresholds"
  },

  // color of armor depends on type
  {
    "hud_armor_type",
    (config_t *) &hud_armor_type, NULL,
    {0}, {0,1}, number, ss_stat, wad_no,
    "color of armor depends on type"
  },

  {
    "hud_widget_font",
    (config_t *) &hud_widget_font, NULL,
    {HUD_WIDGET_OFF}, {HUD_WIDGET_OFF,HUD_WIDGET_ALWAYS}, number, ss_stat, wad_no,
    "use standard Doom font for widgets (1 = on Automap, 2 = on HUD, 3 = always)"
  },

  {
    "hud_widescreen_widgets",
    (config_t *) &hud_widescreen_widgets, NULL,
    {1}, {0,1}, number, ss_stat, wad_no,
    "arrange widgets on widescreen edges"
  },

  {
    "hud_widget_layout",
    (config_t *) &hud_widget_layout, NULL,
    {0}, {0,1}, number, ss_stat, wad_no,
    "Widget layout (0 = Horizontal, 1 = Vertical)"
  },

  {
    "hud_crosshair",
    (config_t *) &hud_crosshair, NULL,
    {0}, {0,HU_CROSSHAIRS-1}, number, ss_stat, wad_no,
    "enable crosshair"
  },

  {
    "hud_crosshair_health",
    (config_t *) &hud_crosshair_health, NULL,
    {0}, {0,1}, number, ss_stat, wad_no,
    "1 to change crosshair color by player health"
  },

  {
    "hud_crosshair_target",
    (config_t *) &hud_crosshair_target, NULL,
    {0}, {0,2}, number, ss_stat, wad_no,
    "change crosshair color on target (1 = highlight, 2 = health)"
  },

  {
    "hud_crosshair_lockon",
    (config_t *) &hud_crosshair_lockon, NULL,
    {0}, {0,1}, number, ss_stat, wad_no,
    "1 to lock crosshair on target"
  },

  {
    "hud_crosshair_color",
    (config_t *) &hud_crosshair_color, NULL,
    {CR_GRAY}, {CR_BRICK,CR_NONE}, number, ss_stat, wad_no,
    "default crosshair color"
  },

  {
    "hud_crosshair_target_color",
    (config_t *) &hud_crosshair_target_color, NULL,
    {CR_YELLOW}, {CR_BRICK,CR_NONE}, number, ss_stat, wad_no,
    "target crosshair color"
  },

  {NULL}         // last entry
};

static char *defaultfile;
static boolean defaults_loaded = false;      // killough 10/98

#define NUMDEFAULTS ((unsigned)(sizeof defaults / sizeof *defaults - 1))

// killough 11/98: hash function for name lookup
static unsigned default_hash(const char *name)
{
  unsigned hash = 0;
  while (*name)
    hash = hash*2 + toupper(*name++);
  return hash % NUMDEFAULTS;
}

default_t *M_LookupDefault(const char *name)
{
  static int hash_init;
  register default_t *dp;

  // Initialize hash table if not initialized already
  if (!hash_init)
    for (hash_init = 1, dp = defaults; dp->name; dp++)
      {
        unsigned h = default_hash(dp->name);
        dp->next = defaults[h].first;
        defaults[h].first = dp;
      }

  // Look up name in hash table
  for (dp = defaults[default_hash(name)].first;
       dp && strcasecmp(name, dp->name); dp = dp->next);

  return dp;
}

//
// M_SaveDefaults
//

void M_SaveDefaults (void)
{
  char *tmpfile;
  register default_t *dp;
  FILE *f;
  int maxlen = 0;

  // killough 10/98: for when exiting early
  if (!defaults_loaded || !defaultfile)
    return;

  // get maximum config key string length
  for (dp = defaults; ; dp++)
  {
    int len;
    if (!dp->name)
      break;
    len = strlen(dp->name);
    if (len > maxlen && len < 80) {
      maxlen = len;
    }
  }

  tmpfile = M_StringJoin(D_DoomPrefDir(), DIR_SEPARATOR_S, "tmp", D_DoomExeName(), ".cfg", NULL);
  NormalizeSlashes(tmpfile);

  errno = 0;
  if (!(f = M_fopen(tmpfile, "w")))  // killough 9/21/98
    goto error;

  // 3/3/98 explain format of file
  // killough 10/98: use executable's name

  if (config_help &&
      fprintf(f,";%s.cfg format:\n"
	      ";[min-max(default)] description of variable\n"
	      ";* at end indicates variable is settable in wads\n"
	      ";variable   value\n", D_DoomExeName()) == EOF)
    goto error;

  // killough 10/98: output comment lines which were read in during input

  for (dp = defaults; ; dp++)
    {
      config_t value = {0};

      // If we still haven't seen any blanks,
      // Output a blank line for separation

      if (putc('\n',f) == EOF)
	goto error;

      if (!dp->name)      // If we're at end of defaults table, exit loop
        break;

      //jff 3/3/98 output help string
      //
      // killough 10/98:
      // Don't output config help if any [ lines appeared before this one.
      // Make default values, and numeric range output, automatic.
      //
      // Always write a help string to avoid incorrect entries
      // in the user config

      if (config_help)
	if ((dp->type == string ? 
	     fprintf(f,"[(\"%s\")]", (char *) dp->defaultvalue.s) :
	     dp->limit.min == UL ?
	     dp->limit.max == UL ?
	     fprintf(f, "[?-?(%d)]", dp->defaultvalue.i) :
	     fprintf(f, "[?-%d(%d)]", dp->limit.max, dp->defaultvalue.i) :
	     dp->limit.max == UL ?
	     fprintf(f, "[%d-?(%d)]", dp->limit.min, dp->defaultvalue.i) :
	     fprintf(f, "[%d-%d(%d)]", dp->limit.min, dp->limit.max,
		     dp->defaultvalue.i)) == EOF ||
	    fprintf(f," %s %s\n", dp->help, dp->wad_allowed ? "*" :"") == EOF)
	  goto error;

      // killough 11/98:
      // Write out original default if .wad file has modified the default
      
      if (dp->type == string)
        value.s = dp->modified ? dp->orig_default.s : dp->location->s;
      else if (dp->type == number)
        value.i = dp->modified ? dp->orig_default.i : dp->location->i;

      //jff 4/10/98 kill super-hack on pointer value
      // killough 3/6/98:
      // use spaces instead of tabs for uniform justification

      if (dp->type != input)
      {
      if (dp->type == number ?
          fprintf(f, "%-*s %i\n", maxlen, dp->name, value.i) == EOF :
          fprintf(f,"%-*s \"%s\"\n", maxlen, dp->name, (char *) value.s) == EOF)
        goto error;
      }

      if (dp->type == input)
      {
        int i;
        const input_t *inputs = M_Input(dp->input_id);

        fprintf(f, "%-*s ", maxlen, dp->name);

        for (i = 0; i < array_size(inputs); ++i)
        {
          if (i > 0)
            fprintf(f, ", ");

          switch (inputs[i].type)
          {
            const char *s;

            case INPUT_KEY:
              if (inputs[i].value >= 33 && inputs[i].value <= 126)
              {
                // The '=', ',', and '.' keys originally meant the shifted
                // versions of those keys, but w/o having to shift them in
                // the game.
                char c = inputs[i].value;
                if (c == ',')
                  c = '<';
                else if (c == '.')
                  c = '>';
                else if (c == '=')
                  c = '+';

                fprintf(f, "%c", c);
              }
              else
              {
                s = M_GetNameForKey(inputs[i].value);
                if (s)
                  fprintf(f, "%s", s);
              }
              break;
            case INPUT_MOUSEB:
              {
                s = M_GetNameForMouseB(inputs[i].value);
                if (s)
                  fprintf(f, "%s", s);
              }
              break;
            case INPUT_JOYB:
              {
                s = M_GetNameForJoyB(inputs[i].value);
                if (s)
                  fprintf(f, "%s", s);
              }
              break;
            default:
              break;
          }
        }

        if (i == 0)
          fprintf(f, "%s", "NONE");

        fprintf(f, "\n");
      }
    }

  if (fclose(f) == EOF)
    {
    error:
      I_Error("Could not write defaults to %s: %s\n%s left unchanged\n",
	      tmpfile, errno ? strerror(errno): "(Unknown Error)",defaultfile);
      return;
    }

  M_remove(defaultfile);

  if (M_rename(tmpfile, defaultfile))
    I_Error("Could not write defaults to %s: %s\n", defaultfile,
	    errno ? strerror(errno): "(Unknown Error)");

  free(tmpfile);
}

//
// M_ParseOption()
//
// killough 11/98:
//
// This function parses .cfg file lines, or lines in OPTIONS lumps
//

boolean M_ParseOption(const char *p, boolean wad)
{
  char name[80], strparm[1024];
  default_t *dp;
  int parm;

  while (isspace(*p))  // killough 10/98: skip leading whitespace
    p++;

  //jff 3/3/98 skip lines not starting with an alphanum
  // killough 10/98: move to be made part of main test, add comment-handling

  if (sscanf(p, "%79s %1023[^\n]", name, strparm) != 2 || !isalnum(*name) ||
      !(dp = M_LookupDefault(name)) ||
      (*strparm == '"') == (dp->type != string) ||
      (wad && !dp->wad_allowed))
    return 1;

  // [FG] bind mapcolor options to the mapcolor preset menu item
  if (strncmp(name, "mapcolor_", 9) == 0 ||
      strcmp(name, "hudcolor_titl") == 0)
  {
    default_t *dp_preset = M_LookupDefault("mapcolor_preset");
    dp->setup_menu = dp_preset->setup_menu;
  }

  if (demo_version < 203 && dp->setup_menu &&
      !(dp->setup_menu->m_flags & S_COSMETIC))
    return 1;

  if (dp->type == string)     // get a string default
    {
      int len = strlen(strparm)-1;

      while (isspace(strparm[len]))
        len--;

      if (strparm[len] == '"')
        len--;

      strparm[len+1] = 0;

      if (wad && !dp->modified)                       // Modified by wad
	{                                             // First time modified
	  dp->modified = 1;                           // Mark it as modified
	  dp->orig_default.s = dp->location->s;         // Save original default
	}
      else
	free(dp->location->s);                // Free old value

      dp->location->s = strdup(strparm+1);    // Change default value

      if (dp->current)                                // Current value
	{
	  free(dp->current->s);               // Free old value
	  dp->current->s = strdup(strparm+1); // Change current value
	}
    }
  else if (dp->type == number)
    {
      if (sscanf(strparm, "%i", &parm) != 1)
	return 1;                       // Not A Number

      //jff 3/4/98 range check numeric parameters
      if ((dp->limit.min == UL || dp->limit.min <= parm) &&
	  (dp->limit.max == UL || dp->limit.max >= parm))
	{
	  if (wad)
	    {
	      if (!dp->modified)         // First time it's modified by wad
		{
		  dp->modified = 1;      // Mark it as modified
		  dp->orig_default.i = dp->location->i;  // Save original default
		}
	      if (dp->current)           // Change current value
		dp->current->i = parm;
	    }
	  dp->location->i = parm;          // Change default
	}
    }
  else if (dp->type == input)
    {
      char buffer[80];
      char *scan;

      M_InputReset(dp->input_id);

      scan = strtok(strparm, ",");

      do
      {
        if (sscanf(scan, "%79s", buffer) == 1)
        {
          if (strlen(buffer) == 1)
          {
            // The '=', ',', and '.' keys originally meant the shifted
            // versions of those keys, but w/o having to shift them in
            // the game.
            char c = buffer[0];
            if (c == '<')
              c = ',';
            else if (c == '>')
              c = '.';
            else if (c == '+')
              c = '=';

            M_InputAddKey(dp->input_id, c);
          }
          else
          {
            int value;
            if ((value = M_GetKeyForName(buffer)) > 0)
              M_InputAddKey(dp->input_id, value);
            else if ((value = M_GetJoyBForName(buffer)) >= 0)
              M_InputAddJoyB(dp->input_id, value);
            else if ((value = M_GetMouseBForName(buffer)) >= 0)
              M_InputAddMouseB(dp->input_id, value);
          }
        }

        scan = strtok(NULL, ",");
      } while (scan);
    }

  if (wad && dp->setup_menu)
    dp->setup_menu->m_flags |= S_DISABLE;

  return 0;                          // Success
}

//
// M_LoadOptions()
//
// killough 11/98:
// This function is used to load the OPTIONS lump.
// It allows wads to change game options.
//

void M_LoadOptions(void)
{
  int lump;

  //!
  // @category mod
  //
  // Avoid loading OPTIONS lumps embedded into WAD files.
  //

  if (!M_CheckParm("-nooptions"))
  {
  if ((lump = W_CheckNumForName("OPTIONS")) != -1)
    {
      int size = W_LumpLength(lump), buflen = 0;
      char *buf = NULL, *p, *options = p = W_CacheLumpNum(lump, PU_STATIC);
      while (size > 0)
	{
	  int len = 0;
	  while (len < size && p[len++] && p[len-1] != '\n');
	  if (len >= buflen)
	    buf = I_Realloc(buf, buflen = len+1);
	  strncpy(buf, p, len)[len] = 0;
	  p += len;
	  size -= len;
	  M_ParseOption(buf, true);
	}
      free(buf);
      Z_ChangeTag(options, PU_CACHE);
    }
  }

  M_Trans();           // reset translucency in case of change
  M_ResetMenu();       // reset menu in case of change
}

//
// M_LoadDefaults
//

void M_LoadDefaults (void)
{
  register default_t *dp;
  int i;
  FILE *f;

  // set everything to base values
  //
  // phares 4/13/98:
  // provide default strings with their own malloced memory so that when
  // we leave this routine, that's what we're dealing with whether there
  // was a config file or not, and whether there were chat definitions
  // in it or not. This provides consistency later on when/if we need to
  // edit these strings (i.e. chat macros in the Chat Strings Setup screen).

  for (dp = defaults; dp->name; dp++)
    if (dp->type == string)
      dp->location->s = strdup(dp->defaultvalue.s);
    else if (dp->type == number)
      dp->location->i = dp->defaultvalue.i;
    else if (dp->type == input)
      M_InputSetDefault(dp->input_id, dp->inputs);

  // Load special keys
  M_InputPredefined();

  // check for a custom default file

  if (!defaultfile)
  {
    //!
    // @arg <file>
    // @vanilla
    //
    // Load main configuration from the specified file, instead of the
    // default.
    //

    if ((i = M_CheckParm("-config")) && i < myargc-1)
      defaultfile = strdup(myargv[i+1]);
    else
      defaultfile = strdup(basedefault);
  }

  NormalizeSlashes(defaultfile);

  // read the file in, overriding any set defaults
  //
  // killough 9/21/98: Print warning if file missing, and use fgets for reading

  if ((f = M_fopen(defaultfile, "r")))
  {
    char s[256];

    while (fgets(s, sizeof s, f))
      M_ParseOption(s, false);
  }

  defaults_loaded = true;            // killough 10/98

  // [FG] initialize logging verbosity early to decide
  //      if the following lines will get printed or not

  I_InitPrintf();

  I_Printf(VB_INFO, "M_LoadDefaults: Load system defaults.");

  if (f)
  {
    I_Printf(VB_INFO, " default file: %s\n", defaultfile);
    fclose(f);
  }
  else
  {
    I_Printf(VB_WARNING, " Warning: Cannot read %s -- using built-in defaults\n",
                         defaultfile);
  }
}

//
// M_WriteFile
//
// killough 9/98: rewritten to use stdio and to flash disk icon

boolean M_WriteFile(char const *name, void *source, int length)
{
  FILE *fp;

  errno = 0;
  
  if (!(fp = M_fopen(name, "wb")))       // Try opening file
    return 0;                          // Could not open file for writing

  I_BeginRead(DISK_ICON_THRESHOLD);                       // Disk icon on
  length = fwrite(source, 1, length, fp) == length;   // Write data
  fclose(fp);
  I_EndRead();                         // Disk icon off

  if (!length)                         // Remove partially written file
    M_remove(name);

  return length;
}

//
// M_ReadFile
//
// killough 9/98: rewritten to use stdio and to flash disk icon

int M_ReadFile(char const *name, byte **buffer)
{
  FILE *fp;

  errno = 0;

  if ((fp = M_fopen(name, "rb")))
    {
      size_t length;

      I_BeginRead(DISK_ICON_THRESHOLD);
      fseek(fp, 0, SEEK_END);
      length = ftell(fp);
      fseek(fp, 0, SEEK_SET);
      *buffer = Z_Malloc(length, PU_STATIC, 0);
      if (fread(*buffer, 1, length, fp) == length)
        {
          fclose(fp);
          I_EndRead();
          return length;
        }
      fclose(fp);
    }

  I_Error("Couldn't read file %s: %s", name, 
	  errno ? strerror(errno) : "(Unknown Error)");

  return 0;
}

//
// SCREEN SHOTS
//

//
// M_ScreenShot
//
// Modified by Lee Killough so that any number of shots can be taken,
// the code is faster, and no annoying "screenshot" message appears.
//
// killough 10/98: improved error-handling

void M_ScreenShot (void)
{
  boolean success = false;

  errno = 0;

  if (!M_access(screenshotdir,2))
    {
      static int shot;
      char lbmname[16] = {0};
      int tries = 10000;
      char *screenshotname = NULL;

      do
      {
        M_snprintf(lbmname, sizeof(lbmname), "%.4s%04d.png",
                   D_DoomExeName(), shot++); // [FG] PNG
        if (screenshotname)
          free(screenshotname);
        screenshotname = M_StringJoin(screenshotdir, DIR_SEPARATOR_S,
                                      lbmname, NULL);
      }
      while (!M_access(screenshotname,0) && --tries);

      if (tries)
        {
          // killough 10/98: detect failure and remove file if error
	  // killough 11/98: add hires support
          if (!(success = I_WritePNGfile(screenshotname))) // [FG] PNG
	    {
	      int t = errno;
	      M_remove(screenshotname);
	      errno = t;
	    }
        }
      if (screenshotname)
        free(screenshotname);
    }

  // 1/18/98 killough: replace "SCREEN SHOT" acknowledgement with sfx
  // players[consoleplayer].message = "screen shot"

  // killough 10/98: print error message and change sound effect if error
  S_StartSound(NULL, !success ? displaymsg("%s", errno ? strerror(errno) :
					"Could not take screenshot"), sfx_oof :
               gamemode==commercial ? sfx_radio : sfx_tink);


}

//----------------------------------------------------------------------------
//
// $Log: m_misc.c,v $
// Revision 1.60  1998/06/03  20:32:12  jim
// Fixed mispelling of key_chat string
//
// Revision 1.59  1998/05/21  12:12:28  jim
// Removed conditional from net code
//
// Revision 1.58  1998/05/16  09:41:15  jim
// formatted net files, installed temp switch for testing Stan/Lee's version
//
// Revision 1.57  1998/05/12  12:47:04  phares
// Removed OVER_UNDER code
//
// Revision 1.56  1998/05/05  19:56:01  phares
// Formatting and Doc changes
//
// Revision 1.55  1998/05/05  16:29:12  phares
// Removed RECOIL and OPT_BOBBING defines
//
// Revision 1.54  1998/05/03  23:05:19  killough
// Fix #includes, remove external decls duplicated elsewhere, fix LONG() conflict
//
// Revision 1.53  1998/04/23  13:07:27  jim
// Add exit line to automap
//
// Revision 1.51  1998/04/22  13:46:12  phares
// Added Setup screen Reset to Defaults
//
// Revision 1.50  1998/04/19  01:13:50  killough
// Fix freeing memory before use in savegame code
//
// Revision 1.49  1998/04/17  10:35:50  killough
// Add traditional_menu option for main menu
//
// Revision 1.48  1998/04/14  08:18:11  killough
// replace obsolete adaptive_gametic with realtic_clock_rate
//
// Revision 1.47  1998/04/13  21:36:33  phares
// Cemented ESC and F1 in place
//
// Revision 1.46  1998/04/13  12:30:02  phares
// Resolved Z_Free error msg when no boom.cfg file
//
// Revision 1.45  1998/04/12  22:55:33  phares
// Remaining 3 Setup screens
//
// Revision 1.44  1998/04/10  23:21:41  jim
// fixed string/int differentiation by value
//
// Revision 1.43  1998/04/10  06:37:54  killough
// Add adaptive gametic timer option
//
// Revision 1.42  1998/04/06  11:05:00  jim
// Remove LEESFIXES, AMAP bdg->247
//
// Revision 1.41  1998/04/06  04:50:00  killough
// Support demo_insurance=2
//
// Revision 1.40  1998/04/05  00:51:13  phares
// Joystick support, Main Menu re-ordering
//
// Revision 1.39  1998/04/03  14:45:49  jim
// Fixed automap disables at 0, mouse sens unbounded
//
// Revision 1.38  1998/03/31  10:44:31  killough
// Add demo insurance option
//
// Revision 1.37  1998/03/31  00:39:44  jim
// Screenshots in BMP format added
//
// Revision 1.36  1998/03/25  16:31:23  jim
// Fixed bad default value for defaultskill
//
// Revision 1.34  1998/03/23  15:24:17  phares
// Changed pushers to linedef control
//
// Revision 1.33  1998/03/20  00:29:47  phares
// Changed friction to linedef control
//
// Revision 1.32  1998/03/11  17:48:16  phares
// New cheats, clean help code, friction fix
//
// Revision 1.31  1998/03/10  07:06:30  jim
// Added secrets on automap after found only option
//
// Revision 1.30  1998/03/09  18:29:12  phares
// Created separately bound automap and menu keys
//
// Revision 1.29  1998/03/09  11:00:20  jim
// allowed -1 in mouse bindings and map functions
//
// Revision 1.28  1998/03/09  07:35:18  killough
// Rearrange order of cfg options, add capslock options
//
// Revision 1.27  1998/03/06  21:41:04  jim
// fixed erroneous range for gamma in config
//
// Revision 1.26  1998/03/05  00:57:47  jim
// Scattered HUD
//
// Revision 1.25  1998/03/04  11:55:42  jim
// Add range checking, help strings to BOOM.CFG
//
// Revision 1.24  1998/03/02  15:34:15  jim
// Added Rand's HELP screen as lump and loaded and displayed it
//
// Revision 1.23  1998/03/02  11:36:44  killough
// clone defaults, add sts_traditional_keys
//
// Revision 1.22  1998/02/27  19:22:05  jim
// Range checked hud/sound card variables
//
// Revision 1.21  1998/02/27  08:10:02  phares
// Added optional player bobbing
//
// Revision 1.20  1998/02/26  22:58:39  jim
// Added message review display to HUD
//
// Revision 1.19  1998/02/24  22:00:57  killough
// turn translucency back on by default
//
// Revision 1.18  1998/02/24  08:46:05  phares
// Pushers, recoil, new friction, and over/under work
//
// Revision 1.17  1998/02/23  14:21:14  jim
// Merged HUD stuff, fixed p_plats.c to support elevators again
//
// Revision 1.16  1998/02/23  04:40:48  killough
// Lots of new options
//
// Revision 1.14  1998/02/20  21:57:00  phares
// Preliminarey sprite translucency
//
// Revision 1.13  1998/02/20  18:46:58  jim
// cleanup of HUD control
//
// Revision 1.12  1998/02/19  16:54:33  jim
// Optimized HUD and made more configurable
//
// Revision 1.11  1998/02/18  11:56:11  jim
// Fixed issues with HUD and reduced screen size
//
// Revision 1.9  1998/02/15  03:21:20  phares
// Jim's comment: Fixed bug in automap from mistaking framebuffer index for mark color
//
// Revision 1.8  1998/02/15  03:17:56  phares
// User-defined keys
//
// Revision 1.6  1998/02/09  03:04:12  killough
// Add weapon preferences, player corpse, vsync options
//
// Revision 1.5  1998/02/02  13:37:26  killough
// Clone compatibility flag, for TNTCOMP to work
//
// Revision 1.4  1998/01/26  19:23:49  phares
// First rev with no ^Ms
//
// Revision 1.3  1998/01/26  04:59:07  killough
// Fix DOOM 1 screenshot acknowledgement
//
// Revision 1.2  1998/01/21  16:56:16  jim
// Music fixed, defaults for cards added
//
// Revision 1.1.1.1  1998/01/19  14:02:57  rand
// Lee's Jan 19 sources
//
//----------------------------------------------------------------------------

