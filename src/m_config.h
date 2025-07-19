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
//-----------------------------------------------------------------------------

#ifndef __M_CONFIG__
#define __M_CONFIG__

#include "doomdef.h"
#include "doomtype.h"

void M_LoadDefaults(void);
void M_SaveDefaults(void);
struct default_s *M_LookupDefault(const char *name);     // killough 11/98
boolean M_ParseOption(const char *name, boolean wad);    // killough 11/98
void M_LoadOptions(void);                                // killough 11/98

void M_InitConfig(void);

void M_BindNum(const char *name, void *location, void *current,
               int default_val, int min_val, int max_val,
               ss_types screen, wad_allowed_t wad,
               const char *help);

#define BIND_NUM(name, v, a, b, help) \
    M_BindNum(#name, &name, NULL, (v), (a), (b), ss_none, wad_no, help)

#define BIND_NUM_GENERAL(name, v, a, b, help) \
    M_BindNum(#name, &name, NULL, (v), (a), (b), ss_gen, wad_no, help)

#define BIND_NUM_SFX(name, v, a, b, help) \
    M_BindNum(#name, &name, NULL, (v), (a), (b), ss_sfx, wad_no, help)

#define BIND_NUM_MUSIC(name, v, a, b, help) \
    M_BindNum(#name, &name, NULL, (v), (a), (b), ss_music, wad_no, help)

void M_BindBool(const char *name, boolean *location, boolean *current,
                boolean default_val, ss_types screen, wad_allowed_t wad,
                const char *help);

#define BIND_BOOL(name, v, help) \
    M_BindBool(#name, &name, NULL, (v), ss_none, wad_no, help)

#define BIND_BOOL_GENERAL(name, v, help) \
    M_BindBool(#name, &name, NULL, (v), ss_gen, wad_no, help)

#define BIND_BOOL_SFX(name, v, help) \
    M_BindBool(#name, &name, NULL, (v), ss_sfx, wad_no, help)

#define BIND_BOOL_MUSIC(name, v, help) \
    M_BindBool(#name, &name, NULL, (v), ss_music, wad_no, help)

void M_BindStr(char *name, const char **location, const char *default_val,
               wad_allowed_t wad, const char *help);

void M_BindInput(const char *name, int input_id, const char *help);

void M_BindMenuNum(const char *name, void *location, int min_val, int max_val);

#define BIND_NUM_MENU(name, a, b) M_BindMenuNum(#name, &name, a, b)

void M_BindMenuBool(const char *name, boolean *current);

#define BIND_BOOL_MENU(name) M_BindMenuBool(#name, &name)

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
