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
#include "m_config.h"
#include "r_main.h"
#include "r_state.h"

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

static int (*RoundSide)(double side);

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

typedef struct
{
    double angle;
    double pitch;
    double side;
    double vert;
} carry_t;

static carry_t prevcarry, carry;

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

// Round to nearest 256 for single byte turning. From Chocolate Doom.
#define BYTE_TURN(x) (((x) + 128) & 0xFF00)

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

short (*G_CarryAngleTic)(double angle);
short (*G_CarryAngle)(double angle);

void G_UpdateAngleFunctions(void)
{
    G_CarryAngleTic = lowres_turn ? CarryAngleTic_LowRes : CarryAngleTic_Full;
    G_CarryAngle = G_CarryAngleTic;

    if (!netgame || solonet)
    {
        if (lowres_turn && fake_longtics)
        {
            G_CarryAngle = CarryAngle_FakeLongTics;
        }
        else if (uncapped && raw_input)
        {
            G_CarryAngle = lowres_turn ? CarryAngle_LowRes : CarryAngle_Full;
        }
    }
}

short G_CarryPitch(double pitch)
{
    const short result = CarryError(pitch, &prevcarry.pitch, &carry.pitch);
    localview.pitch = result << FRACBITS;
    return result;
}

int G_CarrySide(double side)
{
    const double desired = side + prevcarry.side;
    const int actual = RoundSide(desired);
    carry.side = desired - actual;
    return actual;
}

int G_CarryVert(double vert)
{
    return CarryError(vert, &prevcarry.vert, &carry.vert);
}

//
// Gamepad
//

static const int direction[] = {1, -1};
static double deltatics;
static double joy_scale_angle;
static double joy_scale_pitch;

void G_UpdateDeltaTics(uint64_t delta_time)
{
    if (uncapped && raw_input)
    {
        deltatics = (double)delta_time * TICRATE * 1.0e-6;
        deltatics = BETWEEN(0.0, 1.0, deltatics);
    }
    else
    {
        deltatics = 1.0;
    }
}

static double CalcGamepadAngle_Standard(void)
{
    return (axes[AXIS_TURN] * joy_scale_angle * deltatics);
}

static double CalcGamepadAngle_Flick(void)
{
    return (axes[AXIS_TURN] * direction[joy_invert_turn]);
}

double (*G_CalcGamepadAngle)(void);

void G_UpdateGamepadVariables(void)
{
    if (I_StandardLayout())
    {
        joy_scale_angle = ANALOG_MULT * direction[joy_invert_turn];
        joy_scale_pitch = ANALOG_MULT * direction[joy_invert_look];

        if (correct_aspect_ratio)
        {
            joy_scale_pitch /= 1.2;
        }

        G_CalcGamepadAngle = CalcGamepadAngle_Standard;
    }
    else
    {
        joy_scale_angle = 0.0;
        joy_scale_pitch = 0.0;
        G_CalcGamepadAngle = CalcGamepadAngle_Flick;
    }
}

double G_CalcGamepadPitch(void)
{
    return (axes[AXIS_LOOK] * joy_scale_pitch * deltatics);
}

int G_CalcGamepadSideTurn(int speed)
{
    const int side = RoundSide(forwardmove[speed] * axes[AXIS_TURN]
                               * direction[joy_invert_turn]);
    return BETWEEN(-forwardmove[speed], forwardmove[speed], side);
}

int G_CalcGamepadSideStrafe(int speed)
{
    const int side = RoundSide(forwardmove[speed] * axes[AXIS_STRAFE]
                               * direction[joy_invert_strafe]);
    return BETWEEN(-sidemove[speed], sidemove[speed], side);
}

int G_CalcGamepadForward(int speed)
{
    const int forward = lroundf(forwardmove[speed] * axes[AXIS_FORWARD]
                                * direction[joy_invert_forward]);
    return BETWEEN(-forwardmove[speed], forwardmove[speed], forward);
}

