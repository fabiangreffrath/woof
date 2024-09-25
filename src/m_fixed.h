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
//      Fixed point arithemtics, implementation.
//
//-----------------------------------------------------------------------------

#ifndef __M_FIXED__
#define __M_FIXED__

#include <limits.h>
#include <stdlib.h> // abs()
#include <stdint.h> // int64_t

#include "config.h"

#if defined(HAVE__DIV64)

  #define div64_32(a, b) _div64((a), (b), NULL)

#else

  #define div64_32(a, b) ((fixed_t)((a) / (b)))

#endif

//
// Fixed point, 32bit as 16.16.
//

#define FRACBITS 16
#define FRACUNIT (1<<FRACBITS)
#define FIXED2DOUBLE(x) ((x)/(double)FRACUNIT)
#define FRACMASK (FRACUNIT - 1)
#define FRACFILL(x, o) ((x) | ((o) < 0 ? (FRACMASK << (32 - FRACBITS)) : 0))

#define IntToFixed(x) ((x) << FRACBITS)
#define FixedToInt(x) FRACFILL((x) >> FRACBITS, (x))

typedef int fixed_t;

//
// Fixed Point Multiplication
//

inline static fixed_t FixedMul(fixed_t a, fixed_t b)
{
    return (fixed_t)((int64_t) a * b >> FRACBITS);
}

inline static int64_t FixedMul64(int64_t a, int64_t b)
{
    return (a * b >> FRACBITS);
}

//
// Fixed Point Division
//

inline static fixed_t FixedDiv(fixed_t a, fixed_t b)
{
    // [FG] avoid 31-bit shift (from Chocolate Doom)
    if ((abs(a) >> 14) >= abs(b))
    {
        return (a ^ b) < 0 ? INT_MIN : INT_MAX;
    }
    else
    {
        return div64_32((int64_t) a << FRACBITS, b);
    }
}

#endif

//----------------------------------------------------------------------------
//
// $Log: m_fixed.h,v $
// Revision 1.5  1998/05/10  23:42:22  killough
// Add inline assembly for djgpp (x86) target
//
// Revision 1.4  1998/04/27  01:53:37  killough
// Make gcc extensions #ifdef'ed
//
// Revision 1.3  1998/02/02  13:30:35  killough
// move fixed point arith funcs to m_fixed.h
//
// Revision 1.2  1998/01/26  19:27:09  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:02:53  rand
// Lee's Jan 19 sources
//
//----------------------------------------------------------------------------

