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

#include <string.h>

#include "i_gyro.h"
#include "i_input.h"
#include "i_printf.h"
#include "g_game.h"
#include "i_gamepad.h"
#include "i_rumble.h"
#include "i_timer.h"
#include "i_video.h"
#include "m_config.h"
#include "m_input.h"
#include "m_vector.h"

#define CALBITS 24 // 8.24 fixed-point.
#define CALUNIT (1 << CALBITS)
#define CAL2CFG(x) ((x) * (double)CALUNIT)
#define CFG2CAL(x) ((x) / (double)CALUNIT)

#define DEFAULT_ACCEL 1.0f // Default accelerometer magnitude.

#define NUM_SAMPLES 256 // Smoothing samples.

#define SMOOTH_HALF_TIME 4.0f // 1/0.25

#define SHAKINESS_MAX_THRESH 0.4f
#define SHAKINESS_MIN_THRESH 0.01f

#define COR_STILL_RATE 1.0f
#define COR_SHAKY_RATE 0.1f

#define COR_GYRO_FACTOR 0.1f
#define COR_GYRO_MIN_THRESH 0.05f
#define COR_GYRO_MAX_THRESH 0.25f

#define COR_MIN_SPEED 0.01f

#define RELAX_FACTOR_60 2.0943952f // 60 degrees

#define SGNF2(x) ((x) < 0.0f ? -1.0f : 1.0f) // Doesn't return zero.

typedef enum
{
    SPACE_LOCAL,
    SPACE_PLAYER,
} space_t;

typedef enum
{
    ROLL_OFF,
    ROLL_ON,
    ROLL_INVERT,
} roll_t;

typedef enum
{
    ACTION_NONE,
    ACTION_DISABLE,
    ACTION_ENABLE,
    ACTION_INVERT,
    ACTION_RESET,
    ACTION_RESET_DISABLE,
    ACTION_RESET_ENABLE,
    ACTION_RESET_INVERT,
} action_t;

static boolean gyro_enable;
static space_t gyro_space;
static roll_t gyro_local_roll;
static int gyro_button_action;
static int gyro_stick_action;
static int gyro_turn_sensitivity;
static int gyro_look_sensitivity;
static int gyro_acceleration;
static int gyro_accel_min_threshold;
static int gyro_accel_max_threshold;
static int gyro_smooth_threshold;
static int gyro_smooth_time;
static int gyro_tightening;
static fixed_t gyro_calibration_a;
static fixed_t gyro_calibration_x;
static fixed_t gyro_calibration_y;
static fixed_t gyro_calibration_z;

#define GYRO_TICS 7 // 200 ms
static boolean gyro_tap;
static boolean gyro_press;
static int gyro_time;

float gyro_axes[NUM_GYRO_AXES];

typedef struct motion_s
{
    uint64_t last_time;                 // Last update time (us).
    float delta_time;                   // Gyro delta time (seconds).
    vec gyro;                           // Gyro pitch, yaw, roll.
    vec gyro_offset;                    // Calibration offsets for gyro.
    vec accel;                          // Accelerometer x, y, z.
    float accel_magnitude;              // Accelerometer magnitude.
    boolean calibrating;                // Currently calibrating?
    vec gravity;                        // Gravity vector.
    vec smooth_accel;                   // Smoothed acceleration.
    float shakiness;                    // Shakiness.
    boolean stick_moving;               // Moving the camera stick?
    int index;                          // Smoothing sample index.
    float pitch_samples[NUM_SAMPLES];   // Pitch smoothing samples.
    float yaw_samples[NUM_SAMPLES];     // Yaw smoothing samples.

    action_t button_action;             // Gyro button action.
    action_t stick_action;              // Camera stick action.
    float smooth_time;                  // Smoothing window (seconds);
    float lower_smooth;                 // Lower smoothing threshold (rad/s).
    float upper_smooth;                 // Upper smoothing threshold (rad/s).
    float tightening;                   // Tightening threshold (rad/s).
    float min_pitch_sens;               // Min pitch sensitivity.
    float max_pitch_sens;               // Max pitch sensitivity.
    float min_yaw_sens;                 // Min yaw sensitivity.
    float max_yaw_sens;                 // Max yaw sensitivity.
    float accel_min_thresh;             // Lower threshold for accel (rad/s).
    float accel_max_thresh;             // Upper threshold for accel (rad/s).
} motion_t;

