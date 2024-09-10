//
// Copyright(C) 2023 Roman Fomin
// Copyright(C) 2023 ceski
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
//      OpenAL equalizer.
//

#ifndef __I_OALEQUALIZER__
#define __I_OALEQUALIZER__

#include "doomtype.h"

boolean I_OAL_EqualizerInitialized(void);
boolean I_OAL_CustomEqualizer(void);
void I_OAL_ShutdownEqualizer(void);
void I_OAL_InitEqualizer(void);
void I_OAL_SetEqualizer(void);
void I_OAL_EqualizerPreset(void);

void I_BindEqualizerVariables(void);

#endif
