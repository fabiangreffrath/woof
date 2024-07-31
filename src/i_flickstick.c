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

#include "g_game.h"
#include "i_flickstick.h"
#include "i_gamepad.h"
#include "i_input.h"
#include "m_config.h"

#define MAX_F 0.999999f
#define SMOOTH_WINDOW 64000 // 64 ms

static int joy_flick_mode;
static int joy_flick_time;
static int joy_flick_rotation_smooth;
static int joy_flick_rotation_speed;
static int joy_flick_deadzone;
static int joy_flick_forward_deadzone;
static int joy_flick_snap;

flick_t flick;

static float EaseOut(float x)
{
    x = 1.0f - BETWEEN(0.0f, 1.0f, x);
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
    max_samples = BETWEEN(1, NUM_SAMPLES, max_samples);

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
        return BETWEEN(0.0f, 1.0f, raw_factor);
    }
}

static float SmoothTurn(axes_t *ax, float delta_angle)
{
    uint64_t delta_time = ax->time - ax->last_time;
    delta_time = BETWEEN(1, SMOOTH_WINDOW, delta_time);

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
    flick.time = joy_flick_time * 1000.0f;
    flick.upper_smooth = joy_flick_rotation_smooth / 10.0f;
    flick.lower_smooth = flick.upper_smooth * 0.5f;
    flick.rotation_speed = joy_flick_rotation_speed / 10.0f;
    flick.deadzone = joy_flick_deadzone / 100.0f;
    flick.deadzone = MIN(flick.deadzone, MAX_F);
    flick.forward_deadzone = joy_flick_forward_deadzone * PI_F / 180.0f;
    flick.snap = joy_flick_snap ? PI_F / powf(2.0f, joy_flick_snap) : 0.0f;
}

void I_BindFlickStickVariables(void)
{
    BIND_NUM(joy_flick_mode, MODE_DEFAULT, MODE_DEFAULT, NUM_FLICK_MODES - 1,
        "Flick mode (0 = Default; 1 = Flick Only; 2 = Rotate Only)");
    BIND_NUM(joy_flick_time, 100, 100, 500,
        "Flick time [milliseconds]");
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
