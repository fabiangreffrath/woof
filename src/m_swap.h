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
// DESCRIPTION:
//      Endianess handling, swapping 16bit and 32bit.
//
//-----------------------------------------------------------------------------

#ifndef __M_SWAP__
#define __M_SWAP__

#include <SDL3/SDL_endian.h> // [FG]

// Endianess handling.
// WAD files are stored little endian.
//
// killough 5/1/98:
// Replaced old code with inlined code which works regardless of endianess.
//

// Just use SDL's endianness swapping functions.

// These are deliberately cast to signed values; this is the behaviour
// of the macros in the original source and some code relies on it.

#define SHORT(x)  ((signed short) SDL_Swap16LE(x))
#define LONG(x)   ((signed int) SDL_Swap32LE(x))

#define SWAP_BE32(x) ((signed int) SDL_Swap32BE(x))

#endif

//----------------------------------------------------------------------------
//
// $Log: m_swap.h,v $
// Revision 1.3  1998/05/03  23:14:03  killough
// Make endian independent, beautify
//
// Revision 1.2  1998/01/26  19:27:15  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:03:08  rand
// Lee's Jan 19 sources
//
//
//----------------------------------------------------------------------------
