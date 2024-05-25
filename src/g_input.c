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
#include <string.h>

#include "d_main.h"
#include "g_game.h"
#include "i_gamepad.h"
#include "i_timer.h"
#include "i_video.h"
#include "p_mobj.h"
#include "r_state.h"

//
// Local View
//

void (*G_UpdateLocalView)(void);

void G_ClearLocalView(void)
{
    memset(&localview, 0, sizeof(localview));
}

static void UpdateLocalView_FakeLongTics(void)
{
    localview.angle -= localview.angleoffset << FRACBITS;
    localview.rawangle -= localview.angleoffset;
    localview.angleoffset = 0;
    localview.pitch = 0;
    localview.rawpitch = 0.0;
}

//
// Side Movement
//

static int (*RoundSide)(double side);

static int RoundSide_Strict(double side)
{
    return lround(side * 0.5) * 2; // Even values only.
}

static int RoundSide_Full(double side)
{
    return lround(side);
}

void G_UpdateSideMove(void)
{
    if (strictmode || (netgame && !solonet))
    {
        RoundSide = RoundSide_Strict;
        sidemove = default_sidemove;
    }
    else
    {
        RoundSide = RoundSide_Full;
        sidemove = autostrafe50 ? forwardmove : default_sidemove;
    }
}

//
// Error Accumulation
//

// Round to nearest 256 for single byte turning. From Chocolate Doom.
#define BYTE_TURN(x) (((x) + 128) & 0xFF00)

typedef struct
{
    double angle;
    double pitch;
    double side;
    double vert;
} carry_t;

static carry_t prevcarry, carry;
static short (*CarryAngleTic)(double angle);
static short (*CarryAngle)(double angle);

void G_UpdateCarry(void)
{
    prevcarry = carry;
}

void G_ClearCarry(void)
{
    memset(&prevcarry, 0, sizeof(prevcarry));
    memset(&carry, 0, sizeof(carry));
}

static int CarryError(double value, const double *prevcarry, double *carry)
{
    const double desired = value + *prevcarry;
    const int actual = lround(desired);
    *carry = desired - actual;
    return actual;
}

static short CarryAngleTic_Full(double angle)
{
    return CarryError(angle, &prevcarry.angle, &carry.angle);
}

static short CarryAngle_Full(double angle)
{
    const short fullres = CarryAngleTic_Full(angle);
    localview.angle = fullres << FRACBITS;
    return fullres;
}

static short CarryAngle_FakeLongTics(double angle)
{
    return (localview.angleoffset = BYTE_TURN(CarryAngle_Full(angle)));
}

static short CarryAngleTic_LowRes(double angle)
{
    const double fullres = angle + prevcarry.angle;
    const short lowres = BYTE_TURN((short)lround(fullres));
    carry.angle = fullres - lowres;
    return lowres;
}

static short CarryAngle_LowRes(double angle)
{
    const short lowres = CarryAngleTic_LowRes(angle);
    localview.angle = lowres << FRACBITS;
    return lowres;
}

void G_UpdateAngleFunctions(void)
{
    CarryAngleTic = lowres_turn ? CarryAngleTic_LowRes : CarryAngleTic_Full;
    CarryAngle = CarryAngleTic;
    G_UpdateLocalView = G_ClearLocalView;

    if (raw_input && (!netgame || solonet))
    {
        if (lowres_turn && fake_longtics)
        {
            CarryAngle = CarryAngle_FakeLongTics;
            G_UpdateLocalView = UpdateLocalView_FakeLongTics;
        }
        else if (uncapped)
        {
            CarryAngle = lowres_turn ? CarryAngle_LowRes : CarryAngle_Full;
        }
    }
}

static int CarryPitch(double pitch)
{
    return (localview.pitch =
                CarryError(pitch, &prevcarry.pitch, &carry.pitch));
}

static int CarrySide(double side)
{
    const double desired = side + prevcarry.side;
    const int actual = RoundSide(desired);
    carry.side = desired - actual;
    return actual;
}

