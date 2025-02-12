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
#include <string.h>

#include "g_game.h"
#include "i_flickstick.h"
#include "i_gamepad.h"
#include "i_gyro.h"
#include "i_input.h"
#include "i_timer.h"
#include "m_config.h"

#define MIN_DEADZONE 0.00003f // 1/32768
#define MIN_F 0.000001f
#define SGNF(x) ((float)((0.0f < (x)) - ((x) < 0.0f)))
#define REMAP(x, min, max) (((x) - (min)) / ((max) - (min)))

enum
{
    LAYOUT_OFF,
    LAYOUT_DEFAULT,
    LAYOUT_SOUTHPAW,
    LAYOUT_LEGACY,
    LAYOUT_LEGACY_SOUTHPAW,
    LAYOUT_FLICK_STICK,
    LAYOUT_FLICK_STICK_SOUTHPAW,

    NUM_LAYOUTS
};

static boolean joy_enable;
int joy_device, last_joy_device;
joy_platform_t joy_platform;
static int joy_stick_layout;
static int joy_forward_sensitivity;
static int joy_strafe_sensitivity;
static int joy_turn_speed;
static int joy_look_speed;
static int joy_outer_turn_speed;
static int joy_outer_look_speed;
static int joy_outer_ramp_time;
static int joy_movement_type;
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

static axes_t movement;         // Strafe/Forward
static axes_t camera;           // Turn/Look

static int raw[NUM_AXES];       // Raw data for menu indicators.
static int *raw_ptr[NUM_AXES];  // Pointers to raw data.

static float GetInputValue(int value);

void I_GetRawAxesScaleMenu(boolean move, float *scale, float *limit)
{
    *raw_ptr[AXIS_LEFTX] = I_GetAxisState(SDL_CONTROLLER_AXIS_LEFTX);
    *raw_ptr[AXIS_LEFTY] = I_GetAxisState(SDL_CONTROLLER_AXIS_LEFTY);
    *raw_ptr[AXIS_RIGHTX] = I_GetAxisState(SDL_CONTROLLER_AXIS_RIGHTX);
    *raw_ptr[AXIS_RIGHTY] = I_GetAxisState(SDL_CONTROLLER_AXIS_RIGHTY);

    const float x = GetInputValue(raw[move ? AXIS_STRAFE : AXIS_TURN]);
    const float y = GetInputValue(raw[move ? AXIS_FORWARD : AXIS_LOOK]);
    const float length = LENGTH_F(x, y);

    *scale = BETWEEN(0.0f, 0.5f, length) / 0.5f;
    *limit = (move ? movement.inner_deadzone : camera.inner_deadzone) / 0.5f;
}

static void UpdateRawLayout(void)
{
    int *raw_layouts[NUM_LAYOUTS][NUM_AXES] = {
        // Off
        {&raw[AXIS_STRAFE], &raw[AXIS_FORWARD], &raw[AXIS_TURN], &raw[AXIS_LOOK]},
        // Default
        {&raw[AXIS_STRAFE], &raw[AXIS_FORWARD], &raw[AXIS_TURN], &raw[AXIS_LOOK]},
        // Southpaw
        {&raw[AXIS_TURN], &raw[AXIS_LOOK], &raw[AXIS_STRAFE], &raw[AXIS_FORWARD]},
        // Legacy
        {&raw[AXIS_TURN], &raw[AXIS_FORWARD], &raw[AXIS_STRAFE], &raw[AXIS_LOOK]},
        // Legacy Southpaw
        {&raw[AXIS_STRAFE], &raw[AXIS_LOOK], &raw[AXIS_TURN], &raw[AXIS_FORWARD]},
        // Flick Stick
        {&raw[AXIS_STRAFE], &raw[AXIS_FORWARD], &raw[AXIS_TURN], &raw[AXIS_LOOK]},
        // Flick Stick Southpaw
        {&raw[AXIS_TURN], &raw[AXIS_LOOK], &raw[AXIS_STRAFE], &raw[AXIS_FORWARD]},
    };

    for (int i = 0; i < NUM_AXES; i++)
    {
        raw_ptr[i] = raw_layouts[joy_stick_layout][i];
    }
}

boolean I_GamepadEnabled(void)
{
    return joy_enable;
}

