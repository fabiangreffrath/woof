//
//  Copyright (C) 1999 by
//  id Software, Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman
//  Copyright (C) 2022 Roman Fomin
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
//      Timer functions.
//

#include <SDL3/SDL.h>

#include "doomdef.h"
#include "doomtype.h"
#include "m_fixed.h"

static uint64_t basecounter = 0;
static uint64_t basecounter_scaled = 0;
static uint64_t basefreq = 0;

static int MSToTic(uint32_t time)
{
    return time * TICRATE / 1000;
}

static uint64_t TicToCounter(int tic)
{
    return (uint64_t)tic * basefreq / TICRATE;
}

int I_GetTimeMS(void)
{
    uint64_t counter = SDL_GetPerformanceCounter();

    if (basecounter == 0)
    {
        basecounter = counter;
    }

    return ((counter - basecounter) * 1000ull) / basefreq;
}

uint64_t I_GetTimeUS(void)
{
    uint64_t counter = SDL_GetPerformanceCounter();

    if (basecounter == 0)
    {
        basecounter = counter;
    }

    return ((counter - basecounter) * 1000000ull) / basefreq;
}

int time_scale = 100;

static uint64_t GetPerfCounterScaled(void)
{
    uint64_t counter;

    counter = SDL_GetPerformanceCounter() * time_scale / 100;

    if (basecounter_scaled == 0)
    {
        basecounter_scaled = counter;
    }

    return counter - basecounter_scaled;
}

static uint32_t GetTimeMilliSecondScaled(void)
{
    uint64_t counter;

    counter = SDL_GetPerformanceCounter() * time_scale / 100;

    if (basecounter_scaled == 0)
    {
        basecounter_scaled = counter;
    }

    return ((counter - basecounter_scaled) * 1000ull) / basefreq;
}

int I_GetTime_RealTime(void)
{
    return MSToTic(I_GetTimeMS());
}

static int GetTimeScaled(void)
{
    return MSToTic(GetTimeMilliSecondScaled());
}

static int fasttic;

static int GetTimeFastDemo(void)
{
    return fasttic++;
}

int (*I_GetTime)() = GetTimeScaled;

static int GetFracTimeScaled(void)
{
    return GetTimeMilliSecondScaled() * TICRATE % 1000 * FRACUNIT / 1000;
}

// During a fast demo, no time elapses in between ticks

static int GetFracTimeFastDemo(void)
{
    return 0;
}

int (*I_GetFracTime)(void) = GetFracTimeScaled;

void I_InitTimer(void)
{
    basefreq = SDL_GetPerformanceFrequency();

    I_GetTime = GetTimeScaled;
    I_GetFracTime = GetFracTimeScaled;
}

void I_SetTimeScale(int scale)
{
    uint64_t counter;

    counter = GetPerfCounterScaled();

    time_scale = scale;

    basecounter_scaled += (GetPerfCounterScaled() - counter);
}

void I_SetFastdemoTimer(boolean on)
{
    if (on)
    {
        fasttic = GetTimeScaled();

        I_GetTime = GetTimeFastDemo;
        I_GetFracTime = GetFracTimeFastDemo;
    }
    else if (I_GetTime == GetTimeFastDemo)
    {
        uint64_t counter;

        counter = TicToCounter(GetTimeFastDemo());

        basecounter_scaled += (GetPerfCounterScaled() - counter);

        I_GetTime = GetTimeScaled;
        I_GetFracTime = GetFracTimeScaled;
    }
}

// Sleep for a specified number of ms

void I_Sleep(int ms)
{
    SDL_Delay(ms);
}

void I_SleepUS(uint64_t us)
{
    SDL_DelayPrecise(us * 1000ull);
}

void I_WaitVBL(int count)
{
    // haleyjd
    I_Sleep((count * 500) / TICRATE);
}
