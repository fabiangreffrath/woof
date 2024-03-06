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

#ifndef __I_GAMEPAD__
#define __I_GAMEPAD__

#include "d_event.h"
#include "doomkeys.h"
#include "doomtype.h"

extern boolean joy_enable;                  // Enable game controller.
extern int joy_layout;                      // Analog stick layout.
extern int joy_sensitivity_forward;         // Forward axis sensitivity.
extern int joy_sensitivity_strafe;          // Strafe axis sensitivity.
extern int joy_sensitivity_turn;            // Turn axis sensitivity.
extern int joy_sensitivity_look;            // Look axis sensitivity.
extern int joy_extra_sensitivity_turn;      // Extra turn sensitivity.
extern int joy_extra_sensitivity_look;      // Extra look sensitivity.
extern int joy_extra_ramp_time;             // Ramp time for extra sensitivity.
extern boolean joy_scale_diagonal_movement; // Scale diagonal movement.
extern int joy_response_curve_movement;     // Movement response curve.
extern int joy_response_curve_camera;       // Camera response curve.
extern int joy_deadzone_type_movement;      // Movement deadzone type.
extern int joy_deadzone_type_camera;        // Camera deadzone type.
extern int joy_deadzone_movement;           // Movement deadzone percent.
extern int joy_deadzone_camera;             // Camera deadzone percent.
extern int joy_threshold_movement;          // Movement outer threshold percent.
extern int joy_threshold_camera;            // Camera outer threshold percent.
extern int joy_threshold_trigger;           // Trigger threshold percent.
extern boolean joy_invert_forward;          // Invert forward axis.
extern boolean joy_invert_strafe;           // Invert strafe axis.
extern boolean joy_invert_turn;             // Invert turn axis.
extern boolean joy_invert_look;             // Invert look axis.

extern float axes[NUM_AXES];        // Calculated controller values.
extern int trigger_threshold;       // Trigger threshold (axis resolution).

boolean I_CalcControllerAxes(void);
void I_UpdateAxesData(const struct event_s *ev);
void I_ResetControllerAxes(void);
void I_ResetControllerLevel(void);
void I_ResetController(void);

#endif
