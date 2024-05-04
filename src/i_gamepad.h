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

void I_BindGamepadVariables(void);

#endif
