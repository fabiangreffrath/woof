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

#include <math.h>
#include <string.h>

#include "i_flickstick.h"
#include "doomstat.h"
#include "i_gamepad.h"
#include "m_config.h"

#define MAX_F 0.999999f
#define SMOOTH_WINDOW 64000 // 64 ms
#define NUM_SAMPLES 256 // Smoothing samples.

typedef enum flick_mode_e
{
    MODE_DEFAULT,
    MODE_FLICK_ONLY,
    MODE_ROTATE_ONLY,

    NUM_FLICK_MODES
} flick_mode_t;

static int joy_flick_mode;
static int joy_flick_time;
static int joy_flick_rotation_smooth;
static int joy_flick_rotation_speed;
static int joy_flick_deadzone;
static int joy_flick_forward_deadzone;
static int joy_flick_snap;

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

static flick_t flick;

boolean I_FlickStickActive(void)
{
    return flick.active;
}

boolean I_PendingFlickStickReset(void)
{
    return flick.reset;
}

void I_SetPendingFlickStickReset(boolean condition)
{
    flick.reset = condition;
}

static float EaseOut(float x)
{
    x = 1.0f - CLAMP(x, 0.0f, 1.0f);
    return (1.0f - x * x);
}

static float WrapAngle(float angle)
{
    angle = fmodf(angle + PI_F, 2.0f * PI_F);
    angle += (angle < 0.0f) ? PI_F : -PI_F;
    return angle;
}

static float GetSmoothAngle(uint64_t delta_time, float delta_angle,
                            float smooth_factor)
{
    flick.index = (flick.index + (NUM_SAMPLES - 1)) % NUM_SAMPLES;
    flick.samples[flick.index] = delta_angle * smooth_factor;

    int max_samples = (int)ceilf((float)SMOOTH_WINDOW / delta_time);
    max_samples = CLAMP(max_samples, 1, NUM_SAMPLES);

    float smooth_angle = flick.samples[flick.index] / max_samples;

    for (int i = 1; i < max_samples; i++)
    {
        const int index = (flick.index + i) % NUM_SAMPLES;
        smooth_angle += flick.samples[index] / max_samples;
    }

    return smooth_angle;
}

static float GetRawFactor(uint64_t delta_time, float delta_angle)
{
    const float denom = flick.upper_smooth - flick.lower_smooth;

    if (denom <= 0.0f)
    {
        return 1.0f;
    }
    else
    {
        const float rps =
            fabsf(delta_angle) * 1.0e6f / (2.0f * PI_F * delta_time);
        const float raw_factor = (rps - flick.lower_smooth) / denom;
        return CLAMP(raw_factor, 0.0f, 1.0f);
    }
}

static float SmoothTurn(axes_t *ax, float delta_angle)
{
    uint64_t delta_time = ax->time - ax->last_time;
    delta_time = CLAMP(delta_time, 1, SMOOTH_WINDOW);

    const float raw_factor = GetRawFactor(delta_time, delta_angle);
    const float smooth_factor = 1.0f - raw_factor;

    const float raw_angle = delta_angle * raw_factor;
    const float smooth_angle =
        GetSmoothAngle(delta_time, delta_angle, smooth_factor);

    return (raw_angle + smooth_angle);
}

//
// CalcFlickStick
//
// Based on flick stick implementation by Julian "Jibb" Smart.
// http://gyrowiki.jibbsmart.com/blog:good-gyro-controls-part-2:the-flick-stick
//

static void ResetSmoothing(void);

