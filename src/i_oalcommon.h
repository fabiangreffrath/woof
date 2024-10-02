//
// Copyright(C) 2023 Roman Fomin
// Copyright(C) 2023 ceski
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// DESCRIPTION:
//      OpenAL sound and equalizer.
//

#ifndef __I_OALCOMMON__
#define __I_OALCOMMON__

#include "al.h"
#include "alc.h"

#include <math.h>

#include "doomtype.h"

// C doesn't allow casting between function and non-function pointer types, so
// with C99 we need to use a union to reinterpret the pointer type. Pre-C99
// still needs to use a normal cast and live with the warning (C++ is fine with
// a regular reinterpret_cast).
#if __STDC_VERSION__ >= 199901L
#  define FUNCTION_CAST(T, ptr) (union{void *p; T f;}){ptr}.f
#else
#  define FUNCTION_CAST(T, ptr) (T)(ptr)
#endif

#define ALFUNC(T, ptr) (ptr = FUNCTION_CAST(T, alGetProcAddress(#ptr)))

#define DB_TO_GAIN(db) powf(10.0f, (db) / 20.0f)

typedef struct oal_system_s
{
    ALCdevice *device;
    ALCcontext *context;
    ALuint *sources;
    boolean SOFT_source_spatialize;
    boolean EXT_EFX;
    boolean EXT_SOURCE_RADIUS;
    ALfloat absorption;
} oal_system_t;

extern oal_system_t *oal;

#endif
