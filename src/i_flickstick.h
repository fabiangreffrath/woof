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
//      Gamepad flick stick.
//

#ifndef __I_FLICKSTICK__
#define __I_FLICKSTICK__

#include "doomtype.h"

#define NUM_SAMPLES 256

struct axes_s;

typedef enum flick_mode_e
{
    MODE_DEFAULT,
    MODE_FLICK_ONLY,
    MODE_ROTATE_ONLY,

    NUM_FLICK_MODES
} flick_mode_t;

typedef struct flick_s
{
    boolean reset;              // Reset pending?
    boolean active;             // Flick or rotation active?
    float lastx;                // Last read x input.
    float lasty;                // Last read y input.
    uint64_t start_time;        // Time when flick started (us).
    float target_angle;         // Target angle for current flick (radians).
    float percent;              // Percent complete for current flick.
    int index;                  // Smoothing sample index.
    float samples[NUM_SAMPLES]; // Smoothing samples.

    flick_mode_t mode;          // Flick mode.
    float time;                 // Time required to execute a flick (us).
    float lower_smooth;         // Lower smoothing threshold (rotations/s).
    float upper_smooth;         // Upper smoothing threshold (rotations/s).
    float rotation_speed;       // Rotation speed.
    float deadzone;             // Normalized outer deadzone.
    float forward_deadzone;     // Forward deadzone angle (radians).
    float snap;                 // Snap angle (radians).
} flick_t;

extern flick_t flick;

void I_CalcFlickStick(struct axes_s *ax, float *xaxis, float *yaxis);
void I_ResetFlickStick(void);
void I_RefreshFlickStickSettings(void);

void I_BindFlickStickVariables(void);

#endif