//
// Mouse
//

static int mouse_sensitivity;
static int mouse_sensitivity_y;
static int mouse_sensitivity_strafe; // [FG] strafe
static int mouse_sensitivity_y_look; // [FG] look
static boolean mouse_y_invert;       // [FG] invert vertical axis
static int mouse_acceleration;
static int mouse_acceleration_threshold;

static double mouse_sens_angle;
static double mouse_sens_pitch;
static double mouse_sens_side;
static double mouse_sens_vert;
static double mouse_scale;

static double AccelerateMouse_Skip(int val)
{
    return val;
}

static double AccelerateMouse_RawInput(int val)
{
    return (val * mouse_scale);
}

static double AccelerateMouse_Chocolate(int val)
{
    if (val < 0)
    {
        return -AccelerateMouse_Chocolate(-val);
    }

    if (val > mouse_acceleration_threshold)
    {
        return ((val - mouse_acceleration_threshold) * mouse_scale
                + mouse_acceleration_threshold);
    }
    else
    {
        return val;
    }
}

static double (*AccelerateMouse)(int val);

void G_UpdateMouseVariables(void)
{
    mouse_sens_angle = 0.0;
    mouse_sens_pitch = 0.0;
    mouse_sens_side = 0.0;
    mouse_sens_vert = 0.0;
    mouse_scale = 1.0;
    AccelerateMouse = AccelerateMouse_Skip;

    if (mouse_sensitivity)
    {
        mouse_sens_angle = (double)(mouse_sensitivity + 5) * 8 / 10;
    }

    if (mouse_sensitivity_y_look)
    {
        mouse_sens_pitch = ((double)(mouse_sensitivity_y_look + 5) * 8 / 10
                            * direction[mouse_y_invert]);

        if (correct_aspect_ratio)
        {
            mouse_sens_pitch /= 1.2;
        }
    }

    if (mouse_sensitivity_strafe)
    {
        mouse_sens_side = (double)(mouse_sensitivity_strafe + 5) * 2 / 10;
    }

    if (mouse_sensitivity_y)
    {
        mouse_sens_vert = (double)(mouse_sensitivity_y + 5) / 10;
    }

    if (mouse_acceleration)
    {
        mouse_scale = (double)(mouse_acceleration + 10) / 10;
        AccelerateMouse = raw_input ? AccelerateMouse_RawInput
                                    : AccelerateMouse_Chocolate;
    }
}

double G_CalcMouseAngle(int mousex)
{
    return (AccelerateMouse(mousex) * mouse_sens_angle);
}

double G_CalcMousePitch(int mousey)
{
    return (AccelerateMouse(mousey) * mouse_sens_pitch);
}

double G_CalcMouseSide(int mousex)
{
    return (AccelerateMouse(mousex) * mouse_sens_side);
}

double G_CalcMouseVert(int mousey)
{
    return (AccelerateMouse(mousey) * mouse_sens_vert);
}

void G_BindMouseVariables(void)
{
    BIND_NUM_GENERAL(mouse_sensitivity, 15, 0, UL,
        "Horizontal mouse sensitivity for turning");
    BIND_NUM_GENERAL(mouse_sensitivity_y, 15, 0, UL,
        "Vertical mouse sensitivity for moving");
    BIND_NUM_GENERAL(mouse_sensitivity_strafe, 15, 0, UL,
        "Horizontal mouse sensitivity for strafing");
    BIND_NUM_GENERAL(mouse_sensitivity_y_look, 15, 0, UL,
        "Vertical mouse sensitivity for looking");
    BIND_NUM_GENERAL(mouse_acceleration, 0, 0, 40,
        "Mouse acceleration (0 = 1.0; 40 = 5.0)");
    BIND_NUM(mouse_acceleration_threshold, 10, 0, 32,
        "Mouse acceleration threshold");
    BIND_BOOL_GENERAL(mouse_y_invert, false,
        "Invert vertical mouse axis");
}
