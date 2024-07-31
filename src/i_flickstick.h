//----------------------------------------------------------------------------
//
//  Copyright (c) 2018-2021 Julian "Jibb" Smart
//  Copyright (c) 2021-2024 Nicolas Lessard
//  Copyright (c) 2024 ceski
//
//  Permission is hereby granted, free of charge, to any person obtaining
//  a copy of this software and associated documentation files (the
//  "Software"), to deal in the Software without restriction, including
//  without limitation the rights to use, copy, modify, merge, publish,
//  distribute, sublicense, and/or sell copies of the Software, and to
//  permit persons to whom the Software is furnished to do so, subject to
//  the following conditions:
//
//  The above copyright notice and this permission notice shall be
//  included in all copies or substantial portions of the Software.
//
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
//  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
//  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
//  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
//  CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
//  TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
//  SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//
//----------------------------------------------------------------------------

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