static motion_t motion;

typedef struct
{
    gyro_calibration_state_t state;
    int start_time;
    int finish_time;
    int accel_count;
    int gyro_count;
    float accel_sum;
    vec gyro_sum;
} calibration_t;

calibration_t cal;

gyro_calibration_state_t I_GetGyroCalibrationState(void)
{
    return cal.state;
}

static void ClearGyroCalibration(void)
{
    memset(&cal, 0, sizeof(cal));
    motion.accel_magnitude = DEFAULT_ACCEL;
    motion.gyro_offset = (vec){0.0f, 0.0f, 0.0f};
    gyro_calibration_a = 0;
    gyro_calibration_x = 0;
    gyro_calibration_y = 0;
    gyro_calibration_z = 0;
}

void I_LoadGyroCalibration(void)
{
    motion.accel_magnitude = CFG2CAL(gyro_calibration_a) + DEFAULT_ACCEL;
    motion.gyro_offset.x = CFG2CAL(gyro_calibration_x);
    motion.gyro_offset.y = CFG2CAL(gyro_calibration_y);
    motion.gyro_offset.z = CFG2CAL(gyro_calibration_z);

    I_Printf(VB_DEBUG, "Gyro calibration loaded: a: %f x: %f y: %f z: %f",
             motion.accel_magnitude, motion.gyro_offset.x, motion.gyro_offset.y,
             motion.gyro_offset.z);
}

static void SaveCalibration(void)
{
    gyro_calibration_a = CAL2CFG(motion.accel_magnitude - DEFAULT_ACCEL);
    gyro_calibration_x = CAL2CFG(motion.gyro_offset.x);
    gyro_calibration_y = CAL2CFG(motion.gyro_offset.y);
    gyro_calibration_z = CAL2CFG(motion.gyro_offset.z);
}

static void ProcessAccelCalibration(void)
{
    cal.accel_count++;
    cal.accel_sum += vec_length(motion.accel);
}

static void ProcessGyroCalibration(void)
{
    cal.gyro_count++;
    cal.gyro_sum = vec_add(cal.gyro_sum, motion.gyro);
}

static void PostProcessCalibration(void)
{
    if (!cal.accel_count || !cal.gyro_count)
    {
        return;
    }

    motion.accel_magnitude = cal.accel_sum / cal.accel_count;
    motion.accel_magnitude = BETWEEN(0.0f, 2.0f, motion.accel_magnitude);

    motion.gyro_offset = vec_scale(cal.gyro_sum, 1.0f / cal.gyro_count);
    motion.gyro_offset = vec_clamp(-1.0f, 1.0f, motion.gyro_offset);

    SaveCalibration();

    I_Printf(VB_DEBUG,
             "Gyro calibration updated: a: %f x: %f y: %f z: %f accel_count: "
             "%d gyro_count: %d",
             motion.accel_magnitude, motion.gyro_offset.x, motion.gyro_offset.y,
             motion.gyro_offset.z, cal.accel_count, cal.gyro_count);
}

void I_UpdateGyroCalibrationState(void)
{
    switch (cal.state)
    {
        case GYRO_CALIBRATION_INACTIVE:
            I_DisableRumble();
            cal.state = GYRO_CALIBRATION_STARTING;
            cal.start_time = I_GetTimeMS();
            break;

        case GYRO_CALIBRATION_STARTING:
            if (I_GetTimeMS() - cal.start_time > 1500)
            {
                motion.calibrating = true;
                ClearGyroCalibration();
                cal.state = GYRO_CALIBRATION_ACTIVE;
                cal.start_time = I_GetTimeMS();
            }
            break;

        case GYRO_CALIBRATION_ACTIVE:
            if (I_GetTimeMS() - cal.start_time > 3000)
            {
                motion.calibrating = false;
                PostProcessCalibration();
                cal.state = GYRO_CALIBRATION_COMPLETE;
                cal.finish_time = I_GetTimeMS();
            }
            break;

        case GYRO_CALIBRATION_COMPLETE:
            if (I_GetTimeMS() - cal.finish_time > 1500)
            {
                cal.state = GYRO_CALIBRATION_INACTIVE;
                I_UpdateRumbleEnabled();
            }
            break;
    }
}

