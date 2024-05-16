//
// Copyright(C) 2022 Ryan Krafnick
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
//      Command History HUD Widget
//

#ifndef __HU_COMMAND__
#define __HU_COMMAND__

#include "doomtype.h"
#include "hu_lib.h"

struct hu_multiline_s;
struct ticcmd_s;

extern boolean hud_command_history;
extern int hud_command_history_size;
extern boolean hud_hide_empty_commands;

void HU_UpdateTurnFormat(void);
void HU_InitCommandHistory(void);
void HU_ResetCommandHistory(void);
void HU_UpdateCommandHistory(const struct ticcmd_s *cmd);
void HU_BuildCommandHistory(struct hu_multiline_s *const multiline);

#endif
