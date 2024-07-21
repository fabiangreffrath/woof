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
#include "m_config.h"

#define MIN_DEADZONE 0.00003f // 1/32768
#define MIN_F 0.000001f
#define SGNF(x) ((float)((0.0f < (x)) - ((x) < 0.0f)))
#define REMAP(x, min, max) (((x) - (min)) / ((max) - (min)))

boolean joy_enable;
static int joy_layout;
static int joy_forward_sensitivity;
static int joy_strafe_sensitivity;
static int joy_turn_sensitivity;
static int joy_look_sensitivity;
static int joy_outer_turn_sensitivity;
static int joy_outer_look_sensitivity;
static int joy_outer_ramp_time;
static int joy_scale_diagonal_movement;
static int joy_movement_curve;
static int joy_camera_curve;
static int joy_movement_deadzone_type;
static int joy_camera_deadzone_type;
static int joy_movement_inner_deadzone;
static int joy_camera_inner_deadzone;
static int joy_movement_outer_deadzone;
static int joy_camera_outer_deadzone;
static int joy_trigger_deadzone;
boolean joy_invert_forward;
boolean joy_invert_strafe;
boolean joy_invert_turn;
boolean joy_invert_look;

static int *axes_data[NUM_AXES]; // Pointers to ev_joystick event data.
float axes[NUM_AXES];
int trigger_threshold;

typedef struct
{
    int data;                   // ev_joystick event data.
    float sens;                 // Sensitivity.
    float extra_sens;           // Extra sensitivity.
} axis_t;

typedef struct
{
    axis_t x;
    axis_t y;
    float exponent;             // Exponent for response curve.
    float inner_deadzone;       // Normalized inner deadzone.
    float outer_deadzone;       // Normalized outer deadzone.
    boolean extra_active;       // Using extra sensitivity?
    float extra_scale;          // Scaling factor for extra sensitivity.
} axes_t;

static axes_t movement;         // Strafe/Forward
static axes_t camera;           // Turn/Look

static void CalcExtraScale(axes_t *ax, float magnitude)
{
    if (ax != &camera)
    {
        return;
    }

    if ((joy_outer_turn_sensitivity || joy_outer_look_sensitivity) &&
        magnitude > ax->outer_deadzone)
    {
        if (joy_outer_ramp_time)
        {
            static int start_time;
            static int elapsed_time;

            if (ax->extra_active)
            {
                if (elapsed_time < joy_outer_ramp_time)
                {
                    // Continue ramp.
                    elapsed_time = I_GetTimeMS() - start_time;
                    ax->extra_scale = (float)elapsed_time / joy_outer_ramp_time;
                    ax->extra_scale = BETWEEN(0.0f, 1.0f, ax->extra_scale);
                }
            }
            else
            {
                // Start ramp.
                start_time = I_GetTimeMS();
                elapsed_time = 0;
                ax->extra_scale = 0.0f;
                ax->extra_active = true;
            }
        }
        else
        {
            // Instant ramp.
            ax->extra_scale = 1.0f;
            ax->extra_active = true;
        }
    }
    else
    {
        // Reset ramp.
        ax->extra_scale = 0.0f;
        ax->extra_active = false;
    }
}

//
// CircleToSquare
// Radial mapping of a circle to a square using the Fernandez-Guasti method.
// https://squircular.blogspot.com/2015/09/fg-squircle-mapping.html
//

static void CircleToSquare(float *x, float *y)
{
    const float u = *x;
    const float v = *y;

    if (fabsf(u) > MIN_F && fabsf(v) > MIN_F)
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
            *xaxis *= ax->x.sens + ax->extra_scale * ax->x.extra_sens;
            *yaxis *= ax->y.sens + ax->extra_scale * ax->y.extra_sens;
        }
    }
}

