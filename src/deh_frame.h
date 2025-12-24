//
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2021 Roman Fomin
// Copyright(C) 2025 Guilherme Miranda
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
//
// Parses "Frame" sections in dehacked files
//

#ifndef DEH_FRAME_H
#define DEH_FRAME_H

#include "doomtype.h"
#include "info.h"

extern byte *defined_codepointer_args;
extern union actionf_u *deh_codepointer;
extern statenum_t *seenstate_tab;

extern void DEH_InitStates(void);
extern void DEH_FreeStates(void);

extern void DEH_StatesEnsureCapacity(int limit);
extern void DEH_ValidateStateArgs(void);

#endif
