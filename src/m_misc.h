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
//
//    
//-----------------------------------------------------------------------------

#ifndef __M_MISC__
#define __M_MISC__

#include "doomtype.h"
#include "doomdef.h"

#include "m_input.h"

//
// MISC
//

boolean M_WriteFile(const char *name, void *source, int length);
int M_ReadFile(const char *name, byte **buffer);
void M_ScreenShot(void);
void M_LoadDefaults(void);
void M_SaveDefaults(void);
int M_DrawText(int x,int y,boolean direct, char *string);
struct default_s *M_LookupDefault(const char *name);     // killough 11/98
boolean M_ParseOption(const char *name, boolean wad);    // killough 11/98
void M_LoadOptions(void);                                // killough 11/98

// phares 4/21/98:
// Moved from m_misc.c so m_menu.c could see it.
//
// killough 11/98: totally restructured

// [FG] use a union of integer and string pointer to store config values, instead
// of type-punning string pointers to integers which won't work on 64-bit systems anyway

typedef union config_u
{
  int i;
  char *s;
} config_t;

typedef struct default_s
{
  const char *const name;                   // name
  config_t *const location;                 // default variable
  config_t *const current;                  // possible nondefault variable
  config_t  const defaultvalue;             // built-in default value
  struct {int min, max;} const limit;       // numerical limits
  enum {number, string, input} const type;  // type
  ss_types const setupscreen;               // setup screen this appears on
  enum {wad_no, wad_yes} const wad_allowed; // whether it's allowed in wads
  const char *const help;                   // description of parameter

  int ident;
  input_value_t inputs[NUM_INPUTS];

  // internal fields (initialized implicitly to 0) follow

  struct default_s *first, *next;           // hash table pointers
  int modified;                             // Whether it's been modified
  config_t orig_default;                    // Original default, if modified
  struct setup_menu_s *setup_menu;          // Xref to setup menu item, if any
} default_t;

#define UL (-123456789) /* magic number for no min or max for parameter */

#endif

//----------------------------------------------------------------------------
//
// $Log: m_misc.h,v $
// Revision 1.4  1998/05/05  19:56:06  phares
// Formatting and Doc changes
//
// Revision 1.3  1998/04/22  13:46:17  phares
// Added Setup screen Reset to Defaults
//
// Revision 1.2  1998/01/26  19:27:12  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:02:58  rand
// Lee's Jan 19 sources
//
//
//----------------------------------------------------------------------------
