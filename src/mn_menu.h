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
// DESCRIPTION:
//   Menu widget stuff, episode selection and such.
//    
//-----------------------------------------------------------------------------

#ifndef __M_MENU__
#define __M_MENU__

#include "doomtype.h"
#include "doomdef.h"

struct event_s;

//
// MENUS
//
// Called by main loop,
// saves config file and calls I_Quit when user exits.
// Even when the menu is not displayed,
// this can resize the view and change game parameters.
// Does all the real work of the menu interaction.

boolean M_Responder(struct event_s *ev);

// Called by main loop,
// only used for menu (skull cursor) animation.

void M_Ticker (void);

// Called by main loop,
// draws the menus directly into the screen buffer.

void M_Drawer (void);

// Called by D_DoomMain,
// loads the config file.

void M_Init (void);

// Called by intro code to force menu up upon a keypress,
// does nothing if menu is already up.

void M_StartControlPanel (void);

void M_ForcedLoadGame(const char *msg); // killough 5/15/98: forced loadgames

extern int traditional_menu;  // display the menu traditional way

void M_Trans(void);          // killough 11/98: reset translucency

void M_ResetMenu(void);      // killough 11/98: reset main menu ordering

void M_ResetSetupMenu(void);

void M_ResetSetupMenuVideo(void);

void M_ResetTimeScale(void);

void M_DrawCredits(void);    // killough 11/98

void M_SetMenuFontSpacing(void);

void M_DisableVoxelsRenderingItem(void);

void M_InvulMode(void);

typedef struct mrect_s
{
    short x;
    short y;
    short w;
    short h;
} mrect_t;

typedef enum
{
    MENU_BG_OFF,
    MENU_BG_DARK,
    MENU_BG_TEXTURE,
} backdrop_t;

typedef enum
{
    MENU_NULL,
    MENU_UP,
    MENU_DOWN,
    MENU_LEFT,
    MENU_RIGHT,
    MENU_BACKSPACE,
    MENU_ENTER,
    MENU_ESCAPE,
    MENU_CLEAR
} menu_action_t;

typedef enum
{
    null_mode,
    mouse_mode,
    pad_mode,
    key_mode
} menu_input_mode_t;

extern menu_input_mode_t menu_input;

extern backdrop_t menu_backdrop;
extern boolean M_MenuIsShaded(void);

extern void M_SetQuickSaveSlot (int slot);

extern int resolution_scale;
extern int midi_player_menu;

void M_InitMenuStrings(void);

extern boolean StartsWithMapIdentifier (char *str);

extern boolean inhelpscreens;
extern short whichSkull; // which skull to draw (he blinks)
extern int saved_screenblocks;

void M_SizeDisplay(int choice);

void M_SetupNextMenuAlt(ss_types type);
boolean M_PointInsideRect(mrect_t *rect, int x, int y);
void M_ClearMenus(void);
void M_Back(void);

int  M_StringWidth(const char *string);
int  M_StringHeight(const char *string);

#endif

//----------------------------------------------------------------------------
//
// $Log: m_menu.h,v $
// Revision 1.4  1998/05/16  09:17:18  killough
// Make loadgame checksum friendlier
//
// Revision 1.3  1998/05/03  21:56:53  killough
// Add traditional_menu declaration
//
// Revision 1.2  1998/01/26  19:27:11  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:02:58  rand
// Lee's Jan 19 sources
//
//
//----------------------------------------------------------------------------
