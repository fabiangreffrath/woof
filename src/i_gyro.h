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
//      Gamepad gyro and accelerometer.
//

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
    vec accel;                          // Accelerometer x, y, z.
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

    float accel_magnitude; // TODO: calibration
} motion_t;

extern boolean gyro_enable;             // Enable gamepad gyro aiming.
extern float gyro_axes[NUM_GYRO_AXES];  // Calculated gyro values.
extern motion_t motion;

void I_CalcGyroAxes(boolean strafe);
void I_UpdateGyroData(const struct event_s *ev);
void I_UpdateAccelData(const float *data);
void I_ResetGyroAxes(void);
void I_ResetGyro(void);
void I_RefreshGyroSettings(void);

void I_BindGyroVaribales(void);

#endif
