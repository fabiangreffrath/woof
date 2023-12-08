//
// Copyright(C) 2023 Roman Fomin
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

#ifndef __I_INPUT__
#define __I_INPUT__

#include "SDL.h"

#include "doomtype.h"
#include "d_event.h"

void I_OpenController(int which);
void I_CloseController(int which);

void I_UpdateMouseAccel(void);
int64_t I_AccelerateMouse(int64_t val);
void I_ReadMouse(void);
void I_UpdateJoystick(void);

void I_DelayEvent(void);
void I_HandleJoystickEvent(SDL_Event *sdlevent);
void I_HandleKeyboardEvent(SDL_Event *sdlevent);
void I_HandleMouseEvent(SDL_Event *sdlevent);

#endif
