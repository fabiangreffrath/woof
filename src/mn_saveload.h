//
//  Copyright (C) 2026 Roman Fomin
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.

#ifndef MN_SAVELOAD_H
#define MN_SAVELOAD_H

#include "doomtype.h"
#include "mn_common.h"

#define SAVE_STRING_SIZE 24

struct event_s;

void MN_SaveLoadOpen(boolean save);
boolean MN_SaveLoadTextInput(const struct event_s *ev, menu_action_t action,
                             int ch);

void MN_QuickSave(void);
boolean MN_QuickSaveUsed(void);
void MN_LoadQuickSave(void);

void MN_SetDefaultSaveName(char *name, const char *append);

#endif
