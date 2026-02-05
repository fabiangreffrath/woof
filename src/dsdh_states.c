//
// Copyright(C) 2026 Roman Fomin
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

#include <string.h>

#include "info.h"
#include "m_array.h"
#include "m_hashmap.h"

state_t *states = NULL;
int num_states;

static hashmap_t *translate;

void DSDH_StatesInit(void)
{
    num_states = NUMSTATES;

    array_resize(states, NUMSTATES);
    memcpy(states, original_states, NUMSTATES * sizeof(*states));
}

int DSDH_StateTranslate(int frame_number)
{
    if (frame_number < NUMSTATES)
    {
        return frame_number;
    }

    if (!translate)
    {
        translate = hashmap_init(2048, sizeof(int));
    }

    int *index = hashmap_get(translate, frame_number);
    if (index)
    {
        return *index;
    }

    int new_index = num_states;
    hashmap_put(translate, frame_number, &new_index);

    state_t state = {.sprite = SPR_TNT1, .tics = -1, .nextstate = new_index};
    array_push(states, state);
    ++num_states;

    return new_index;
}
