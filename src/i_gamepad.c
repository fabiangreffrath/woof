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

#include "g_game.h"
#include "i_gamepad.h"
#include "i_input.h"
#include "i_timer.h"
#include "m_config.h"

#define MIN_DEADZONE 0.00003f // 1/32768
#define MIN_F 0.000001f
#define MAX_F 0.999999f
#define NUM_SAMPLES 64
#define SMOOTH_WINDOW 64000 // 64 ms
#define PI_F 3.1415927f
#define RAD2TIC(x) ((x) * 32768.0f / PI_F) // Radians to ticcmd angle.
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
static int joy_flick_time;
static int joy_flick_smoothing;
static int joy_flick_deadzone;
static int joy_flick_forward_deadzone;
static int joy_flick_snap;

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
    uint64_t time;              // Update time (us).
    uint64_t last_time;         // Last update time (us).
    axis_t x;
    axis_t y;
    uint64_t ramp_time;         // Ramp time for extra sensitivity (us).
    boolean circle_to_square;   // Circle to square correction?
    float exponent;             // Exponent for response curve.
    float inner_deadzone;       // Normalized inner deadzone.
    float outer_deadzone;       // Normalized outer deadzone.
    boolean max_mag;            // Max magnitude?
    boolean extra_active;       // Using extra sensitivity?
    float extra_scale;          // Scaling factor for extra sensitivity.
} axes_t;

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

    float time;                 // Time required to execute a flick (us).
    float lower_smooth;         // Lower smoothing threshold (rotations/s).
    float upper_smooth;         // Upper smoothing threshold (rotations/s).
    float deadzone;             // Normalized outer deadzone.
    float forward_deadzone;     // Forward deadzone angle (radians).
    float snap;                 // Snap angle (radians).
} flick_t;

static axes_t movement;         // Strafe/Forward
static axes_t camera;           // Turn/Look
static flick_t flick;           // Flick Stick

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

boolean I_UseStandardLayout(void)
{
    return (joy_layout < LAYOUT_FLICK_STICK);
}

