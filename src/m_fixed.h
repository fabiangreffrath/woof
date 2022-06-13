// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// $Id: m_fixed.h,v 1.5 1998/05/10 23:42:22 killough Exp $
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
// DESCRIPTION:
//      Fixed point arithemtics, implementation.
//
//-----------------------------------------------------------------------------

#ifndef __M_FIXED__
#define __M_FIXED__

#include <stdlib.h> // abs()
#include "doomtype.h"
#include "i_system.h"

//
// Fixed point, 32bit as 16.16.
//

#define FRACBITS 16
#define FRACUNIT (1<<FRACBITS)
#define FIXED2DOUBLE(x) (x/(double)FRACUNIT)

typedef int fixed_t;

//
// Fixed Point Multiplication
//

inline static fixed_t FixedMul(fixed_t a, fixed_t b)
{
  return (fixed_t)((Long64) a*b >> FRACBITS);
}

//
// Fixed Point Division
//

inline static fixed_t FixedDiv(fixed_t a, fixed_t b)
{
  // [FG] avoid 31-bit shift (from Chocolate Doom)
  return (abs(a)>>14) >= abs(b) ? ((a^b) < 0 ? D_MININT : D_MAXINT) :
    (fixed_t)(((Long64) a << FRACBITS) / b);
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

