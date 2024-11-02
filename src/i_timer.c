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

#include "SDL.h"

#include "doomdef.h"
#include "doomtype.h"
#include "i_system.h"
#include "m_fixed.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

HANDLE hTimer = NULL;
#else
#include <unistd.h>
#endif

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

static uint64_t GetPerfCounter_Scaled(void)
{
    uint64_t counter;

    counter = SDL_GetPerformanceCounter() * time_scale / 100;

    if (basecounter_scaled == 0)
    {
        basecounter_scaled = counter;
    }

    return counter - basecounter_scaled;
}

static uint32_t GetTimeMS_Scaled(void)
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

static int I_GetTime_Scaled(void)
{
    return MSToTic(GetTimeMS_Scaled());
}

static int fasttic;

static int I_GetTime_FastDemo(void)
{
    return fasttic++;
}

int (*I_GetTime)() = I_GetTime_Scaled;

static int I_GetFracTime_Scaled(void)
{
    return GetTimeMS_Scaled() * TICRATE % 1000 * FRACUNIT / 1000;
}

// During a fast demo, no time elapses in between ticks

static int I_GetFracTime_FastDemo(void)
{
    return 0;
}

int (*I_GetFracTime)(void) = I_GetFracTime_Scaled;

void I_ShutdownTimer(void)
{
    SDL_QuitSubSystem(SDL_INIT_TIMER);
}

void I_InitTimer(void)
{
    if (SDL_Init(SDL_INIT_TIMER) < 0)
    {
        I_Error("I_InitTimer: Failed to initialize timer: %s", SDL_GetError());
    }

#ifdef _WIN32
    // Create an unnamed waitable timer.
    hTimer = NULL;
  #ifdef HAVE_HIGH_RES_TIMER
    hTimer = CreateWaitableTimerEx(NULL, NULL,
                                   CREATE_WAITABLE_TIMER_MANUAL_RESET
                                   | CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
                                   TIMER_ALL_ACCESS);
  #endif
    if (hTimer == NULL)
    {
        hTimer = CreateWaitableTimer(NULL, TRUE, NULL);
    }
    if (hTimer == NULL)
    {
        I_Error("I_InitTimer: CreateWaitableTimer failed");
    }
#endif

    I_AtExit(I_ShutdownTimer, true);

    basefreq = SDL_GetPerformanceFrequency();

    I_GetTime = I_GetTime_Scaled;
    I_GetFracTime = I_GetFracTime_Scaled;
}

void I_SetTimeScale(int scale)
{
    uint64_t counter;

    counter = GetPerfCounter_Scaled();

    time_scale = scale;

    basecounter_scaled += (GetPerfCounter_Scaled() - counter);
}

void I_SetFastdemoTimer(boolean on)
{
    if (on)
    {
        fasttic = I_GetTime_Scaled();

        I_GetTime = I_GetTime_FastDemo;
        I_GetFracTime = I_GetFracTime_FastDemo;
    }
    else if (I_GetTime == I_GetTime_FastDemo)
    {
        uint64_t counter;

        counter = TicToCounter(I_GetTime_FastDemo());

        basecounter_scaled += (GetPerfCounter_Scaled() - counter);

        I_GetTime = I_GetTime_Scaled;
        I_GetFracTime = I_GetFracTime_Scaled;
    }
}

// Sleep for a specified number of ms

void I_Sleep(int ms)
{
    SDL_Delay(ms);
}

void I_SleepUS(uint64_t us)
{
#if defined(_WIN32)
    LARGE_INTEGER liDueTime;
    liDueTime.QuadPart = -(LONGLONG)(us * 10);
    if (SetWaitableTimer(hTimer, &liDueTime, 0, NULL, NULL, 0))
    {
        WaitForSingleObject(hTimer, INFINITE);
    }
#else
    usleep(us);
#endif
}

void I_WaitVBL(int count)
{
    // haleyjd
    I_Sleep((count * 500) / TICRATE);
}