static int CarryVert(double vert)
{
    return CarryError(vert, &prevcarry.vert, &carry.vert);
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

short G_CalcControllerAngle(void)
{
    localview.rawangle -= (deltatics * axes[AXIS_TURN] * angleturn[1]
                           * direction[joy_invert_turn]);
    return CarryAngle(localview.rawangle);
}

int G_CalcControllerPitch(void)
{
    localview.rawpitch -= (deltatics * axes[AXIS_LOOK] * angleturn[1]
                           * direction[joy_invert_look] * FRACUNIT);
    return CarryPitch(localview.rawpitch);
}

int G_CalcControllerSideTurn(int speed)
{
    const int side = RoundSide(axes[AXIS_TURN] * forwardmove[speed]
                               * direction[joy_invert_turn]);
    return BETWEEN(-forwardmove[speed], forwardmove[speed], side);
}

int G_CalcControllerSideStrafe(int speed)
{
    const int side = RoundSide(axes[AXIS_STRAFE] * forwardmove[speed]
                               * direction[joy_invert_strafe]);
    return BETWEEN(-sidemove[speed], sidemove[speed], side);
}

int G_CalcControllerForward(int speed)
{
    const int forward = lroundf(axes[AXIS_FORWARD] * forwardmove[speed]
                                * direction[joy_invert_forward]);
    return BETWEEN(-forwardmove[speed], forwardmove[speed], forward);
}

//
// Mouse
//

static double (*AccelerateMouse)(int val);

static double AccelerateMouse_Threshold(int val)
{
    if (val < 0)
    {
        return -AccelerateMouse_Threshold(-val);
    }

    if (val > mouse_acceleration_threshold)
    {
        return ((double)(val - mouse_acceleration_threshold)
                * (mouse_acceleration + 10) / 10
                + mouse_acceleration_threshold);
    }
    else
    {
        return val;
    }
}

static double AccelerateMouse_NoThreshold(int val)
{
    return ((double)val * (mouse_acceleration + 10) / 10);
}

static double AccelerateMouse_Skip(int val)
{
    return val;
}

void G_UpdateAccelerateMouse(void)
{
    if (mouse_acceleration)
    {
        AccelerateMouse = raw_input ? AccelerateMouse_NoThreshold
                                    : AccelerateMouse_Threshold;
    }
    else
    {
        AccelerateMouse = AccelerateMouse_Skip;
    }
}

short G_CalcMouseAngle(void)
{
    if (mouse_sensitivity)
    {
        localview.rawangle -= (AccelerateMouse(mousex)
                               * (mouse_sensitivity + 5) * 8 / 10);
        return CarryAngle(localview.rawangle);
    }
    else
    {
        return 0;
    }
}

int G_CalcMousePitch(void)
{
    if (mouse_sensitivity_y_look)
    {
        localview.rawpitch += (AccelerateMouse(mousey)
                               * (mouse_sensitivity_y_look + 5) * 8 / 10
                               * direction[mouse_y_invert] * FRACUNIT);
        return CarryPitch(localview.rawpitch);
    }
    else
    {
        return 0;
    }
}

int G_CalcMouseSide(void)
{
    if (mouse_sensitivity_strafe)
    {
        const double side = (AccelerateMouse(mousex)
                             * (mouse_sensitivity_strafe + 5) * 2 / 10);
        return CarrySide(side);
    }
    else
    {
        return 0;
    }
}

int G_CalcMouseVert(void)
{
    if (mouse_sensitivity_y)
    {
        const double vert = (AccelerateMouse(mousey)
                             * (mouse_sensitivity_y + 5) / 10);
        return CarryVert(vert);
    }
    else
    {
        return 0;
    }
}

//
// Composite Turn
//

static angle_t saved_angle;

void G_SavePlayerAngle(const player_t *player)
{
    saved_angle = player->mo->angle;
}

void G_AddToTicAngle(player_t *player)
{
    player->ticangle += player->mo->angle - saved_angle;
}

void G_UpdateTicAngleTurn(ticcmd_t *cmd, int angle)
{
    const short old_angleturn = cmd->angleturn;
    cmd->angleturn = CarryAngleTic(localview.rawangle + angle);
    cmd->ticangleturn = cmd->angleturn - old_angleturn;
}

//
// Quickstart Cache
// When recording a demo and the map is reloaded, cached input from a circular
// buffer can be applied prior to the screen wipe. Adapted from DSDA-Doom.
//

int quickstart_cache_tics;
boolean quickstart_queued;
float axis_turn_tic;
int mousex_tic;

void G_ClearQuickstartTic(void)
{
    axis_turn_tic = 0.0f;
    mousex_tic = 0;
}

void G_ApplyQuickstartCache(ticcmd_t *cmd, boolean strafe)
{
    static float axis_turn_cache[TICRATE];
    static int mousex_cache[TICRATE];
    static short angleturn_cache[TICRATE];
    static int index;

    if (quickstart_cache_tics < 1)
    {
        return;
    }

    if (quickstart_queued)
    {
        axes[AXIS_TURN] = 0.0f;
        mousex = 0;

        if (strafe)
        {
            for (int i = 0; i < quickstart_cache_tics; i++)
            {
                axes[AXIS_TURN] += axis_turn_cache[i];
                mousex += mousex_cache[i];
            }

            cmd->angleturn = 0;
            localview.rawangle = 0.0;
        }
        else
        {
            short result = 0;

            for (int i = 0; i < quickstart_cache_tics; i++)
            {
                result += angleturn_cache[i];
            }

            cmd->angleturn = CarryAngleTic(result);
            localview.rawangle = cmd->angleturn;
        }

        memset(axis_turn_cache, 0, sizeof(axis_turn_cache));
        memset(mousex_cache, 0, sizeof(mousex_cache));
        memset(angleturn_cache, 0, sizeof(angleturn_cache));
        index = 0;

        quickstart_queued = false;
    }
    else
    {
        axis_turn_cache[index] = axis_turn_tic;
        mousex_cache[index] = mousex_tic;
        angleturn_cache[index] = cmd->angleturn;
        index = (index + 1) % quickstart_cache_tics;
    }
}
