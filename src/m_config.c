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

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "am_map.h"
#include "config.h"
#include "d_main.h"
#include "doomdef.h"
#include "doomstat.h"
#include "doomtype.h"
#include "dstrings.h"
#include "hu_lib.h" // HU_MAXMESSAGES
#include "hu_obituary.h"
#include "hu_stuff.h"
#include "i_printf.h"
#include "i_system.h"
#include "m_argv.h"
#include "m_array.h"
#include "m_config.h"
#include "m_input.h"
#include "m_io.h"
#include "m_misc.h"
#include "mn_menu.h"
#include "mn_internal.h"
#include "r_main.h"
#include "st_stuff.h"
#include "v_video.h"
#include "w_wad.h"
#include "z_zone.h"

//
// DEFAULTS
//

static int config_help; // jff 3/3/98
// [FG] invert vertical axis
extern int showMessages;
extern int show_toggle_messages;
extern int show_pickup_messages;
extern boolean palette_changes;

extern char *chat_macros[]; // killough 10/98

// jff 3/3/98 added min, max, and help string to all entries
// jff 4/10/98 added isstr field to specify whether value is string or int
//
//  killough 11/98: entirely restructured to allow options to be modified
//  from wads, and to consolidate with menu code

default_t *defaults = NULL;

default_t defaults_orig[] = {

  { //jff 3/3/98
    "config_help",
    (config_t *) &config_help, NULL,
    {1}, {0,1}, number, ss_none, wad_no,
    "1 to show help strings about each variable in config file"
  },

  //
  // QOL features
  //

  {
    "palette_changes",
    (config_t *) &palette_changes, NULL,
    {1}, {0,1}, number, ss_gen, wad_no,
    "0 to disable palette changes"
  },

  //
  // Compatibility
  //

  {
    "shorttics",
    (config_t *) &shorttics, NULL,
    {0}, {0, 1}, number, ss_none, wad_no,
    "1 to use low resolution turning."
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
    {1}, {0,2}, number, ss_auto, wad_no,
    "automap color preset (0 = Vanilla Doom, 1 = Boom (default), 2 = ZDoom)"
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
    "map_keyed_door",
    (config_t *) &map_keyed_door, NULL,
    {MAP_KEYED_DOOR_COLOR}, {MAP_KEYED_DOOR_OFF, MAP_KEYED_DOOR_FLASH}, number, ss_auto, wad_no,
    "keyed doors are colored (1) or flashing (2) on the automap"
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
    {AM_OVERLAY_OFF}, {AM_OVERLAY_OFF,AM_OVERLAY_DARK}, number, ss_auto, wad_no,
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
    "sts_colored_numbers",
    (config_t *) &sts_colored_numbers, NULL,
    {0}, {0,1}, number, ss_stat, wad_yes,
    "1 to enable use of color on status bar"
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
    "hud_blink_keys",
    (config_t *) &hud_blink_keys, NULL,
    {0}, {0,1}, number, ss_none, wad_yes,
    "1 to make missing keys blink when trying to trigger linedef actions"
  },

  {
    "st_solidbackground",
    (config_t *) &st_solidbackground, NULL,
    {0}, {0,1}, number, ss_stat, wad_yes,
    "1 for solid color status bar background in widescreen mode"
  },

  { // [Alaux]
    "hud_animated_counts",
    (config_t *) &hud_animated_counts, NULL,
    {0}, {0,1}, number, ss_stat, wad_yes,
    "1 to enable animated health/armor counts"
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
    "Fullscreen HUD (0 = Crispy, 1 = Boom (No Bars), 2 = Boom)"
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

void M_BindInt(const char *name, int *location, int *current,
               int default_val, int min_val, int max_val,
               ss_types screen, wad_allowed_t wad,
               const char *help)
{
    default_t item = { name, (config_t *)location, (config_t *)current,
                       {.i = default_val}, {min_val, max_val},
                       number, screen, wad, help };
    array_push(defaults, item);
}

void M_BindBool(const char *name, boolean *location, boolean *current,
                boolean default_val, ss_types screen, wad_allowed_t wad,
                const char *help)
{
    M_BindInt(name, (int *)location, (int *)current, (int)default_val, 0, 1,
              screen, wad, help);
}

void M_BindStr(const char *name, const char **location, char *default_val,
               const char *help)
{
    default_t item = { name, (config_t *)location, NULL,
                       {.s = default_val}, {0},
                       string, ss_none, wad_no, help };
    array_push(defaults, item);
}

void M_BindInput(const char *name, int input_id, const char *help)
{
    default_t item = {name, NULL, NULL, {0}, {UL,UL}, input, ss_keys, wad_no,
                      help, input_id};
    array_push(defaults, item);
}

void M_InitConfig(void)
{
    for (int i = 0; i < arrlen(defaults_orig); ++i)
    {
        array_push(defaults, defaults_orig[i]);
    }
}

static char *defaultfile;
static boolean defaults_loaded = false; // killough 10/98

//#define NUMDEFAULTS ((unsigned)(sizeof defaults / sizeof *defaults - 1))

// killough 11/98: hash function for name lookup
static unsigned default_hash(const char *name)
{
    unsigned hash = 0;
    while (*name)
    {
        hash = hash * 2 + M_ToUpper(*name++);
    }
    return hash % (array_size(defaults) - 1);
}

default_t *M_LookupDefault(const char *name)
{
    static int hash_init;
    register default_t *dp;

    // Initialize hash table if not initialized already
    if (!hash_init)
    {
        for (hash_init = 1, dp = defaults; dp->name; dp++)
        {
            unsigned h = default_hash(dp->name);
            dp->next = defaults[h].first;
            defaults[h].first = dp;
        }
    }

    // Look up name in hash table
    for (dp = defaults[default_hash(name)].first;
         dp && strcasecmp(name, dp->name); dp = dp->next)
        ;

    return dp;
}

//
// M_SaveDefaults
//

void M_SaveDefaults(void)
{
    char *tmpfile;
    register default_t *dp;
    FILE *f;
    int maxlen = 0;

    // killough 10/98: for when exiting early
    if (!defaults_loaded || !defaultfile)
    {
        return;
    }

    // get maximum config key string length
    for (dp = defaults;; dp++)
    {
        int len;
        if (!dp->name)
        {
            break;
        }
        len = strlen(dp->name);
        if (len > maxlen && len < 80)
        {
            maxlen = len;
        }
    }

    tmpfile = M_StringJoin(D_DoomPrefDir(), DIR_SEPARATOR_S, "tmp",
                           D_DoomExeName(), ".cfg", NULL);

    errno = 0;
    if (!(f = M_fopen(tmpfile, "w"))) // killough 9/21/98
    {
        goto error;
    }

    // 3/3/98 explain format of file
    // killough 10/98: use executable's name

    if (config_help
        && fprintf(f,
                   ";%s.cfg format:\n"
                   ";[min-max(default)] description of variable\n"
                   ";* at end indicates variable is settable in wads\n"
                   ";variable   value\n",
                   D_DoomExeName())
               == EOF)
    {
        goto error;
    }

    // killough 10/98: output comment lines which were read in during input

    for (dp = defaults;; dp++)
    {
        config_t value = {0};

        // If we still haven't seen any blanks,
        // Output a blank line for separation

        if (putc('\n', f) == EOF)
        {
            goto error;
        }

        if (!dp->name) // If we're at end of defaults table, exit loop
        {
            break;
        }

        // jff 3/3/98 output help string
        //
        //  killough 10/98:
        //  Don't output config help if any [ lines appeared before this one.
        //  Make default values, and numeric range output, automatic.
        //
        //  Always write a help string to avoid incorrect entries
        //  in the user config

        if (config_help)
        {
            if ((dp->type == string
                     ? fprintf(f, "[(\"%s\")]", (char *)dp->defaultvalue.s)
                     : dp->limit.min == UL
                        ? dp->limit.max == UL
                           ? fprintf(f, "[?-?(%d)]", dp->defaultvalue.i)
                           : fprintf(f, "[?-%d(%d)]", dp->limit.max,
                                     dp->defaultvalue.i)
                        : dp->limit.max == UL
                           ? fprintf(f, "[%d-?(%d)]", dp->limit.min,
                                     dp->defaultvalue.i)
                           : fprintf(f, "[%d-%d(%d)]", dp->limit.min, dp->limit.max,
                                     dp->defaultvalue.i))
                       == EOF
                || fprintf(f, " %s %s\n", dp->help, dp->wad_allowed ? "*" : "")
                       == EOF)
            {
                goto error;
            }
        }

        // killough 11/98:
        // Write out original default if .wad file has modified the default

        if (dp->type == string)
        {
            value.s = dp->modified ? dp->orig_default.s : dp->location->s;
        }
        else if (dp->type == number)
        {
            value.i = dp->modified ? dp->orig_default.i : dp->location->i;
        }

        // jff 4/10/98 kill super-hack on pointer value
        //  killough 3/6/98:
        //  use spaces instead of tabs for uniform justification

        if (dp->type != input)
        {
            if (dp->type == number
                    ? fprintf(f, "%-*s %i\n", maxlen, dp->name, value.i) == EOF
                    : fprintf(f, "%-*s \"%s\"\n", maxlen, dp->name, value.s)
                          == EOF)
            {
                goto error;
            }
        }

        if (dp->type == input)
        {
            int i;
            const input_t *inputs = M_Input(dp->input_id);

            fprintf(f, "%-*s ", maxlen, dp->name);

            for (i = 0; i < array_size(inputs); ++i)
            {
                if (i > 0)
                {
                    fprintf(f, ", ");
                }

                switch (inputs[i].type)
                {
                    const char *s;

                    case INPUT_KEY:
                        if (inputs[i].value >= 33 && inputs[i].value <= 126)
                        {
                            // The '=', ',', and '.' keys originally meant the
                            // shifted versions of those keys, but w/o having to
                            // shift them in the game.
                            char c = inputs[i].value;
                            if (c == ',')
                            {
                                c = '<';
                            }
                            else if (c == '.')
                            {
                                c = '>';
                            }
                            else if (c == '=')
                            {
                                c = '+';
                            }

                            fprintf(f, "%c", c);
                        }
                        else
                        {
                            s = M_GetNameForKey(inputs[i].value);
                            if (s)
                            {
                                fprintf(f, "%s", s);
                            }
                        }
                        break;
                    case INPUT_MOUSEB:
                        {
                            s = M_GetNameForMouseB(inputs[i].value);
                            if (s)
                            {
                                fprintf(f, "%s", s);
                            }
                        }
                        break;
                    case INPUT_JOYB:
                        {
                            s = M_GetNameForJoyB(inputs[i].value);
                            if (s)
                            {
                                fprintf(f, "%s", s);
                            }
                        }
                        break;
                    default:
                        break;
                }
            }

            if (i == 0)
            {
                fprintf(f, "%s", "NONE");
            }

            fprintf(f, "\n");
        }
    }

    if (fclose(f) == EOF)
    {
    error:
        I_Error("Could not write defaults to %s: %s\n%s left unchanged\n",
                tmpfile, errno ? strerror(errno) : "(Unknown Error)",
                defaultfile);
        return;
    }

    M_remove(defaultfile);

    if (M_rename(tmpfile, defaultfile))
    {
        I_Error("Could not write defaults to %s: %s\n", defaultfile,
                errno ? strerror(errno) : "(Unknown Error)");
    }

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

    while (isspace(*p)) // killough 10/98: skip leading whitespace
    {
        p++;
    }

    // jff 3/3/98 skip lines not starting with an alphanum
    //  killough 10/98: move to be made part of main test, add comment-handling

    if (sscanf(p, "%79s %1023[^\n]", name, strparm) != 2 || !isalnum(*name)
        || !(dp = M_LookupDefault(name))
        || (*strparm == '"') == (dp->type != string)
        || (wad && !dp->wad_allowed))
    {
        return 1;
    }

    // [FG] bind mapcolor options to the mapcolor preset menu item
    if (strncmp(name, "mapcolor_", 9) == 0
        || strcmp(name, "hudcolor_titl") == 0)
    {
        default_t *dp_preset = M_LookupDefault("mapcolor_preset");
        dp->setup_menu = dp_preset->setup_menu;
    }

    if (demo_version < DV_MBF && dp->setup_menu
        && !(dp->setup_menu->m_flags & S_COSMETIC))
    {
        return 1;
    }

    if (dp->type == string) // get a string default
    {
        int len = strlen(strparm) - 1;

        while (isspace(strparm[len]))
        {
            len--;
        }

        if (strparm[len] == '"')
        {
            len--;
        }

        strparm[len + 1] = 0;

        if (wad && !dp->modified)                 // Modified by wad
        {                                         // First time modified
            dp->modified = 1;                     // Mark it as modified
            dp->orig_default.s = dp->location->s; // Save original default
        }
        else
        {
            free(dp->location->s); // Free old value
        }

        dp->location->s = strdup(strparm + 1); // Change default value

        if (dp->current) // Current value
        {
            free(dp->current->s);                 // Free old value
            dp->current->s = strdup(strparm + 1); // Change current value
        }
    }
    else if (dp->type == number)
    {
        if (sscanf(strparm, "%i", &parm) != 1)
        {
            return 1; // Not A Number
        }

        // jff 3/4/98 range check numeric parameters
        if ((dp->limit.min == UL || dp->limit.min <= parm)
            && (dp->limit.max == UL || dp->limit.max >= parm))
        {
            if (wad)
            {
                if (!dp->modified) // First time it's modified by wad
                {
                    dp->modified = 1; // Mark it as modified
                    dp->orig_default.i = dp->location->i; // Save original default
                }
                if (dp->current) // Change current value
                {
                    dp->current->i = parm;
                }
            }
            dp->location->i = parm; // Change default
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
                    {
                        c = ',';
                    }
                    else if (c == '>')
                    {
                        c = '.';
                    }
                    else if (c == '+')
                    {
                        c = '=';
                    }

                    M_InputAddKey(dp->input_id, c);
                }
                else
                {
                    int value;
                    if ((value = M_GetKeyForName(buffer)) > 0)
                    {
                        M_InputAddKey(dp->input_id, value);
                    }
                    else if ((value = M_GetJoyBForName(buffer)) >= 0)
                    {
                        M_InputAddJoyB(dp->input_id, value);
                    }
                    else if ((value = M_GetMouseBForName(buffer)) >= 0)
                    {
                        M_InputAddMouseB(dp->input_id, value);
                    }
                }
            }

            scan = strtok(NULL, ",");
        } while (scan);
    }

    if (wad && dp->setup_menu)
    {
        dp->setup_menu->m_flags |= S_DISABLE;
    }

    return 0; // Success
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
            char *buf = NULL, *p,
                 *options = p = W_CacheLumpNum(lump, PU_STATIC);
            while (size > 0)
            {
                int len = 0;
                while (len < size && p[len++] && p[len - 1] != '\n')
                    ;
                if (len >= buflen)
                {
                    buf = I_Realloc(buf, buflen = len + 1);
                }
                strncpy(buf, p, len)[len] = 0;
                p += len;
                size -= len;
                M_ParseOption(buf, true);
            }
            free(buf);
            Z_ChangeTag(options, PU_CACHE);
        }
    }

    MN_Trans();     // reset translucency in case of change
    MN_ResetMenu(); // reset menu in case of change
}

//
// M_LoadDefaults
//

void M_LoadDefaults(void)
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
    {
        if (dp->type == string)
        {
            dp->location->s = strdup(dp->defaultvalue.s);
        }
        else if (dp->type == number)
        {
            dp->location->i = dp->defaultvalue.i;
        }
        else if (dp->type == input)
        {
            M_InputSetDefault(dp->input_id);
        }
    }

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

        if ((i = M_CheckParm("-config")) && i < myargc - 1)
        {
            defaultfile = strdup(myargv[i + 1]);
        }
        else
        {
            defaultfile = strdup(basedefault);
        }
    }

    // read the file in, overriding any set defaults
    //
    // killough 9/21/98: Print warning if file missing, and use fgets for
    // reading

    if ((f = M_fopen(defaultfile, "r")))
    {
        char s[256];

        while (fgets(s, sizeof s, f))
        {
            M_ParseOption(s, false);
        }
    }

    defaults_loaded = true; // killough 10/98

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
        I_Printf(VB_WARNING,
                 " Warning: Cannot read %s -- using built-in defaults\n",
                 defaultfile);
    }
}
