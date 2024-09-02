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
//      Game Input Utility Functions
//

#ifndef __G_INPUT__
#define __G_INPUT__

#include "doomtype.h"

// Side Movement

void G_UpdateSideMove(void);

// Error Accumulation

void G_UpdateCarry(void);
void G_ClearCarry(void);
extern short (*G_CarryAngleTic)(double angle);
extern short (*G_CarryAngle)(double angle);
void G_UpdateAngleFunctions(void);
short G_CarryPitch(double pitch);
int G_CarrySide(double side);
int G_CarryVert(double vert);

// Gamepad

void G_UpdateGamepadVariables(void);
void G_UpdateDeltaTics(uint64_t delta_time);
extern double (*G_CalcGamepadAngle)(void);
double G_CalcGamepadPitch(void);
int G_CalcGamepadSideTurn(int speed);
int G_CalcGamepadSideStrafe(int speed);
int G_CalcGamepadForward(int speed);

// Mouse

void G_UpdateMouseVariables(void);
double G_CalcMouseAngle(int mousex);
double G_CalcMousePitch(int mousey);
double G_CalcMouseSide(int mousex);
double G_CalcMouseVert(int mousey);
void G_BindMouseVariables(void);

#endif