void I_CalcFlickStick(axes_t *ax, float *xaxis, float *yaxis)
{
    float output_angle = 0.0f;
    float x_input, y_input;
    I_CalcRadial(ax, &x_input, &y_input);

    if (!ax->max_mag && flick.active)
    {
        const float input_mag = LENGTH_F(x_input, y_input);
        ax->max_mag = (input_mag >= flick.deadzone);
    }

    if (ax->max_mag)
    {
        float angle = atan2f(x_input, -y_input);

        if (flick.active) // Turn after flick.
        {
            if (flick.mode != MODE_FLICK_ONLY)
            {
                const float last_angle = atan2f(flick.lastx, -flick.lasty);
                const float delta_angle = WrapAngle(angle - last_angle)
                                          * flick.rotation_speed;
                output_angle = SmoothTurn(ax, delta_angle);
            }
        }
        else // Start flick.
        {
            flick.active = true;

            if (flick.mode != MODE_ROTATE_ONLY)
            {
                flick.start_time = ax->time;

                if (STRICTMODE(flick.snap > 0.0f))
                {
                    angle = roundf(angle / flick.snap) * flick.snap;
                }

                if (fabsf(angle) <= flick.forward_deadzone)
                {
                    angle = 0.0f;
                }

                flick.target_angle = angle;
                flick.percent = 0.0f;
                ResetSmoothing();
            }
        }
    }
    else // Stop turning.
    {
        flick.active = false;
    }

    if (flick.mode != MODE_ROTATE_ONLY)
    {
        // Continue flick.
        const float last_percent = flick.percent;
        const float raw_percent = (ax->time - flick.start_time) / flick.time;
        flick.percent = EaseOut(raw_percent);
        output_angle += (flick.percent - last_percent) * flick.target_angle;
    }

    *xaxis = RAD2TIC(output_angle);

    flick.lastx = x_input;
    flick.lasty = y_input;
}

static void ResetSmoothing(void)
{
    flick.index = 0;
    memset(flick.samples, 0, sizeof(flick.samples));
}

void I_ResetFlickStick(void)
{
    flick.reset = false;
    flick.active = false;
    flick.lastx = 0.0f;
    flick.lasty = 0.0f;
    flick.start_time = 0;
    flick.target_angle = 0.0f;
    flick.percent = 0.0f;
    ResetSmoothing();
}

void I_RefreshFlickStickSettings(void)
{
    flick.mode = joy_flick_mode;
    flick.time = joy_flick_time * 10000.0f;
    flick.upper_smooth = joy_flick_rotation_smooth / 10.0f;
    flick.lower_smooth = flick.upper_smooth * 0.5f;
    flick.rotation_speed = joy_flick_rotation_speed / 10.0f;
    flick.deadzone = joy_flick_deadzone / 100.0f;
    flick.deadzone = MIN(flick.deadzone, MAX_F);
    flick.forward_deadzone = joy_flick_forward_deadzone * PI_F / 180.0f;
    flick.snap = joy_flick_snap ? PI_F / powf(2.0f, joy_flick_snap) : 0.0f;
}

#define BIND_NUM_PADADV(name, v, a, b, help) \
    M_BindNum(#name, &name, NULL, (v), (a), (b), ss_padadv, wad_no, help)

void I_BindFlickStickVariables(void)
{
    BIND_NUM(joy_flick_mode, MODE_DEFAULT, MODE_DEFAULT, NUM_FLICK_MODES - 1,
        "Flick mode (0 = Default; 1 = Flick Only; 2 = Rotate Only)");
    BIND_NUM_PADADV(joy_flick_time, 10, 10, 50,
        "Flick time (10 = 100 ms; 50 = 500 ms)");
    BIND_NUM(joy_flick_rotation_smooth, 8, 0, 50,
        "Flick rotation smoothing threshold "
        "(0 = Off; 50 = 5.0 rotations/second)");
    BIND_NUM(joy_flick_rotation_speed, 10, 0, 50,
        "Flick rotation speed (0 = 0.0x; 50 = 5.0x)");
    BIND_NUM(joy_flick_deadzone, 90, 0, 100,
        "Flick deadzone relative to camera deadzone [percent]");
    BIND_NUM(joy_flick_forward_deadzone, 7, 0, 45,
        "Forward angle range where flicks are disabled [degrees]");
    BIND_NUM(joy_flick_snap, 0, 0, 2,
        "Snap to cardinal directions (0 = Off; 1 = 4-way; 2 = 8-way)");
}
