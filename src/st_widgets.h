//
// Copyright(C) 2024 Roman Fomin
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

#ifndef ST_WIDGETS_H
#define ST_WIDGETS_H

#include "doomtype.h"
#include "hu_message.h"

struct sbarelem_s;
struct sbe_widget_s;
struct player_s;

#define HU_MAXMESSAGES   20

typedef enum
{
    HUD_WIDGET_OFF,
    HUD_WIDGET_AUTOMAP,
    HUD_WIDGET_HUD,
    HUD_WIDGET_ALWAYS,
    HUD_WIDGET_ADVANCED,
} widgetstate_t;

extern boolean show_messages;
extern boolean show_toggle_messages;
extern boolean show_pickup_messages;

typedef enum
{
  SECRETMESSAGE_OFF,
  SECRETMESSAGE_ON,
  SECRETMESSAGE_COUNT
} secretmessage_t;

extern secretmessage_t hud_secret_message; // "A secret is revealed!" message

extern widgetstate_t hud_level_stats;
extern widgetstate_t hud_level_time;
extern widgetstate_t hud_player_coords;
extern int hudcolor_titl;
extern int hudcolor_xyco;

extern boolean hud_time_use;

extern struct sbarelem_s *st_time_elem, *st_cmd_elem;

extern boolean message_centered;

void ST_ResetTitle(void);
void ST_ResetMessages(void);

// Show the message that is still on the screen again.

void ST_ReviewMessages(void);

void ST_ClearLines(struct sbe_widget_s *widget);
void ST_AddLine(struct sbe_widget_s *widget, const char *string);
void ST_UpdateWidget(struct sbarelem_s *elem, struct player_s *player);

extern int speedometer;

extern int playback_tic, playback_totaltics;
boolean ST_DemoProgressBar(boolean force);

void ST_InitWidgets(void);
void ST_BindHUDVariables(void);

#endif
