//
// Copyright(C) 2026 Roman Fomin
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

#ifndef M_PROFILER_H
#define M_PROFILER_H

#include "config.h"

#ifdef HAVE_TRACY
    #include "tracy/TracyC.h"

    #define PROF_FRAME_MARK          TracyCFrameMark
    #define PROF_ZONE_BEGIN(ctx)     TracyCZone(ctx, 1)
    #define PROF_ZONE_BEGIN_N(ctx, name) TracyCZoneN(ctx, name, 1)
    #define PROF_ZONE_END(ctx)       TracyCZoneEnd(ctx)
    #define PROF_ALLOC(ptr, size)    TracyCAlloc(ptr, size)
    #define PROF_FREE(ptr)           TracyCFree(ptr)
    #define PROF_THREAD_NAME(name)   TracyCSetThreadName(name)
#else
    #define PROF_FRAME_MARK
    #define PROF_ZONE_BEGIN(ctx)
    #define PROF_ZONE_BEGIN_N(ctx, name)
    #define PROF_ZONE_END(ctx)
    #define PROF_ALLOC(ptr, size)
    #define PROF_FREE(ptr)
    #define PROF_THREAD_NAME(name)
#endif

#endif // M_PROFILER_H
