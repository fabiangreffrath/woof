//
//  Copyright (C) 1999 by
//  id Software, Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman
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

#ifndef MN_INTERNAL_H
#define MN_INTERNAL_H

typedef struct
{
    short x;
    short y;
    short w;
    short h;
} mrect_t;

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

typedef enum
{
    MENU_HELP_OFF,
    MENU_HELP_AUTO,
    MENU_HELP_KEY,
    MENU_HELP_PAD
} menu_help_t;

extern menu_input_mode_t help_input, old_help_input; // pad_mode or key_mode.
extern menu_input_mode_t menu_input, old_menu_input;

void M_ChooseSkill(int choice);

extern int maxscreenblocks;

extern short whichSkull; // which skull to draw (he blinks)

extern int warning_about_changes, print_warning_about_changes;

extern const char *gamma_strings[];
void MN_DrawDelVerify(void);

#define M_X_CENTER (-1)
void MN_DrawTitle(int x, int y, const char *patch, const char *alttext);

#endif
