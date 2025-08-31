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
//      Gamepad rumble.
//

#ifndef __I_RUMBLE__
#define __I_RUMBLE__

#include <SDL3/SDL.h>

#include "doomtype.h"

struct mobj_s;
struct sfxinfo_s;

typedef enum
{
    RUMBLE_NONE,

    RUMBLE_ITEMUP,
    RUMBLE_WPNUP,
    RUMBLE_GETPOW,
    RUMBLE_OOF,
    RUMBLE_PAIN,
    RUMBLE_HITFLOOR,

    RUMBLE_PLAYER,
    RUMBLE_ORIGIN,

    RUMBLE_PISTOL,
    RUMBLE_SHOTGUN,
    RUMBLE_SSG,
    RUMBLE_CGUN,
    RUMBLE_ROCKET,
    RUMBLE_PLASMA,
    RUMBLE_BFG,
} rumble_type_t;

void I_ShutdownRumble(void);
void I_InitRumble(void);
void I_CacheRumble(struct sfxinfo_s *sfx, int format, const byte *data,
                   int size, int rate);

boolean I_RumbleEnabled(void);
boolean I_RumbleSupported(void);
void I_RumbleMenuFeedback(void);
void I_UpdateRumbleEnabled(void);
void I_SetRumbleSupported(SDL_Gamepad *gamepad);

void I_ResetRumbleChannel(int handle);
void I_ResetAllRumbleChannels(void);

void I_UpdateRumble(void);
void I_UpdateRumbleParams(const struct mobj_s *listener,
                          const struct mobj_s *origin, int handle);

void I_StartRumble(const struct mobj_s *listener, const struct mobj_s *origin,
                   const struct sfxinfo_s *sfx, int handle,
                   rumble_type_t rumble_type);

// For gyro calibration only.
void I_DisableRumble(void);

void I_BindRumbleVariables(void);

#endif
