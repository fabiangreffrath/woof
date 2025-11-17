//
//  Copyright (C) 1999 by
//   id Software, Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman
//  Copyright (C) 2023 by
//   Ryan Krafnick
//  Copyright (C) 2025 by
//   Guilherme Miranda
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
//   Generation of transparency lookup tables.
//

#include "doomtype.h"

#ifndef __R_TRANMAP__
#define __R_TRANMAP__

extern const byte *tranmap;
extern const byte *main_tranmap;
extern const byte *main_addimap;

// killough 3/6/98: translucency initialization
void R_InitTranMap(void);
byte *R_NormalTranMap(int alpha, boolean force);
#define GetNormalTranMap(alpha) R_NormalTranMap(alpha, false)

#endif
