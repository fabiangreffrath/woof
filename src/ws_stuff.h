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
//      Weapon slots.
//

#ifndef __WS_STUFF__
#define __WS_STUFF__

#include "doomdef.h"
#include "doomtype.h"

struct event_s;

#define NUM_WS_SLOTS 4
#define NUM_WS_WEAPS 3

typedef enum
{
    WS_SELECT_DPAD,
    WS_SELECT_FACE_BUTTONS,
    WS_SELECT_1234,
    NUM_WS_SELECT
} weapon_slots_selection_t;

void WS_Reset(void);
boolean WS_Enabled(void);
weapon_slots_selection_t WS_Selection(void);
void WS_UpdateSelection(void);
void WS_UpdateSlots(void);
void WS_Init(void);

void WS_UpdateState(const struct event_s *ev);
boolean WS_Override(void);
boolean WS_HoldOverride(void);

void WS_UpdateStateTic(void);
boolean WS_Responder(const struct event_s *ev);

boolean WS_SlotSelected(void);
weapontype_t WS_SlotWeapon(void);

void WS_BindVariables(void);

#endif
