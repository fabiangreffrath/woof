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

#include "SDL.h"

#include <math.h>

#include "d_event.h"
#include "doomkeys.h"
#include "doomtype.h"
#include "g_game.h"
#include "i_timer.h"

#define MIN_DEADZONE 0.00003f // 1/32768
#define SGNF(x) ((float)((0.0f < (x)) - ((x) < 0.0f)))
#define REMAP(x, min, max) (((x) - (min)) / ((max) - (min)))

boolean joy_enable;
int joy_layout;
int joy_sensitivity_forward;
int joy_sensitivity_strafe;
int joy_sensitivity_turn;
int joy_sensitivity_look;
int joy_extra_sensitivity_turn;
int joy_extra_sensitivity_look;
int joy_extra_ramp_time;
boolean joy_scale_diagonal_movement;
int joy_response_curve_movement;
int joy_response_curve_camera;
int joy_deadzone_type_movement;
int joy_deadzone_type_camera;
int joy_deadzone_movement;
int joy_deadzone_camera;
int joy_threshold_movement;
int joy_threshold_camera;
int joy_threshold_trigger;
boolean joy_invert_forward;
boolean joy_invert_strafe;
boolean joy_invert_turn;
boolean joy_invert_look;

static int *axes_data[NUM_AXES]; // Pointers to ev_joystick event data.
float axes[NUM_AXES];
int trigger_threshold;

typedef struct axis_s
{
    int data;           // ev_joystick event data.
    float sens;         // Sensitivity.
    float extra;        // Extra sensitivity.
} axis_t;

typedef struct axes_s
{
    axis_t x;
    axis_t y;
    float exponent;     // Exponent for response curve.
    float deadzone;     // Normalized deadzone.
    float threshold;    // Normalized outer threshold.
    boolean useextra;   // Using extra sensitivity?
    float extrascale;   // Scaling factor for extra sensitivity.
} axes_t;

static axes_t movement; // Strafe/Forward
static axes_t camera;   // Turn/Look

static void CalcExtraScale(axes_t *ax, float magnitude)
{
    if (ax != &camera)
    {
        return;
    }

    if ((joy_extra_sensitivity_turn || joy_extra_sensitivity_look) &&
        magnitude > ax->threshold)
    {
        if (joy_extra_ramp_time)
        {
            static int start_time;
            static int elapsed_time;

            if (ax->useextra)
            {
                if (elapsed_time < joy_extra_ramp_time)
                {
                    // Continue ramp.
                    elapsed_time = I_GetTimeMS() - start_time;
                    ax->extrascale = (float)elapsed_time / joy_extra_ramp_time;
                    ax->extrascale = BETWEEN(0.0f, 1.0f, ax->extrascale);
                }
            }
            else
            {
                // Start ramp.
                start_time = I_GetTimeMS();
                elapsed_time = 0;
                ax->extrascale = 0.0f;
                ax->useextra = true;
            }
        }
        else
        {
            // Instant ramp.
            ax->extrascale = 1.0f;
            ax->useextra = true;
        }
    }
    else
    {
        // Reset ramp.
        ax->extrascale = 0.0f;
        ax->useextra = false;
    }
}

//
// CircleToSquare
// Radial mapping of a circle to a square using the Fernandez-Guasti method.
// https://squircular.blogspot.com/2015/09/fg-squircle-mapping.html
//

#define MIN_C2S 0.000001f

static void CircleToSquare(float *x, float *y)
{
    const float u = *x;
    const float v = *y;

    if (fabsf(u) > MIN_C2S && fabsf(v) > MIN_C2S)
    {
        const float uv = u * v;
        const float sgnuv = SGNF(uv);

        if (sgnuv)
        {
            const float r2 = u * u + v * v;
            const float rad = r2 * (r2 - 4.0f * uv * uv);

            if (rad > 0.0f)
            {
                const float r2rad = 0.5f * (r2 - sqrtf(rad));

                if (r2rad > 0.0f)
                {
                    const float sqrto = sqrtf(r2rad);

                    *x = sgnuv / v * sqrto;
                    *y = sgnuv / u * sqrto;

                    *x = BETWEEN(-1.0f, 1.0f, *x);
                    *y = BETWEEN(-1.0f, 1.0f, *y);
                }
            }
        }
    }
}

static void ApplySensitivity(const axes_t *ax, float *xaxis, float *yaxis)
{
    if (*xaxis || *yaxis)
    {
        if (ax == &movement)
        {
            if (joy_scale_diagonal_movement)
            {
                CircleToSquare(xaxis, yaxis);
            }

            *xaxis *= ax->x.sens;
            *yaxis *= ax->y.sens;
        }
        else // camera
        {
            *xaxis *= (ax->x.sens + ax->extrascale * ax->x.extra);
            *yaxis *= (ax->y.sens + ax->extrascale * ax->y.extra);
        }
    }
}

static float CalcAxialValue(axes_t *ax, float input)
{
    if (fabsf(input) > ax->deadzone)
    {
        return (SGNF(input) * REMAP(fabsf(input), ax->deadzone, ax->threshold));
    }
    else
    {
        return 0.0f;
    }
}

static float GetInputValue(int value)
{
    if (value > 0)
    {
        return (value / (float)SDL_JOYSTICK_AXIS_MAX);
    }
    else if (value < 0)
    {
        return (value / (float)(-SDL_JOYSTICK_AXIS_MIN));
    }
    else
    {
        return 0.0f;
    }
}