boolean I_GyroEnabled(void)
{
    return gyro_enable;
}

boolean I_GyroAcceleration(void)
{
    return (gyro_acceleration > 10);
}

void I_SetStickMoving(boolean condition)
{
    motion.stick_moving = condition;
}

static void ResetCameraPress(void)
{
    if (M_InputGameActive(input_gyro))
    {
        if (!gyro_press)
        {
            players[consoleplayer].centering = true;
            gyro_press = true;
        }
    }
    else
    {
        gyro_press = false;
    }
}

static void ResetCameraTap(void)
{
    if (M_InputGameActive(input_gyro))
    {
        if (!gyro_tap && !gyro_press)
        {
            gyro_time = I_GetTime();
            gyro_tap = true;
            gyro_press = true;
        }
        else if (gyro_tap && gyro_press && I_GetTime() - gyro_time > GYRO_TICS)
        {
            gyro_tap = false;
        }
    }
    else
    {
        gyro_press = false;

        if (gyro_tap)
        {
            if (I_GetTime() - gyro_time <= GYRO_TICS)
            {
                players[consoleplayer].centering = true;
            }

            gyro_tap = false;
        }
    }
}

static boolean GyroActive(void)
{
    // Camera stick action has priority over gyro button action.
    if (motion.stick_moving)
    {
        switch (motion.stick_action)
        {
            case ACTION_DISABLE:
                return false;

            case ACTION_ENABLE:
                return true;

            default:
                break;
        }
    }

    switch (motion.button_action)
    {
        case ACTION_DISABLE:
            return !M_InputGameActive(input_gyro);

        case ACTION_ENABLE:
            return M_InputGameActive(input_gyro);

        case ACTION_RESET_DISABLE:
            ResetCameraTap();
            return !M_InputGameActive(input_gyro);

        case ACTION_RESET_ENABLE:
            ResetCameraTap();
            return M_InputGameActive(input_gyro);

        case ACTION_RESET:
            ResetCameraPress();
            break;

        case ACTION_RESET_INVERT:
            ResetCameraTap();
            break;

        default:
            break;
    }

    return true;
}

//
// I_CalcGyroAxes
//

void I_CalcGyroAxes(boolean strafe)
{
    if (GyroActive())
    {
        gyro_axes[GYRO_TURN] = !strafe ? RAD2TIC(gyro_axes[GYRO_TURN]) : 0.0f;

        if (padlook)
        {
            gyro_axes[GYRO_LOOK] = RAD2TIC(gyro_axes[GYRO_LOOK]);

            if (correct_aspect_ratio)
            {
                gyro_axes[GYRO_LOOK] /= 1.2f;
            }
        }
        else
        {
            gyro_axes[GYRO_LOOK] = 0.0f;
        }

        if ((motion.button_action == ACTION_INVERT
             || motion.button_action == ACTION_RESET_INVERT)
            && M_InputGameActive(input_gyro))
        {
            gyro_axes[GYRO_TURN] = -gyro_axes[GYRO_TURN];
            gyro_axes[GYRO_LOOK] = -gyro_axes[GYRO_LOOK];
        }
    }
    else
    {
        gyro_axes[GYRO_TURN] = 0.0f;
        gyro_axes[GYRO_LOOK] = 0.0f;
    }
}

static void (*AccelerateGyro)(void);

static void AccelerateGyro_Skip(void)
{
    motion.gyro.x *= motion.min_pitch_sens;
    motion.gyro.y *= motion.min_yaw_sens;
}

