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

struct player_s;
struct ticcmd_s;

// Local View

extern void (*G_UpdateLocalView)(void);
void G_ClearLocalView(void);

// Side Movement

void G_UpdateSideMove(void);

// Error Accumulation

void G_UpdateCarry(void);
void G_ClearCarry(void);
void G_UpdateAngleFunctions(void);

// Gamepad

void G_UpdateDeltaTics(void);
short G_CalcControllerAngle(void);
int G_CalcControllerPitch(void);
int G_CalcControllerSideTurn(int speed);
int G_CalcControllerSideStrafe(int speed);
int G_CalcControllerForward(int speed);

// Mouse

void G_UpdateAccelerateMouse(void);
short G_CalcMouseAngle(void);
int G_CalcMousePitch(void);
int G_CalcMouseSide(void);
int G_CalcMouseVert(void);

// Composite Turn

void G_SavePlayerAngle(const struct player_s *player);
void G_AddToTicAngle(struct player_s *player);
void G_UpdateTicAngleTurn(struct ticcmd_s *cmd, int angle);

// Quickstart Cache

extern int quickstart_cache_tics;
extern boolean quickstart_queued;
extern float axis_turn_tic;
extern int mousex_tic;
void G_ClearQuickstartTic(void);
void G_ApplyQuickstartCache(struct ticcmd_s *cmd, boolean strafe);

#endif
