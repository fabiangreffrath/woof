// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id: dstrings.h,v 1.5 1998/05/04 22:00:43 thldrmn Exp $
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
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 
//  02111-1307, USA.
//
//
//
// DESCRIPTION:
//   DOOM strings, by language.
//   Note:  In BOOM, some new strings hav ebeen defined that are
//          not found in the French version.  A better approach is 
//          to create a BEX text-replacement file for other
//          languages since any language can be supported that way
//          without recompiling the program.
//
//-----------------------------------------------------------------------------


#ifndef __DSTRINGS__
#define __DSTRINGS__

// All important printed strings.
// Language selection (message strings).
// Use -DFRENCH etc.

#ifdef FRENCH
#include "d_french.h"
#else
#include "d_englsh.h"
#endif

// Note this is not externally modifiable through DEH/BEX
// Misc. other strings.
// #define SAVEGAMENAME  "boomsav"      /* killough 3/22/98 */
// Ty 05/04/98 - replaced with a modifiable string, see d_deh.c
                                                               

//
// File locations,
//  relative to current position.
// Path names are OS-sensitive.
//
#define DEVMAPS "devmaps"
#define DEVDATA "devdata"


// Not done in french?

// QuitDOOM messages

// killough 1/18/98: 
// replace hardcoded limit with extern var (silly hack, I know)

#include <stddef.h>

extern const size_t NUM_QUITMESSAGES;  // Calculated in dstrings.c

extern const char* const endmsg[];   // killough 1/18/98 const added


#endif

//----------------------------------------------------------------------------
//
// $Log: dstrings.h,v $
// Revision 1.5  1998/05/04  22:00:43  thldrmn
// savegamename globalization
//
// Revision 1.3  1998/03/23  03:12:58  killough
// Rename doomsav to boomsav
//
// Revision 1.2  1998/01/26  19:26:45  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:02:51  rand
// Lee's Jan 19 sources
//
//
//----------------------------------------------------------------------------