static void AccelerateGyro_Full(void)
{
    float magnitude = LENGTH_F(motion.gyro.x, motion.gyro.y);
    magnitude -= motion.accel_min_thresh;
    magnitude = MAX(0.0f, magnitude);

    const float denom = motion.accel_max_thresh - motion.accel_min_thresh;
    float accel;
    if (denom <= 0.0f)
    {
        accel = magnitude > 0.0f ? 1.0f : 0.0f;
    }
    else
    {
        accel = magnitude / denom;
        accel = BETWEEN(0.0f, 1.0f, accel);
    }
    const float no_accel = (1.0f - accel);

    motion.gyro.x *=
        (motion.min_pitch_sens * no_accel + motion.max_pitch_sens * accel);

    motion.gyro.y *=
        (motion.min_yaw_sens * no_accel + motion.max_yaw_sens * accel);
}

static void (*TightenGyro)(void);

static void TightenGyro_Skip(void)
{
    // no-op
}

static void TightenGyro_Full(void)
{
    const float magnitude = LENGTH_F(motion.gyro.x, motion.gyro.y);

    if (magnitude < motion.tightening)
    {
        const float factor = magnitude / motion.tightening;
        motion.gyro.x *= factor;
        motion.gyro.y *= factor;
    }
}

static void GetSmoothedGyro(float smooth_factor, float *smooth_pitch,
                            float *smooth_yaw)
{
    motion.index = (motion.index + (NUM_SAMPLES - 1)) % NUM_SAMPLES;
    motion.pitch_samples[motion.index] = motion.gyro.x * smooth_factor;
    motion.yaw_samples[motion.index] = motion.gyro.y * smooth_factor;

    const float delta_time =
        BETWEEN(1.0e-6f, motion.smooth_time, motion.delta_time);
    int max_samples = lroundf((float)motion.smooth_time / delta_time);
    max_samples = BETWEEN(1, NUM_SAMPLES, max_samples);

    *smooth_pitch = motion.pitch_samples[motion.index] / max_samples;
    *smooth_yaw = motion.yaw_samples[motion.index] / max_samples;

    for (int i = 1; i < max_samples; i++)
    {
        const int index = (motion.index + i) % NUM_SAMPLES;
        *smooth_pitch += motion.pitch_samples[index] / max_samples;
        *smooth_yaw += motion.yaw_samples[index] / max_samples;
    }
}

static float GetRawFactorGyro(float magnitude)
{
    const float denom = motion.upper_smooth - motion.lower_smooth;

    if (denom <= 0.0f)
    {
        return (magnitude < motion.lower_smooth ? 0.0f : 1.0f);
    }
    else
    {
        const float raw_factor = (magnitude - motion.lower_smooth) / denom;
        return BETWEEN(0.0f, 1.0f, raw_factor);
    }
}

static void (*SmoothGyro)(void);

static void SmoothGyro_Skip(void)
{
    // no-op
}

static void SmoothGyro_Full(void)
{
    const float magnitude = LENGTH_F(motion.gyro.x, motion.gyro.y);
    const float raw_factor = GetRawFactorGyro(magnitude);
    const float raw_pitch = motion.gyro.x * raw_factor;
    const float raw_yaw = motion.gyro.y * raw_factor;

    const float smooth_factor = 1.0f - raw_factor;
    float smooth_pitch, smooth_yaw;
    GetSmoothedGyro(smooth_factor, &smooth_pitch, &smooth_yaw);

    motion.gyro.x = raw_pitch + smooth_pitch;
    motion.gyro.y = raw_yaw + smooth_yaw;
}

static float raw[2];

void I_GetRawGyroScaleMenu(float *scale, float *limit)
{
    const float deg_per_sec = LENGTH_F(raw[0], raw[1]) * 180.0f / PI_F;
    *scale = BETWEEN(0.0f, 50.0f, deg_per_sec) / 50.0f;
    *limit = gyro_smooth_threshold / 500.0f;
}

static void SaveRawGyroData(void)
{
    raw[0] = motion.gyro.x;
    raw[1] = motion.gyro.y;
}

//
// Gyro Space
//
// Based on gyro space implementations by Julian "Jibb" Smart.
// http://gyrowiki.jibbsmart.com/blog:player-space-gyro-and-alternatives-explained
//

static void (*ApplyGyroSpace)(void);

