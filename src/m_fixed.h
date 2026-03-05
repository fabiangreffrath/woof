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
#include <stdint.h> // int64_t
#include <stdlib.h> // abs()

//
// Fixed point, 32bit as 16.16.
//

typedef int fixed_t;

#define FRACBITS 16
#define FRACUNIT (1 << FRACBITS)
#define FRACMASK (FRACUNIT - 1)

inline static int32_t shiftleft32(int32_t x, int shift)
{
    return (int32_t)((uint32_t)x << shift);
}

inline static int64_t shiftleft64(int64_t x, int shift)
{
    return (int64_t)((uint64_t)x << shift);
}

inline static fixed_t IntToFixed(int32_t x)
{
    return shiftleft32(x, FRACBITS);
}

inline static int32_t FixedToInt(fixed_t x)
{
    return x >> FRACBITS;
}

inline static fixed_t DoubleToFixed(double x)
{
    return (fixed_t)(x * FRACUNIT);
}

inline static double FixedToDouble(fixed_t x)
{
    return (double)x / FRACUNIT;
}

//
// Fixed Point Multiplication
//

inline static fixed_t FixedMul(fixed_t a, fixed_t b)
{
    return ((int64_t)a * b) >> FRACBITS;
}

inline static int64_t FixedMul64(int64_t a, int64_t b)
{
    return (a * b) >> FRACBITS;
}

//
// Fixed Point Division
//

inline static int32_t div64_32(int64_t a, int32_t b)
{
#if defined(_MSC_VER)
    return _div64(a, b, NULL);
#else
    return (int32_t)(a / b);
#endif
}

inline static fixed_t FixedDiv(fixed_t a, fixed_t b)
{
    // Get absolute values without triggering UBSan
    int abs_a, abs_b;

    // For 'a': handle INT_MIN specially
    if (a == INT_MIN)
    {
        abs_a = INT_MIN; // Matches original abs(INT_MIN) behavior
    }
    else
    {
        abs_a = abs(a);
    }

    // For 'b': handle INT_MIN specially
    if (b == INT_MIN)
    {
        abs_b = INT_MIN; // Matches original abs(INT_MIN) behavior
    }
    else
    {
        abs_b = abs(b);
    }

    // Original overflow check
    if ((abs_a >> 14) >= abs_b)
    {
        return (a ^ b) < 0 ? INT_MIN : INT_MAX;
    }

    return div64_32(shiftleft64(a, FRACBITS), b);
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
