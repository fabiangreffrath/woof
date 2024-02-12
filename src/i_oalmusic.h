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

typedef int (*callback_func_t)(byte *buffer, int buffer_samples);

boolean I_OAL_HookMusic(callback_func_t callback_func);
void I_OAL_SetGain(float gain);

#endif
