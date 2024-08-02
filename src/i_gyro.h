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

#ifndef __I_GYRO__
#define __I_GYRO__

#include "doomkeys.h"
#include "doomtype.h"
#include "m_vector.h"

#define NUM_SAMPLES 256

struct event_s;

typedef enum action_e
{
    ACTION_NONE,
    ACTION_DISABLE,
    ACTION_ENABLE,

    NUM_ACTIONS
} action_t;

typedef struct motion_s
{
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

extern boolean gyro_enable;             // Enable gamepad gyro aiming.
extern float gyro_axes[NUM_GYRO_AXES];  // Calculated gyro values.
extern motion_t motion;

typedef enum gyro_calibration_state_e
{
    GYRO_CALIBRATION_INACTIVE,
    GYRO_CALIBRATION_ACTIVE,
    GYRO_CALIBRATION_COMPLETE,
} gyro_calibration_state_t;

gyro_calibration_state_t I_GetGyroCalibrationState(void);
boolean I_DefaultGyroCalibration(void);
void I_ClearGyroCalibration(void);
void I_UpdateGyroCalibrationState(void);

void I_CalcGyroAxes(boolean strafe);
void I_UpdateGyroData(const struct event_s *ev);
void I_UpdateAccelData(const float *data);
void I_ResetGyroAxes(void);
void I_ResetGyro(void);
void I_UpdateGyroSteadying(void);
void I_RefreshGyroSettings(void);

void I_BindGyroVaribales(void);

#endif