static void ApplyGyroSpace_Local(void)
{
    switch (gyro_local_roll)
    {
        case ROLL_ON:
            motion.gyro.y -= motion.gyro.z;
            break;

        case ROLL_INVERT:
            motion.gyro.y += motion.gyro.z;
            break;

        default: // ROLL_OFF
            break;
    }
}

static void ApplyGyroSpace_Player(void)
{
    const vec grav_norm = vec_normalize(motion.gravity);
    const float world_yaw =
        motion.gyro.y * grav_norm.y + motion.gyro.z * grav_norm.z;

    const float world_part = fabsf(world_yaw) * RELAX_FACTOR_60;
    const float gyro_part = LENGTH_F(motion.gyro.y, motion.gyro.z);

    motion.gyro.y = -SGNF2(world_yaw) * MIN(world_part, gyro_part);
}

//
// CalcGravity
//
// Based on sensor fusion implementation by Julian "Jibb" Smart.
// http://gyrowiki.jibbsmart.com/blog:finding-gravity-with-sensor-fusion
//

static void (*CalcGravityVector)(void);

static void CalcGravityVector_Skip(void)
{
    // no-op
}

static void CalcGravityVector_Full(void)
{
    // Convert gyro input to reverse rotation.
    const float angle_speed = vec_length(motion.gyro);
    const float angle = angle_speed * motion.delta_time;
    const vec negative_gyro = vec_negative(motion.gyro);
    const quat reverse_rotation = angle_axis(angle, negative_gyro);

    // Rotate gravity vector.
    motion.gravity = vec_rotate(motion.gravity, reverse_rotation);

    // Check accelerometer magnitude now.
    const float accel_magnitude = vec_length(motion.accel);
    if (accel_magnitude <= 0.0f)
    {
        return;
    }
    const vec accel_norm = vec_scale(motion.accel, 1.0f / accel_magnitude);

    // Shakiness/smoothness.
    motion.smooth_accel = vec_rotate(motion.smooth_accel, reverse_rotation);
    const float smooth_factor = exp2f(-motion.delta_time * SMOOTH_HALF_TIME);
    motion.shakiness *= smooth_factor;
    const vec delta_accel = vec_subtract(motion.accel, motion.smooth_accel);
    const float delta_accel_magnitude = vec_length(delta_accel);
    motion.shakiness = MAX(motion.shakiness, delta_accel_magnitude);
    motion.smooth_accel =
        vec_lerp(motion.accel, motion.smooth_accel, smooth_factor);

    // Find the difference between gravity and raw acceleration.
    const vec new_gravity = vec_scale(accel_norm, -motion.accel_magnitude);
    const vec gravity_delta = vec_subtract(new_gravity, motion.gravity);
    const vec gravity_direction = vec_normalize(gravity_delta);

    // Calculate correction rate.
    float still_or_shaky = (motion.shakiness - SHAKINESS_MIN_THRESH)
                           / (SHAKINESS_MAX_THRESH - SHAKINESS_MIN_THRESH);
    still_or_shaky = BETWEEN(0.0f, 1.0f, still_or_shaky);
    float correction_rate =
        COR_STILL_RATE + (COR_SHAKY_RATE - COR_STILL_RATE) * still_or_shaky;

    // Limit correction rate in proportion to gyro rate.
    const float angle_speed_adjusted = angle_speed * COR_GYRO_FACTOR;
    const float correction_limit = MAX(angle_speed_adjusted, COR_MIN_SPEED);
    if (correction_rate > correction_limit)
    {
        const float gravity_delta_magnitude = vec_length(gravity_delta);
        float close_factor = (gravity_delta_magnitude - COR_GYRO_MIN_THRESH)
                             / (COR_GYRO_MAX_THRESH - COR_GYRO_MIN_THRESH);
        close_factor = BETWEEN(0.0f, 1.0f, close_factor);
        correction_rate = correction_limit
                          + (correction_rate - correction_limit) * close_factor;
    }

    // Apply correction to gravity vector.
    const vec correction =
        vec_scale(gravity_direction, correction_rate * motion.delta_time);
    if (vec_lengthsquared(correction) < vec_lengthsquared(gravity_delta))
    {
        motion.gravity = vec_add(motion.gravity, correction);
    }
    else
    {
        motion.gravity = vec_scale(accel_norm, -motion.accel_magnitude);
    }
}

