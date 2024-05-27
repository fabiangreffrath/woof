//
// Copyright(C) 2024 ceski
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
//      Game Input Utility Functions
//

#include <math.h>

#include "d_main.h"
#include "g_game.h"
#include "i_gamepad.h"
#include "i_timer.h"
#include "i_video.h"
#include "r_main.h"

//
// Side Movement
//

static int RoundSide_Strict(double side)
{
    return lround(side * 0.5) * 2; // Even values only.
}

static int RoundSide_Full(double side)
{
    return lround(side);
}

int (*G_RoundSide)(double side);

void G_UpdateSideMove(void)
{
    if (strictmode || (netgame && !solonet))
    {
        G_RoundSide = RoundSide_Strict;
        sidemove = default_sidemove;
    }
    else
    {
        G_RoundSide = RoundSide_Full;
        sidemove = autostrafe50 ? forwardmove : default_sidemove;
    }
}

//
// Gamepad
//

static const int direction[] = {1, -1};
static double deltatics;

void G_UpdateDeltaTics(void)
{
    if (uncapped && raw_input)
    {
        static uint64_t last_time;
        const uint64_t current_time = I_GetTimeUS();

        if (input_ready)
        {
            const uint64_t delta_time = current_time - last_time;
            deltatics = (double)delta_time * TICRATE / 1000000.0;
            deltatics = BETWEEN(0.0, 1.0, deltatics);
        }
        else
        {
            deltatics = 0.0;
        }

        last_time = current_time;
    }
    else
    {
        deltatics = 1.0;
    }
}

double G_CalcControllerAngle(void)
{
    return (angleturn[1] * axes[AXIS_TURN] * direction[joy_invert_turn]
            * deltatics);
}

double G_CalcControllerPitch(void)
{
    return (angleturn[1] * axes[AXIS_LOOK] * direction[joy_invert_look]
            * deltatics * FRACUNIT);
}

int G_CalcControllerSideTurn(int speed)
{
    const int side = G_RoundSide(forwardmove[speed] * axes[AXIS_TURN]
                                 * direction[joy_invert_turn]);
    return BETWEEN(-forwardmove[speed], forwardmove[speed], side);
}

int G_CalcControllerSideStrafe(int speed)
{
    const int side = G_RoundSide(forwardmove[speed] * axes[AXIS_STRAFE]
                                 * direction[joy_invert_strafe]);
    return BETWEEN(-sidemove[speed], sidemove[speed], side);
}

int G_CalcControllerForward(int speed)
{
    const int forward = lroundf(forwardmove[speed] * axes[AXIS_FORWARD]
                                * direction[joy_invert_forward]);
    return BETWEEN(-forwardmove[speed], forwardmove[speed], forward);
}
