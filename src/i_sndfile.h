//
//  Copyright (C) 1999 by
//  id Software, Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman
//  Copyright (C) 2023 Fabian Greffrath
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
//      Load sound lumps with libsndfile.

#ifndef __I_SNDFILE__
#define __I_SNDFILE__

#include "al.h"

#include "doomtype.h"

#define FADETIME 1000 // microseconds

boolean I_SND_LoadFile(void *data, ALenum *format, byte **wavdata,
                       ALsizei *size, ALsizei *freq, boolean looping);

#endif