static void ApplyCalibration(void)
{
    motion.gyro = vec_subtract(motion.gyro, motion.gyro_offset);
}

static float GetDeltaTime(void)
{
    const uint64_t current_time = I_GetTimeUS();
    float delta_time = (current_time - motion.last_time) * 1.0e-6f;
    motion.last_time = current_time;
    return BETWEEN(0.0f, 0.05f, delta_time);
}

//
// I_UpdateGyroData
//
// Based on gyro implementation by Julian "Jibb" Smart.
// http://gyrowiki.jibbsmart.com/blog:good-gyro-controls-part-1:the-gyro-is-a-mouse
//

void I_UpdateGyroData(const event_t *ev)
{
    motion.delta_time = GetDeltaTime();
    motion.gyro.x = ev->data1.f; // pitch
    motion.gyro.y = ev->data2.f; // yaw
    motion.gyro.z = ev->data3.f; // roll

    if (motion.calibrating)
    {
        motion.delta_time = ev->data4.f;
        ProcessGyroCalibration();
    }

    ApplyCalibration();
    CalcGravityVector();
    ApplyGyroSpace();

    SaveRawGyroData();

    SmoothGyro();
    TightenGyro();
    AccelerateGyro();

    gyro_axes[GYRO_LOOK] += motion.gyro.x * motion.delta_time;
    gyro_axes[GYRO_TURN] += motion.gyro.y * motion.delta_time;
}

//
// I_UpdateAccelData
//

void I_UpdateAccelData(const float *data)
{
    motion.accel.x = data[0];
    motion.accel.y = data[1];
    motion.accel.z = data[2];

    if (motion.calibrating)
    {
        ProcessAccelCalibration();
    }
}

static void ResetGyroData(void)
{
    motion.delta_time = 0.0f;
    motion.gyro = (vec){0.0f, 0.0f, 0.0f};
    motion.accel = (vec){0.0f, 0.0f, 0.0f};
}

void I_ResetGyroAxes(void)
{
    memset(gyro_axes, 0, sizeof(gyro_axes));
}

void I_ResetGyro(void)
{
    I_ResetGyroAxes();
    ResetGyroData();
    motion.gravity = (vec){0.0f, 0.0f, 0.0f};
    motion.smooth_accel = (vec){0.0f, 0.0f, 0.0f};
    motion.shakiness = 0.0f;
    motion.stick_moving = false;
    motion.index = 0;
    memset(motion.pitch_samples, 0, sizeof(motion.pitch_samples));
    memset(motion.yaw_samples, 0, sizeof(motion.yaw_samples));
    gyro_tap = false;
    gyro_press = false;
    gyro_time = 0;
}

void I_UpdateGyroSteadying(void)
{
    // Menu item for gyro "steadying" controls both smoothing threshold and
    // tightening threshold.
    gyro_tightening = gyro_smooth_threshold;
}

void I_RefreshGyroSettings(void)
{
    if (gyro_space == SPACE_PLAYER)
    {
        CalcGravityVector = CalcGravityVector_Full;
        ApplyGyroSpace = ApplyGyroSpace_Player;
    }
    else
    {
        CalcGravityVector = CalcGravityVector_Skip;
        ApplyGyroSpace = ApplyGyroSpace_Local;
    }

    motion.button_action = gyro_button_action;
    motion.stick_action = gyro_stick_action;

    AccelerateGyro =
        I_GyroAcceleration() ? AccelerateGyro_Full : AccelerateGyro_Skip;
    motion.min_pitch_sens = gyro_look_sensitivity / 10.0f;
    motion.min_yaw_sens = gyro_turn_sensitivity / 10.0f;
    motion.max_pitch_sens = motion.min_pitch_sens * gyro_acceleration / 10.0f;
    motion.max_yaw_sens = motion.min_yaw_sens * gyro_acceleration / 10.0f;
    gyro_accel_max_threshold =
        MAX(gyro_accel_max_threshold, gyro_accel_min_threshold);
    motion.accel_min_thresh = gyro_accel_min_threshold * PI_F / 180.0f;
    motion.accel_max_thresh = gyro_accel_max_threshold * PI_F / 180.0f;

    SmoothGyro = (gyro_smooth_threshold && gyro_smooth_time) ? SmoothGyro_Full
                                                             : SmoothGyro_Skip;
    motion.upper_smooth = gyro_smooth_threshold / 10.0f * PI_F / 180.0f;
    motion.lower_smooth = motion.upper_smooth * 0.5f;
    motion.smooth_time = gyro_smooth_time / 1000.0f;

    TightenGyro = gyro_tightening ? TightenGyro_Full : TightenGyro_Skip;
    motion.tightening = gyro_tightening / 10.0f * PI_F / 180.0f;
}