static void ScaleCamera(axes_t *ax, float *xaxis, float *yaxis)
{
    CalcExtraScale(ax);

    if (*xaxis || *yaxis)
    {
        *xaxis *= ax->x.sens + ax->extra_scale * ax->x.extra_sens;

        if (padlook && I_UseStandardLayout())
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
    const float input_mag = sqrtf(x_input * x_input + y_input * y_input);

    *xaxis = CalcAxialValue(ax, x_input);
    *yaxis = CalcAxialValue(ax, y_input);
    ax->max_mag = (input_mag >= ax->outer_deadzone);
}

static void CalcRadial(axes_t *ax, float *xaxis, float *yaxis)
{
    const float x_input = GetInputValue(ax->x.data);
    const float y_input = GetInputValue(ax->y.data);
    const float input_mag = sqrtf(x_input * x_input + y_input * y_input);

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

static void ResetFlickSmoothing(void)
{
    flick.index = 0;
    memset(flick.samples, 0, sizeof(flick.samples));
}

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

//
// CalcFlickStick
// Based on flick stick implementation by Julian "Jibb" Smart.
// http://gyrowiki.jibbsmart.com/blog:good-gyro-controls-part-2:the-flick-stick
//

static void CalcFlickStick(axes_t *ax, float *xaxis, float *yaxis)
{
    float output_angle = 0.0f;
    float x_input, y_input;
    CalcRadial(ax, &x_input, &y_input);

    if (!ax->max_mag && flick.active)
    {
        const float input_mag = sqrtf(x_input * x_input + y_input * y_input);
        ax->max_mag = (input_mag >= flick.deadzone);
    }

    if (ax->max_mag)
    {
        float angle = atan2f(x_input, -y_input);

        if (flick.active) // Turn after flick.
        {
            const float last_angle = atan2f(flick.lastx, -flick.lasty);
            const float delta_angle = WrapAngle(angle - last_angle);
            output_angle = SmoothTurn(ax, delta_angle);
        }
        else // Start flick.
        {
            flick.active = true;
            flick.start_time = ax->time;

            if (flick.snap > 0.0f)
            {
                angle = roundf(angle / flick.snap) * flick.snap;
            }

            if (fabsf(angle) <= flick.forward_deadzone)
            {
                angle = 0.0f;
            }

            flick.target_angle = angle;
            flick.percent = 0.0f;
            ResetFlickSmoothing();
        }
    }
    else // Stop turning.
    {
        flick.active = false;
    }

    // Continue flick.
    const float last_percent = flick.percent;
    const float raw_percent = (ax->time - flick.start_time) / flick.time;
    flick.percent = EaseOut(raw_percent);
    output_angle += (flick.percent - last_percent) * flick.target_angle;

    *xaxis = RAD2TIC(output_angle);

    flick.lastx = x_input;
    flick.lasty = y_input;
}

static void ResetFlick(void)
{
    flick.reset = false;
    flick.active = false;
    flick.lastx = 0.0f;
    flick.lasty = 0.0f;
    flick.start_time = camera.time;
    flick.target_angle = 0.0f;
    flick.percent = 0.0f;
    ResetFlickSmoothing();
}

static void ResetData(void)
{
    movement.x.data = 0;
    movement.y.data = 0;
    camera.x.data = 0;
    camera.y.data = 0;
}

void I_CalcGamepadAxes(boolean strafe)
{
    CalcMovement(&movement, &axes[AXIS_STRAFE], &axes[AXIS_FORWARD]);
    ScaleMovement(&movement, &axes[AXIS_STRAFE], &axes[AXIS_FORWARD]);

    camera.time = I_GetTimeUS();

    if (I_UseStandardLayout() || strafe)
    {
        CalcCamera(&camera, &axes[AXIS_TURN], &axes[AXIS_LOOK]);
        flick.reset = true;
    }
    else
    {
        if (flick.reset)
        {
            ResetFlick();
        }

        CalcFlickStick(&camera, &axes[AXIS_TURN], &axes[AXIS_LOOK]);
    }

    ScaleCamera(&camera, &axes[AXIS_TURN], &axes[AXIS_LOOK]);

    ResetData();
    G_UpdateDeltaTics(camera.time - camera.last_time);
    camera.last_time = camera.time;
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
    ResetFlick();
    ResetData();
    camera.extra_scale = 0.0f;
    camera.extra_active = false;
}

static void UpdateStickLayout(void)
{
    int *layouts[NUM_LAYOUTS][NUM_AXES] = {
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
    camera.ramp_time = joy_outer_ramp_time * 1000;

    movement.circle_to_square = (joy_scale_diagonal_movement > 0);

    movement.exponent = joy_movement_curve / 10.0f;
    camera.exponent = joy_camera_curve / 10.0f;

    movement.inner_deadzone = joy_movement_inner_deadzone / 100.0f;
    camera.inner_deadzone = joy_camera_inner_deadzone / 100.0f;
    movement.inner_deadzone = MAX(MIN_DEADZONE, movement.inner_deadzone);
    camera.inner_deadzone = MAX(MIN_DEADZONE, camera.inner_deadzone);

    movement.outer_deadzone = 1.0f - joy_movement_outer_deadzone / 100.0f;
    camera.outer_deadzone = 1.0f - joy_camera_outer_deadzone / 100.0f;

    trigger_threshold = SDL_JOYSTICK_AXIS_MAX * joy_trigger_deadzone / 100;

    flick.time = joy_flick_time * 1000.0f;
    flick.upper_smooth = joy_flick_smoothing / 10.0f;
    flick.lower_smooth = flick.upper_smooth * 0.5f;
    flick.deadzone = joy_flick_deadzone / 100.0f;
    flick.deadzone = MIN(flick.deadzone, MAX_F);
    flick.forward_deadzone = joy_flick_forward_deadzone * PI_F / 180.0f;
    flick.snap = joy_flick_snap ? PI_F / powf(2.0f, joy_flick_snap) : 0.0f;
}

void I_ResetGamepad(void)
{
    I_ResetGamepadState();
    UpdateStickLayout();
    RefreshSettings();
    G_UpdateGamepadVariables();
    I_FlushGamepadEvents();
}

void I_BindGamepadVariables(void)
{
    BIND_BOOL(joy_enable, true, "Enable gamepad");
    BIND_NUM_GENERAL(joy_layout, LAYOUT_DEFAULT, 0, NUM_LAYOUTS - 1,
        "Analog stick layout (0 = Default; 1 = Southpaw; 2 = Legacy; "
        "3 = Legacy Southpaw; 4 = Flick Stick; 5 = Flick Stick Southpaw)");
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
    BIND_NUM(joy_flick_time, 100, 100, 500,
        "Flick time [milliseconds]");
    BIND_NUM(joy_flick_smoothing, 8, 0, 50,
        "Flick rotation smoothing threshold "
        "(0 = Off; 50 = 5.0 rotations/second)");
    BIND_NUM(joy_flick_deadzone, 90, 0, 100,
        "Flick deadzone relative to camera deadzone [percent]");
    BIND_NUM(joy_flick_forward_deadzone, 7, 0, 45,
        "Forward angle range where flicks are disabled [degrees]");
    BIND_NUM(joy_flick_snap, 0, 0, 2,
        "Snap to cardinal directions (0 = Off; 1 = 4-way; 2 = 8-way)");
}
