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

#include "i_timer.h"
#include "m_fixed.h"
#include "doomstat.h"

static int MSToTic(Uint32 time)
{
    return time * TICRATE / 1000;
}

static Uint32 TicToMS(int tic)
{
    return (Uint32)tic * 1000 / TICRATE;
}

static Uint32 basetime = 0;

int I_GetTimeMS(void)
{
    Uint32 time;

    time = SDL_GetTicks();

    if (basetime == 0)
        basetime = time;

    return time - basetime;
}

static int time_scale = 100;

static Uint32 GetTimeMS_Scaled(void)
{
    Uint32 time;

    if (time_scale == 100)
    {
        time = SDL_GetTicks();
    }
    else
    {
        time = SDL_GetTicks() * time_scale / 100;
    }

    if (basetime == 0)
        basetime = time;

    return time - basetime;
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

void I_InitTimer(void)
{
    if (fastdemo)
    {
        I_GetTime = I_GetTime_FastDemo;
        I_GetFracTime = I_GetFracTime_FastDemo;
    }
    else
    {
        I_GetTime = I_GetTime_Scaled;
        I_GetFracTime = I_GetFracTime_Scaled;
    }
}

void I_SetTimeScale(int scale)
{
    Uint32 time;

    time = GetTimeMS_Scaled();

    time_scale = scale;

    basetime += (GetTimeMS_Scaled() - time);
}

void I_SetFastdemoTimer(void)
{
    Uint32 time;

    if (fastdemo)
    {
        time = GetTimeMS_Scaled();

        fasttic = MSToTic(time);
    }
    else
    {
        time = TicToMS(I_GetTime_FastDemo());

        basetime += (GetTimeMS_Scaled() - time);
    }

    I_InitTimer();
}

// [FG] toggle demo warp mode
void I_EnableWarp(boolean warp)
{
    static int (*I_GetTime_old)() = I_GetTime_Scaled;
    static boolean nodrawers_old;
    static boolean nomusicparm_old, nosfxparm_old;

    if (warp)
    {
        I_GetTime_old = I_GetTime;
        nodrawers_old = nodrawers;
        nomusicparm_old = nomusicparm;
        nosfxparm_old = nosfxparm;

        I_GetTime = I_GetTime_FastDemo;
        nodrawers = true;
        nomusicparm = true;
        nosfxparm = true;
    }
    else
    {
        I_GetTime = I_GetTime_old;
        D_StartGameLoop();
        nodrawers = nodrawers_old;
        nomusicparm = nomusicparm_old;
        nosfxparm = nosfxparm_old;
    }
}


// Sleep for a specified number of ms

void I_Sleep(int ms)
{
    SDL_Delay(ms);
}

void I_WaitVBL(int count)
{
    // haleyjd
    I_Sleep((count * 500) / TICRATE);
}
