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

struct event_s;

extern float gyro_axes[NUM_GYRO_AXES];  // Calculated gyro values.

typedef enum gyro_calibration_state_e
{
    GYRO_CALIBRATION_INACTIVE,
    GYRO_CALIBRATION_STARTING,
    GYRO_CALIBRATION_ACTIVE,
    GYRO_CALIBRATION_COMPLETE,
} gyro_calibration_state_t;

gyro_calibration_state_t I_GetGyroCalibrationState(void);
void I_LoadGyroCalibration(void);
void I_UpdateGyroCalibrationState(void);

boolean I_GyroEnabled(void);
boolean I_GyroAcceleration(void);
void I_SetStickMoving(boolean condition);
void I_GetRawGyroScaleMenu(float *scale, float *limit);
void I_CalcGyroAxes(boolean strafe);
void I_UpdateGyroData(const struct event_s *ev);
void I_UpdateAccelData(const float *data);
void I_ResetGyroAxes(void);
void I_ResetGyro(void);
void I_UpdateGyroSteadying(void);
void I_RefreshGyroSettings(void);

void I_BindGyroVaribales(void);

#endif