static void CalcAxial(axes_t *ax, float *xaxis, float *yaxis)
{
    const float x_input = GetInputValue(ax->x.data);
    const float y_input = GetInputValue(ax->y.data);
    const float input_mag = sqrtf(x_input * x_input + y_input * y_input);

    const float x_axial = CalcAxialValue(ax, x_input);
    const float y_axial = CalcAxialValue(ax, y_input);
    const float axial_mag = sqrtf(x_axial * x_axial + y_axial * y_axial);

    if (axial_mag > 0.000001f)
    {
        float scaled_mag;

        scaled_mag = BETWEEN(0.0f, 1.0f, axial_mag);
        scaled_mag = powf(scaled_mag, ax->exponent);

        *xaxis = scaled_mag * x_axial / axial_mag;
        *yaxis = scaled_mag * y_axial / axial_mag;
    }
    else
    {
        *xaxis = 0.0f;
        *yaxis = 0.0f;
    }

    CalcExtraScale(ax, input_mag);
    ApplySensitivity(ax, xaxis, yaxis);
}

static void CalcRadial(axes_t *ax, float *xaxis, float *yaxis)
{
    const float x_input = GetInputValue(ax->x.data);
    const float y_input = GetInputValue(ax->y.data);
    const float input_mag = sqrtf(x_input * x_input + y_input * y_input);

    if (input_mag > ax->deadzone)
    {
        float scaled_mag = REMAP(input_mag, ax->deadzone, ax->threshold);

        scaled_mag = BETWEEN(0.0f, 1.0f, scaled_mag);
        scaled_mag = powf(scaled_mag, ax->exponent);

        *xaxis = scaled_mag * x_input / input_mag;
        *yaxis = scaled_mag * y_input / input_mag;
    }
    else
    {
        *xaxis = 0.0f;
        *yaxis = 0.0f;
    }

    CalcExtraScale(ax, input_mag);
    ApplySensitivity(ax, xaxis, yaxis);
}

static void (*CalcMovement)(axes_t *ax, float *xaxis, float *yaxis);
static void (*CalcCamera)(axes_t *ax, float *xaxis, float *yaxis);

boolean I_CalcControllerAxes(void)
{
    boolean camera_update = false;

    if (movement.x.data || movement.y.data)
    {
        CalcMovement(&movement, &axes[AXIS_STRAFE], &axes[AXIS_FORWARD]);

        movement.x.data = 0;
        movement.y.data = 0;
    }

    if (camera.x.data || camera.y.data)
    {
        if (!padlook)
        {
            camera.y.data = 0;
        }

        CalcCamera(&camera, &axes[AXIS_TURN], &axes[AXIS_LOOK]);

        camera.x.data = 0;
        camera.y.data = 0;

        camera_update = true;
    }

    return camera_update;
}

void I_UpdateAxesData(const event_t *ev)
{
    *axes_data[AXIS_LEFTX] = ev->data1;
    *axes_data[AXIS_LEFTY] = ev->data2;
    *axes_data[AXIS_RIGHTX] = ev->data3;
    *axes_data[AXIS_RIGHTY] = ev->data4;
}

void I_ResetControllerAxes(void)
{
    memset(axes, 0, sizeof(axes));
}

void I_ResetControllerLevel(void)
{
    I_ResetControllerAxes();
    movement.x.data = 0;
    movement.y.data = 0;
    camera.x.data = 0;
    camera.y.data = 0;
    camera.extrascale = 0.0f;
    camera.useextra = false;
}

static void UpdateStickLayout(void)
{
    int i;
    int *layouts[NUM_LAYOUTS][NUM_AXES] = {
        // Default
        {&movement.x.data, &movement.y.data, &camera.x.data, &camera.y.data},
        // Swap
        {&camera.x.data, &camera.y.data, &movement.x.data, &movement.y.data},
        // Legacy
        {&camera.x.data, &movement.y.data, &movement.x.data, &camera.y.data},
        // Legacy Swap
        {&movement.x.data, &camera.y.data, &camera.x.data, &movement.y.data},
    };

    for (i = 0; i < NUM_AXES; i++)
    {
        axes_data[i] = layouts[joy_layout][i];
    }
}

static void RefreshSettings(void)
{
    void *axes_func[] = {CalcAxial, CalcRadial};

    CalcMovement = axes_func[joy_deadzone_type_movement];
    CalcCamera = axes_func[joy_deadzone_type_camera];

    movement.x.sens = joy_sensitivity_strafe / 50.0f;
    movement.y.sens = joy_sensitivity_forward / 50.0f;

    camera.x.sens = joy_sensitivity_turn / 50.0f;
    camera.y.sens = joy_sensitivity_look / 50.0f;

    camera.x.extra = joy_extra_sensitivity_turn / 50.0f;
    camera.y.extra = joy_extra_sensitivity_look / 50.0f;

    movement.exponent = joy_response_curve_movement / 10.0f;
    camera.exponent = joy_response_curve_camera / 10.0f;

    movement.deadzone = joy_deadzone_movement / 100.0f;
    camera.deadzone = joy_deadzone_camera / 100.0f;
    movement.deadzone = MAX(MIN_DEADZONE, movement.deadzone);
    camera.deadzone = MAX(MIN_DEADZONE, camera.deadzone);

    movement.threshold = 1.0f - joy_threshold_movement / 100.0f;
    camera.threshold = 1.0f - joy_threshold_camera / 100.0f;

    trigger_threshold = SDL_JOYSTICK_AXIS_MAX * joy_threshold_trigger / 100;
}

void I_ResetController(void)
{
    I_ResetControllerLevel();
    UpdateStickLayout();
    RefreshSettings();
}
