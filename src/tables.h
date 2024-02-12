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
//      Lookup tables.
//      Do not try to look them up :-).
//      In the order of appearance:
//
//      int finetangent[4096]   - Tangens LUT.
//       Should work with BAM fairly well (12 of 16bit,
//      effectively, by shifting).
//
//      int finesine[10240]             - Sine lookup.
//       Guess what, serves as cosine, too.
//       Remarkable thing is, how to use BAMs with this?
//
//      int tantoangle[2049]    - ArcTan LUT,
//        maps tan(angle) to angle fast. Gotta search.
//
//-----------------------------------------------------------------------------

#ifndef __TABLES__
#define __TABLES__

#include "doomtype.h" // inline
#include "m_fixed.h"

#define FINEANGLES              8192
#define FINEMASK                (FINEANGLES-1)

// 0x100000000 to 0x2000
#define ANGLETOFINESHIFT        19

// Effective size is 10240.
extern const fixed_t finesine[5*FINEANGLES/4];

// Re-use data, is just PI/2 pahse shift.
extern const fixed_t *const finecosine;

// Effective size is 4096.
extern const fixed_t finetangent[FINEANGLES/2];

// Binary Angle Measument, BAM.
#define ANG45   0x20000000
#define ANG75   0x35555555
#define ANG90   0x40000000
#define ANG180  0x80000000
#define ANG270  0xc0000000
#define ANG1    (ANG45/45)
#define ANG60   (ANG180/3)
#define ANGLE_MAX 0xffffffff

#define SLOPERANGE 2048
#define SLOPEBITS    11
#define DBITS      (FRACBITS-SLOPEBITS)

typedef unsigned angle_t;

// Effective size is 2049;
// The +1 size is to handle the case when x==y without additional checking.

extern const angle_t tantoangle[SLOPERANGE+1];

// Utility function, called by R_PointToAngle.
int SlopeDiv(unsigned num, unsigned den);
int SlopeDivCrispy(unsigned num, unsigned den);

// mbf21: More utility functions, courtesy of Quasar (James Haley).
// These are straight from Eternity so demos stay in sync.
inline static angle_t FixedToAngle(fixed_t a)
{
  return (angle_t)(((uint64_t)a * ANG1) >> FRACBITS);
}

inline static fixed_t AngleToFixed(angle_t a)
{
  return (fixed_t)(((uint64_t)a << FRACBITS) / ANG1);
}

// [XA] Clamped angle->slope, for convenience
inline static fixed_t AngleToSlope(int a)
{
  if (a > ANG90)
    return finetangent[0];
  else if (-a > ANG90)
    return finetangent[FINEANGLES / 2 - 1];
  else
    return finetangent[(ANG90 - a) >> ANGLETOFINESHIFT];
}

// [XA] Ditto, using fixed-point-degrees input
inline static fixed_t DegToSlope(fixed_t a)
{
  if (a >= 0)
    return AngleToSlope(FixedToAngle(a));
  else
    return AngleToSlope(-(int)FixedToAngle(-a));
}

#endif

//----------------------------------------------------------------------------
//
// $Log: tables.h,v $
// Revision 1.3  1998/05/03  22:58:56  killough
// beautification
//
// Revision 1.2  1998/01/26  19:27:58  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:03:05  rand
// Lee's Jan 19 sources
//
//----------------------------------------------------------------------------