boolean I_UseStickLayout(void)
{
    return (joy_stick_layout > LAYOUT_OFF);
}

boolean I_StandardLayout(void)
{
    return (joy_stick_layout < LAYOUT_FLICK_STICK);
}

boolean I_RampTimeEnabled(void)
{
    return (joy_outer_turn_speed > 0);
}

static void CalcExtraScale(axes_t *ax)
{
    if ((ax->x.extra_sens > 0.0f || ax->y.extra_sens > 0.0f) && ax->max_mag)
    {
        if (ax->ramp_time > 0)
        {
            static uint64_t start_time;
            static uint64_t elapsed_time;

            if (ax->extra_active)
            {
                if (elapsed_time < ax->ramp_time)
                {
                    // Continue ramp.
                    elapsed_time = ax->time - start_time;
                    elapsed_time = BETWEEN(0, ax->ramp_time, elapsed_time);
                    ax->extra_scale = (float)elapsed_time / ax->ramp_time;
                }
            }
            else
            {
                // Start ramp.
                start_time = ax->time;
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
//
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

static void ScaleMovement(axes_t *ax, float *xaxis, float *yaxis)
{
    if (*xaxis || *yaxis)
    {
        if (ax->circle_to_square)
        {
            CircleToSquare(xaxis, yaxis);
        }

        *xaxis *= ax->x.sens;
        *yaxis *= ax->y.sens;
    }
}

static void ScaleCamera(axes_t *ax, float *xaxis, float *yaxis)
{
    CalcExtraScale(ax);

    if (*xaxis || *yaxis)
    {
        *xaxis *= ax->x.sens + ax->extra_scale * ax->x.extra_sens;

        if (padlook && I_StandardLayout())
        {
            *yaxis *= ax->y.sens + ax->extra_scale * ax->y.extra_sens;
        }
        else
        {
            *yaxis = 0.0f;
        }
    }
}

static float CalcAxialValue(axes_t *ax, float input)
{
    const float input_mag = fabsf(input);

    if (input_mag <= ax->inner_deadzone)
    {
        return 0.0f;
    }
    else if (input_mag >= ax->outer_deadzone)
    {
        return SGNF(input);
    }
    else
    {
        float scale = REMAP(input_mag, ax->inner_deadzone, ax->outer_deadzone);
        scale = powf(scale, ax->exponent) / input_mag;
        return (input * scale);
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

static void (*CalcMovement)(axes_t *ax, float *xaxis, float *yaxis);
static void (*CalcCamera)(axes_t *ax, float *xaxis, float *yaxis);

static void CalcAxial(axes_t *ax, float *xaxis, float *yaxis)
{
    const float x_input = GetInputValue(ax->x.data);
    const float y_input = GetInputValue(ax->y.data);
    const float input_mag = LENGTH_F(x_input, y_input);

    *xaxis = CalcAxialValue(ax, x_input);
    *yaxis = CalcAxialValue(ax, y_input);
    ax->max_mag = (input_mag >= ax->outer_deadzone);
}

void I_CalcRadial(axes_t *ax, float *xaxis, float *yaxis)
{
    const float x_input = GetInputValue(ax->x.data);
    const float y_input = GetInputValue(ax->y.data);
    const float input_mag = LENGTH_F(x_input, y_input);

    if (input_mag <= ax->inner_deadzone)
    {
        *xaxis = 0.0f;
        *xaxis = 0.0f;
        ax->max_mag = false;
    }
    else if (input_mag >= ax->outer_deadzone)
    {
        *xaxis = x_input / input_mag;
        *yaxis = y_input / input_mag;
        ax->max_mag = true;
    }
    else
    {
        float scale = REMAP(input_mag, ax->inner_deadzone, ax->outer_deadzone);
        scale = powf(scale, ax->exponent) / input_mag;
        *xaxis = x_input * scale;
        *yaxis = y_input * scale;
        ax->max_mag = false;
    }
}

//
// I_CalcGamepadAxes
//

static void ResetData(void);

void I_CalcGamepadAxes(boolean strafe)
{
    CalcMovement(&movement, &axes[AXIS_STRAFE], &axes[AXIS_FORWARD]);
    ScaleMovement(&movement, &axes[AXIS_STRAFE], &axes[AXIS_FORWARD]);

    camera.time = I_GetTimeUS();

    if (I_StandardLayout() || strafe)
    {
        CalcCamera(&camera, &axes[AXIS_TURN], &axes[AXIS_LOOK]);
        I_SetStickMoving(axes[AXIS_TURN] || axes[AXIS_LOOK]);
        I_SetPendingFlickStickReset(true);
        ScaleCamera(&camera, &axes[AXIS_TURN], &axes[AXIS_LOOK]);
    }
    else
    {
        if (I_PendingFlickStickReset())
        {
            I_ResetFlickStick();
        }

        I_CalcFlickStick(&camera, &axes[AXIS_TURN], &axes[AXIS_LOOK]);
        I_SetStickMoving(I_FlickStickActive());
    }

    ResetData();
    G_UpdateDeltaTics(camera.time - camera.last_time);
    camera.last_time = camera.time;
}

//
// I_UpdateAxesData
//

void I_UpdateAxesData(const event_t *ev)
{
    *axes_data[AXIS_LEFTX] = ev->data1.i;
    *axes_data[AXIS_LEFTY] = ev->data2.i;
    *axes_data[AXIS_RIGHTX] = ev->data3.i;
    *axes_data[AXIS_RIGHTY] = ev->data4.i;
}

static void UpdateStickLayout(void)
{
    int *layouts[NUM_LAYOUTS][NUM_AXES] = {
        // Off
        {&movement.x.data, &movement.y.data, &camera.x.data, &camera.y.data},
        // Default
        {&movement.x.data, &movement.y.data, &camera.x.data, &camera.y.data},
        // Southpaw
        {&camera.x.data, &camera.y.data, &movement.x.data, &movement.y.data},
        // Legacy
        {&camera.x.data, &movement.y.data, &movement.x.data, &camera.y.data},
        // Legacy Southpaw
        {&movement.x.data, &camera.y.data, &camera.x.data, &movement.y.data},
        // Flick Stick
        {&movement.x.data, &movement.y.data, &camera.x.data, &camera.y.data},
        // Flick Stick Southpaw
        {&camera.x.data, &camera.y.data, &movement.x.data, &movement.y.data},
    };

    for (int i = 0; i < NUM_AXES; i++)
    {
        axes_data[i] = layouts[joy_stick_layout][i];
    }

    UpdateRawLayout();
}

static void ResetData(void)
{
    movement.x.data = 0;
    movement.y.data = 0;
    camera.x.data = 0;
    camera.y.data = 0;
}

void I_ResetGamepadAxes(void)
{
    memset(axes, 0, sizeof(axes));
}

void I_ResetGamepadState(void)
{
    I_ResetGamepadAxes();
    ResetData();
    camera.max_mag = false;
    camera.extra_scale = 0.0f;
    camera.extra_active = false;
    I_ResetFlickStick();
    I_ResetGyro();
}

static void RefreshSettings(void);

void I_ResetGamepad(void)
{
    I_ResetGamepadState();
    UpdateStickLayout();
    RefreshSettings();
    G_UpdateGamepadVariables();
    I_FlushGamepadEvents();
}

static void RefreshSettings(void)
{
    void *axes_func[] = {CalcAxial, I_CalcRadial};

    CalcMovement = axes_func[joy_movement_deadzone_type];
    CalcCamera = axes_func[joy_camera_deadzone_type];

    movement.x.sens = joy_strafe_sensitivity / 10.0f;
    movement.y.sens = joy_forward_sensitivity / 10.0f;

    camera.x.sens = joy_turn_speed / (float)DEFAULT_SPEED;
    camera.y.sens = joy_look_speed / (float)DEFAULT_SPEED;

    camera.x.extra_sens = joy_outer_turn_speed / (float)DEFAULT_SPEED;
    camera.y.extra_sens = joy_outer_look_speed / (float)DEFAULT_SPEED;
    camera.ramp_time = joy_outer_ramp_time * 10000;

    movement.circle_to_square = (joy_movement_type > 0);

    movement.exponent = joy_movement_curve / 10.0f;
    camera.exponent = I_StandardLayout() ? (joy_camera_curve / 10.0f) : 1.0f;

    movement.inner_deadzone = joy_movement_inner_deadzone / 100.0f;
    camera.inner_deadzone = joy_camera_inner_deadzone / 100.0f;
    movement.inner_deadzone = MAX(MIN_DEADZONE, movement.inner_deadzone);
    camera.inner_deadzone = MAX(MIN_DEADZONE, camera.inner_deadzone);

    movement.outer_deadzone = 1.0f - joy_movement_outer_deadzone / 100.0f;
    camera.outer_deadzone = 1.0f - joy_camera_outer_deadzone / 100.0f;

    trigger_threshold = SDL_JOYSTICK_AXIS_MAX * joy_trigger_deadzone / 100;

    I_RefreshFlickStickSettings();
    I_RefreshGyroSettings();
}

#define BIND_NUM_PADADV(name, v, a, b, help) \
    M_BindNum(#name, &name, NULL, (v), (a), (b), ss_padadv, wad_no, help)

void I_BindGamepadVariables(void)
{
    BIND_BOOL(joy_enable, true, "Enable gamepad");
    BIND_NUM_GENERAL(joy_device, 1, 0, UL, "Gamepad device (do not modify)");
    BIND_NUM(joy_platform, PLATFORM_AUTO, PLATFORM_AUTO, NUM_PLATFORMS - 1,
        "Gamepad platform (0 = Auto; 1 = Xbox 360; 2 = Xbox One/Series; "
        "3 = Playstation 3; 4 = Playstation 4; 5 = Playstation 5; 6 = Switch)");
    BIND_NUM_PADADV(joy_stick_layout, LAYOUT_DEFAULT, 0, NUM_LAYOUTS - 1,
        "Analog stick layout (0 = Off; 1 = Default; 2 = Southpaw; 3 = Legacy; "
        "4 = Legacy Southpaw; 5 = Flick Stick; 6 = Flick Stick Southpaw)");
    BIND_NUM_PADADV(joy_forward_sensitivity, 10, 0, 40,
        "Forward sensitivity (0 = 0.0x; 40 = 4.0x)");
    BIND_NUM_PADADV(joy_strafe_sensitivity, 10, 0, 40,
        "Strafe sensitivity (0 = 0.0x; 40 = 4.0x)");
    BIND_NUM_GENERAL(joy_turn_speed, DEFAULT_SPEED, 0, 720,
        "Turn speed [degrees/second]");
    BIND_NUM_GENERAL(joy_look_speed, DEFAULT_SPEED * 9 / 16, 0, 720,
        "Look speed [degrees/second]");
    BIND_NUM_PADADV(joy_outer_turn_speed, 0, 0, 720,
        "Extra turn speed at outer deadzone [degrees/second]");
    BIND_NUM(joy_outer_look_speed, 0, 0, 720,
        "Extra look speed at outer deadzone [degrees/second]");
    BIND_NUM_PADADV(joy_outer_ramp_time, 20, 0, 100,
        "Ramp time for extra speed (0 = Instant; 100 = 1000 ms)");
    BIND_NUM_PADADV(joy_movement_type, 1, 0, 1,
        "Movement type (0 = Normalized; 1 = Faster Diagonals)");
    BIND_NUM(joy_movement_curve, 10, 10, 30,
        "Movement response curve (10 = Linear; 20 = Squared; 30 = Cubed)");
    BIND_NUM_PADADV(joy_camera_curve, 20, 10, 30,
        "Camera response curve (10 = Linear; 20 = Squared; 30 = Cubed)");
    BIND_NUM(joy_movement_deadzone_type, 1, 0, 1,
        "Movement deadzone type (0 = Axial; 1 = Radial)");
    BIND_NUM(joy_camera_deadzone_type, 1, 0, 1,
        "Camera deadzone type (0 = Axial; 1 = Radial)");
    BIND_NUM_GENERAL(joy_movement_inner_deadzone, 15, 0, 50,
        "Movement inner deadzone [percent]");
    BIND_NUM_GENERAL(joy_camera_inner_deadzone, 15, 0, 50,
        "Camera inner deadzone [percent]");
    BIND_NUM(joy_movement_outer_deadzone, 2, 0, 30,
        "Movement outer deadzone [percent]");
    BIND_NUM(joy_camera_outer_deadzone, 2, 0, 30,
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