static float CalcAxialValue(axes_t *ax, float input)
{
    if (fabsf(input) > ax->inner_deadzone)
    {
        return (SGNF(input)
                * REMAP(fabsf(input), ax->inner_deadzone, ax->outer_deadzone));
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

    if (axial_mag > MIN_F)
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

    if (input_mag > ax->inner_deadzone)
    {
        float scaled_mag =
            REMAP(input_mag, ax->inner_deadzone, ax->outer_deadzone);

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

void I_CalcGamepadAxes(void)
{
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
    }
}

void I_UpdateAxesData(const event_t *ev)
{
    *axes_data[AXIS_LEFTX] = ev->data1;
    *axes_data[AXIS_LEFTY] = ev->data2;
    *axes_data[AXIS_RIGHTX] = ev->data3;
    *axes_data[AXIS_RIGHTY] = ev->data4;
}

void I_ResetGamepadAxes(void)
{
    memset(axes, 0, sizeof(axes));
}

void I_ResetGamepadState(void)
{
    I_ResetGamepadAxes();
    movement.x.data = 0;
    movement.y.data = 0;
    camera.x.data = 0;
    camera.y.data = 0;
    camera.extra_scale = 0.0f;
    camera.extra_active = false;
}

static void UpdateStickLayout(void)
{
    int i;
    int *layouts[NUM_LAYOUTS][NUM_AXES] = {
        // Default
        {&movement.x.data, &movement.y.data, &camera.x.data, &camera.y.data},
        // Southpaw
        {&camera.x.data, &camera.y.data, &movement.x.data, &movement.y.data},
        // Legacy
        {&camera.x.data, &movement.y.data, &movement.x.data, &camera.y.data},
        // Legacy Southpaw
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

    CalcMovement = axes_func[joy_movement_deadzone_type];
    CalcCamera = axes_func[joy_camera_deadzone_type];

    movement.x.sens = joy_strafe_sensitivity / 50.0f;
    movement.y.sens = joy_forward_sensitivity / 50.0f;

    camera.x.sens = joy_turn_sensitivity / 50.0f;
    camera.y.sens = joy_look_sensitivity / 50.0f;

    camera.x.extra_sens = joy_outer_turn_sensitivity / 50.0f;
    camera.y.extra_sens = joy_outer_look_sensitivity / 50.0f;

    movement.exponent = joy_movement_curve / 10.0f;
    camera.exponent = joy_camera_curve / 10.0f;

    movement.inner_deadzone = joy_movement_inner_deadzone / 100.0f;
    camera.inner_deadzone = joy_camera_inner_deadzone / 100.0f;
    movement.inner_deadzone = MAX(MIN_DEADZONE, movement.inner_deadzone);
    camera.inner_deadzone = MAX(MIN_DEADZONE, camera.inner_deadzone);

    movement.outer_deadzone = 1.0f - joy_movement_outer_deadzone / 100.0f;
    camera.outer_deadzone = 1.0f - joy_camera_outer_deadzone / 100.0f;

    trigger_threshold = SDL_JOYSTICK_AXIS_MAX * joy_trigger_deadzone / 100;
}

void I_ResetGamepad(void)
{
    I_ResetGamepadState();
    UpdateStickLayout();
    RefreshSettings();
}

void I_BindGamepadVariables(void)
{
    BIND_BOOL(joy_enable, true, "Enable gamepad");
    BIND_NUM_GENERAL(joy_layout, LAYOUT_DEFAULT, 0, NUM_LAYOUTS - 1,
        "Analog stick layout (0 = Default; 1 = Southpaw; 2 = Legacy; "
        "3 = Legacy Southpaw)");
    BIND_NUM(joy_forward_sensitivity, 50, 0, 100,
        "Forward sensitivity");
    BIND_NUM(joy_strafe_sensitivity, 50, 0, 100,
        "Strafe sensitivity");
    BIND_NUM_GENERAL(joy_turn_sensitivity, 50, 0, 100,
        "Turn sensitivity");
    BIND_NUM_GENERAL(joy_look_sensitivity, 50, 0, 100,
        "Look sensitivity");
    BIND_NUM(joy_outer_turn_sensitivity, 0, 0, 100,
        "Extra turn sensitivity at outer deadzone (joy_camera_outer_deadzone)");
    BIND_NUM(joy_outer_look_sensitivity, 0, 0, 100,
        "Extra look sensitivity at outer deadzone (joy_camera_outer_deadzone)");
    BIND_NUM(joy_outer_ramp_time, 200, 0, 1000,
        "Ramp time for extra sensitivity [milliseconds]");
    BIND_NUM(joy_scale_diagonal_movement, 1, 0, 1,
        "Scale diagonal movement (0 = Linear; 1 = Circle to Square)");
    BIND_NUM(joy_movement_curve, 10, 10, 30,
        "Movement response curve (10 = Linear; 20 = Squared; 30 = Cubed)");
    BIND_NUM_GENERAL(joy_camera_curve, 20, 10, 30,
        "Camera response curve (10 = Linear; 20 = Squared; 30 = Cubed)");
    BIND_NUM(joy_movement_deadzone_type, 1, 0, 1,
        "Movement deadzone type (0 = Axial; 1 = Radial)");
    BIND_NUM(joy_camera_deadzone_type, 1, 0, 1,
        "Camera deadzone type (0 = Axial; 1 = Radial)");
    BIND_NUM_GENERAL(joy_movement_inner_deadzone, 15, 0, 50,
        "Movement inner deadzone [percent]");
    BIND_NUM_GENERAL(joy_camera_inner_deadzone, 15, 0, 50,
        "Camera inner deadzone [percent]");
    BIND_NUM(joy_movement_outer_deadzone, 5, 0, 30,
        "Movement outer deadzone [percent]");
    BIND_NUM(joy_camera_outer_deadzone, 5, 0, 30,
        "Camera outer deadzone [percent]");
    BIND_NUM(joy_trigger_deadzone, 15, 0, 50,
        "Trigger deadzone [percent]");
    BIND_BOOL(joy_invert_forward, false,
        "Invert forward axis");
    BIND_BOOL(joy_invert_strafe, false,
        "Invert strafe axis");
    BIND_BOOL(joy_invert_turn, false,
        "Invert turn axis");
    BIND_BOOL_GENERAL(joy_invert_look, false,
        "Invert look axis");
}
