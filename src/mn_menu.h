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

#ifndef __M_MENU__
#define __M_MENU__

#include "doomtype.h"

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

void M_Ticker(void);

// Called by main loop,
// draws the menus directly into the screen buffer.

void M_Drawer(void);

// Called by D_DoomMain,
// loads the config file.

void M_Init(void);

// Called by intro code to force menu up upon a keypress,
// does nothing if menu is already up.

void MN_StartControlPanel(void);

void MN_ForcedLoadGame(const char *msg); // killough 5/15/98: forced loadgames
void MN_Trans(void);     // killough 11/98: reset translucency
void MN_ResetMenu(void); // killough 11/98: reset main menu ordering
void MN_SetupResetMenu(void);
void MN_ResetTimeScale(void);
void MN_DrawCredits(void); // killough 11/98
void MN_SetHUFontKerning(void);
void MN_DisableVoxelsRenderingItem(void);
void MN_UpdateDynamicResolutionItem(void);
void MN_DisableResolutionScaleItem(void);
void MN_UpdateFpsLimitItem(void);

extern int traditional_menu; // display the menu traditional way

typedef enum
{
    MENU_BG_OFF,
    MENU_BG_DARK,
    MENU_BG_TEXTURE,
} backdrop_t;

extern backdrop_t menu_backdrop;
boolean MN_MenuIsShaded(void);

void MN_SetQuickSaveSlot(int slot);

void MN_InitMenuStrings(void);

boolean MN_StartsWithMapIdentifier(char *str);

extern boolean inhelpscreens;

int MN_GetPixelWidth(const char *ch);
void MN_DrawString(int cx, int cy, int color, const char *ch);

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
