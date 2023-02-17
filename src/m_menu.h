// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id: m_menu.h,v 1.4 1998/05/16 09:17:18 killough Exp $
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
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 
//  02111-1307, USA.
//
// DESCRIPTION:
//   Menu widget stuff, episode selection and such.
//    
//-----------------------------------------------------------------------------

#ifndef __M_MENU__
#define __M_MENU__

#include "d_event.h"

//
// MENUS
//
// Called by main loop,
// saves config file and calls I_Quit when user exits.
// Even when the menu is not displayed,
// this can resize the view and change game parameters.
// Does all the real work of the menu interaction.

boolean M_Responder (event_t *ev);

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

void M_UpdateCriticalItems(void);

void M_DrawCredits(void);    // killough 11/98

// killough 8/15/98: warn about changes not being committed until next game
#define warn_about_changes(x) (warning_about_changes=(x), \
			       print_warning_about_changes = 2)

extern int warning_about_changes, print_warning_about_changes;

/////////////////////////////
//
// The following #defines are for the m_flags field of each item on every
// Setup Screen. They can be OR'ed together where appropriate

#define S_HILITE     0x1 // Cursor is sitting on this item
#define S_SELECT     0x2 // We're changing this item
#define S_TITLE      0x4 // Title item
#define S_YESNO      0x8 // Yes or No item
#define S_CRITEM    0x10 // Message color
#define S_AM_COLOR  0x20 // Automap color
#define S_CHAT_MACRO 0x40 // Chat String
#define S_RESET     0x80 // Reset to Defaults Button
#define S_PREV     0x100 // Previous menu exists
#define S_NEXT     0x200 // Next menu exists
#define S_INPUT    0x400 // Composite input
#define S_WEAP     0x800 // Weapon #
#define S_NUM     0x1000 // Numerical item
#define S_SKIP    0x2000 // Cursor can't land here
#define S_KEEP    0x4000 // Don't swap key out
#define S_END     0x8000 // Last item in list (dummy)
#define S_LEVWARN 0x10000// killough 8/30/98: Always warn about pending change
#define S_PRGWARN 0x20000// killough 10/98: Warn about change until next run
#define S_BADVAL  0x40000// killough 10/98: Warn about bad value
// removed
#define S_LEFTJUST 0x100000 // killough 10/98: items which are left-justified
#define S_CREDIT  0x200000  // killough 10/98: credit
#define S_BADVID  0x400000  // killough 12/98: video mode change error
#define S_CHOICE  0x800000  // [FG] selection of choices
#define S_DISABLE 0x1000000 // Disable item
#define S_COSMETIC 0x2000000 // Don't warn about change
#define S_THERMO  0x4000000 // Mini-thermo
#define S_NAME    0x8000000 // Player name
#define S_NEXT_LINE 0x10000000

// S_SHOWDESC  = the set of items whose description should be displayed
// S_SHOWSET   = the set of items whose setting should be displayed
// S_STRING    = the set of items whose settings are strings -- killough 10/98:
// S_HASDEFPTR = the set of items whose var field points to default array

#define S_COLOR (S_AM_COLOR|S_COSMETIC)

#define S_CHAT (S_CHAT_MACRO|S_COSMETIC)

#define S_SHOWDESC (S_TITLE|S_YESNO|S_CRITEM|S_COLOR|S_CHAT|S_RESET|S_PREV|S_NEXT|S_INPUT|S_WEAP|S_NUM|S_CREDIT|S_CHOICE|S_THERMO|S_NAME)

#define S_SHOWSET  (S_YESNO|S_CRITEM|S_COLOR|S_CHAT|S_INPUT|S_WEAP|S_NUM|S_CHOICE|S_THERMO|S_NAME)

#define S_STRING (S_CHAT_MACRO|S_NAME)

#define S_HASDEFPTR (S_STRING|S_YESNO|S_NUM|S_WEAP|S_COLOR|S_CRITEM|S_CHOICE|S_THERMO)

/////////////////////////////
//
// The setup_group enum is used to show which 'groups' keys fall into so
// that you can bind a key differently in each 'group'.

typedef enum {
  m_null,       // Has no meaning; not applicable
  m_scrn,       // A key can not be assigned to more than one action
  m_map,        // in the same group. A key can be assigned to one
  m_menu,       // action in one group, and another action in another.
} setup_group;

/////////////////////////////
//
// phares 4/17/98:
// State definition for each item.
// This is the definition of the structure for each setup item. Not all
// fields are used by all items.
//
// A setup screen is defined by an array of these items specific to
// that screen.
//
// killough 11/98:
//
// Restructured to allow simpler table entries, 
// and to Xref with defaults[] array in m_misc.c.
// Moved from m_menu.c to m_menu.h so that m_misc.c can use it.

typedef struct setup_menu_s
{
  const char  *m_text;  // text to display
  int         m_flags;  // phares 4/17/98: flag bits S_* (defined above)
  setup_group m_group;  // Group
  short       m_x;      // screen x position (left is 0)
  short       m_y;      // screen y position (top is 0)

  union  // killough 11/98: The first field is a union of several types
   {
     void      *var;    // generic variable
     int       *m_key;  // key value, or 0 if not shown
     char      *name;   // name
     struct default_s *def;      // default[] table entry
     struct setup_menu_s *menu;  // next or prev menu
  } var;

  int ident; // composite input
  void (*action)(void); // killough 10/98: function to call after changing
  const char **selectstrings; // [FG] selection of choices
} setup_menu_t;

typedef enum
{
  background_on,
  background_off,
  background_dark,
} background_t;

extern background_t menu_background;
extern boolean M_MenuIsShaded(void);

#define MAX_MIDI_PLAYER_MENU_ITEMS 128
extern void M_GetMidiDevices(void);

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
