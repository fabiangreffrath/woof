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

boolean I_UseController(void);
void I_InitController(void);
void I_OpenController(int which);
void I_CloseController(int which);

extern double (*I_AccelerateMouse)(int val);
void I_UpdateAccelerateMouse(void);
void I_ReadMouse(void);
void I_UpdateJoystick(boolean axis_buttons);
void I_UpdateJoystickMenu(void);

void I_DelayEvent(void);
void I_HandleJoystickEvent(SDL_Event *sdlevent);
void I_HandleKeyboardEvent(SDL_Event *sdlevent);
void I_HandleMouseEvent(SDL_Event *sdlevent);

#endif
