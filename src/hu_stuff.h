//  Copyright (C) 1999 by
//  id Software, Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman
//  Copyright (C) 2023 Fabian Greffrath
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
// DESCRIPTION:  Head up display
//
//-----------------------------------------------------------------------------

#ifndef __HU_STUFF_H__
#define __HU_STUFF_H__

#include "d_event.h"
#include "r_defs.h"
#include "hu_lib.h"

#define HU_BROADCAST    5

#define HU_MSGTIMEOUT   (4*TICRATE)

//
// Heads up text
//
void HU_Init(void);
void HU_Start(void);
void HU_disable_all_widgets (void);

boolean HU_Responder(event_t* ev);

void HU_Ticker(void);
void HU_Drawer(void);
char HU_dequeueChatChar(void);
void HU_Erase(void);

boolean HU_DemoProgressBar(boolean force);

void HU_ResetMessageColors(void);

// killough 5/2/98: moved from m_misc.c:

//jff 2/16/98 hud supported automap colors added
extern int hudcolor_titl;   // color range of automap level title
extern int hudcolor_xyco;   // color range of new coords on automap
//jff 2/16/98 hud text colors, controls added
extern int hudcolor_mesg;   // color range of scrolling messages
extern int hudcolor_chat;   // color range of chat lines
//jff 2/26/98 hud message list color and background enable
extern int hud_msg_lines;   // number of message lines in window up to 16
extern int message_list;    // killough 11/98: whether message list is active
extern int message_timer;   // killough 11/98: timer used for normal messages
extern int chat_msg_timer;  // killough 11/98: timer used for chat messages
//jff 2/23/98 hud is currently displayed
extern int hud_displayed;   // hud is displayed
//jff 2/18/98 hud/status control
extern int hud_active;      // hud mode 0=off, 1=small, 2=full
extern int hud_secret_message; // "A secret is revealed!" message
extern int hud_player_coords, hud_level_stats, hud_level_time;
extern int hud_widget_font;
extern int hud_widescreen_widgets;
extern int hud_draw_bargraphs;
extern int hud_threelined_widgets;
extern boolean message_centered; // center messages
extern boolean message_colorized; // colorize player messages

extern int playback_tic, playback_totaltics;

extern int crispy_hud;

extern int hud_crosshair;
extern boolean hud_crosshair_health;

typedef enum
{
  crosstarget_off,
  crosstarget_highlight,
  crosstarget_health, // [Alaux] Color crosshair by target health
} crosstarget_t;
extern crosstarget_t hud_crosshair_target;

// [Alaux] Lock crosshair on target
extern boolean hud_crosshair_lockon;
extern mobj_t *crosshair_target;
void HU_UpdateCrosshairLock(int x, int y);
void HU_DrawCrosshair(void);

extern int hud_crosshair_color;
extern int hud_crosshair_target_color;

#define HU_CROSSHAIRS 10
extern const char *crosshair_nam[HU_CROSSHAIRS];
extern const char *crosshair_str[HU_CROSSHAIRS+1];

#endif

//----------------------------------------------------------------------------
//
// $Log: hu_stuff.h,v $
// Revision 1.6  1998/05/10  19:03:50  jim
// formatted/documented hu_stuff
//
// Revision 1.5  1998/05/03  22:25:03  killough
// add external declarations for hud options
//
// Revision 1.4  1998/02/18  00:59:04  jim
// Addition of HUD
//
// Revision 1.3  1998/02/15  02:48:12  phares
// User-defined keys
//
// Revision 1.2  1998/01/26  19:26:54  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:02:56  rand
// Lee's Jan 19 sources
//
//
//----------------------------------------------------------------------------