#define BIND_NUM_GYRO(name, v, a, b, help) \
    M_BindNum(#name, &name, NULL, (v), (a), (b), ss_gyro, wad_no, help)

#define BIND_BOOL_GYRO(name, v, help) \
    M_BindBool(#name, &name, NULL, (v), ss_gyro, wad_no, help)

void I_BindGyroVaribales(void)
{
    BIND_BOOL_GYRO(gyro_enable, false,
        "Enable gamepad gyro aiming");
    BIND_NUM_GYRO(gyro_space,
        SPACE_PLAYER, SPACE_LOCAL, SPACE_PLAYER,
        "Gyro space (0 = Local; 1 = Player)");
    BIND_NUM(gyro_local_roll,
        ROLL_ON, ROLL_OFF, ROLL_INVERT,
        "Local gyro space uses roll (0 = Off; 1 = On; 2 = Invert)");
    BIND_NUM_GYRO(gyro_button_action,
        ACTION_ENABLE, ACTION_NONE, ACTION_RESET_INVERT,
        "Gyro button action (0 = None; 1 = Disable Gyro; 2 = Enable Gyro; "
        "3 = Invert Gyro; 4 = Reset Camera; 5 = Reset / Disable Gyro; "
        "6 = Reset / Enable Gyro; 7 = Reset / Invert Gyro)");
    BIND_NUM_GYRO(gyro_stick_action,
        ACTION_NONE, ACTION_NONE, ACTION_ENABLE,
        "Camera stick action (0 = None; 1 = Disable Gyro; 2 = Enable Gyro)");
    BIND_NUM_GYRO(gyro_turn_sensitivity, 25, 0, 200,
        "Gyro turn sensitivity (0 = 0.0x; 200 = 20.0x)");
    BIND_NUM_GYRO(gyro_look_sensitivity, 25, 0, 200,
        "Gyro look sensitivity (0 = 0.0x; 200 = 20.0x)");
    BIND_NUM_GYRO(gyro_acceleration, 10, 10, 200,
        "Gyro acceleration multiplier (10 = 1.0x; 200 = 20.0x)");
    BIND_NUM_GYRO(gyro_accel_min_threshold, 0, 0, 300,
        "Lower threshold for applying gyro acceleration [degrees/second]");
    BIND_NUM_GYRO(gyro_accel_max_threshold, 75, 0, 300,
        "Upper threshold for applying gyro acceleration [degrees/second]");
    BIND_NUM_GYRO(gyro_smooth_threshold, 30, 0, 500,
        "Gyro steadying: smoothing threshold "
        "(0 = Off; 500 = 50.0 degrees/second)");
    BIND_NUM(gyro_smooth_time, 125, 0, 500,
        "Gyro steadying: smoothing time [milliseconds]");
    BIND_NUM(gyro_tightening, 30, 0, 500,
        "Gyro steadying: tightening threshold "
        "(0 = Off; 500 = 50.0 degrees/second)");

    BIND_NUM(gyro_calibration_a, 0, UL, UL, "Accelerometer calibration");
    BIND_NUM(gyro_calibration_x, 0, UL, UL, "Gyro calibration (x-axis)");
    BIND_NUM(gyro_calibration_y, 0, UL, UL, "Gyro calibration (y-axis)");
    BIND_NUM(gyro_calibration_z, 0, UL, UL, "Gyro calibration (z-axis)");
}
