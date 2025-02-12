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
//

#ifndef __I_GAMEPAD__
#define __I_GAMEPAD__

#include "d_event.h"
#include "doomkeys.h"

#define DEFAULT_SPEED 240 // degrees/second.
#define DEG2TICCMD (32768.0 / 180.0)
#define ANALOG_MULT (DEFAULT_SPEED * DEG2TICCMD / TICRATE)
#define PI_F 3.1415927f
#define RAD2TIC(x) ((x) * 32768.0f / PI_F) // Radians to ticcmd angle.
#define LENGTH_F(x, y) sqrtf((x) * (x) + (y) * (y))

typedef enum joy_platform_e
{
    PLATFORM_AUTO,
    PLATFORM_XBOX360,
    PLATFORM_XBOXONE,
    PLATFORM_PS3,
    PLATFORM_PS4,
    PLATFORM_PS5,
    PLATFORM_SWITCH,

    NUM_PLATFORMS,

    // Internal only:
    PLATFORM_SWITCH_PRO,
    PLATFORM_SWITCH_JOYCON_LEFT,
    PLATFORM_SWITCH_JOYCON_RIGHT,
    PLATFORM_SWITCH_JOYCON_PAIR,
} joy_platform_t;

typedef struct axis_s
{
    int data;                   // ev_joystick event data.
    float sens;                 // Sensitivity.
    float extra_sens;           // Extra sensitivity.
} axis_t;

typedef struct axes_s
{
    uint64_t time;              // Update time (us).
    uint64_t last_time;         // Last update time (us).
    axis_t x;
    axis_t y;
    boolean max_mag;            // Max magnitude?
    boolean extra_active;       // Using extra sensitivity?
    float extra_scale;          // Scaling factor for extra sensitivity.

    uint64_t ramp_time;         // Ramp time for extra sensitivity (us).
    boolean circle_to_square;   // Circle to square correction?
    float exponent;             // Exponent for response curve.
    float inner_deadzone;       // Normalized inner deadzone.
    float outer_deadzone;       // Normalized outer deadzone.
} axes_t;

extern int joy_device, last_joy_device;     // Gamepad device.
extern joy_platform_t joy_platform;         // Gamepad platform (button names).
extern boolean joy_invert_forward;          // Invert forward axis.
extern boolean joy_invert_strafe;           // Invert strafe axis.
extern boolean joy_invert_turn;             // Invert turn axis.
extern boolean joy_invert_look;             // Invert look axis.

extern float axes[NUM_AXES];        // Calculated gamepad values.
extern int trigger_threshold;       // Trigger threshold (axis resolution).

void I_GetRawAxesScaleMenu(boolean move, float *scale, float *limit);
boolean I_GamepadEnabled(void);
boolean I_UseStickLayout(void);
boolean I_StandardLayout(void);
boolean I_RampTimeEnabled(void);
void I_CalcRadial(axes_t *ax, float *xaxis, float *yaxis);
void I_CalcGamepadAxes(boolean strafe);
void I_UpdateAxesData(const struct event_s *ev);
void I_ResetGamepadAxes(void);
void I_ResetGamepadState(void);
void I_ResetGamepad(void);

void I_BindGamepadVariables(void);

#endif
