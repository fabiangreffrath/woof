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

#ifndef __I_OALMUSIC__
#define __I_OALMUSIC__

#include "doomtype.h"

typedef uint32_t (*callback_func_t)(uint8_t *buffer, uint32_t buffer_samples);

boolean I_OAL_HookMusic(callback_func_t callback_func);
void I_OAL_SetGain(float gain);
void I_OAL_UpdatePlayer(void);

#endif
