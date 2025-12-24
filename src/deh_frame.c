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

#include <stdio.h>
#include <stdlib.h>

#include "deh_defs.h"
#include "deh_io.h"
#include "deh_main.h"
#include "deh_mapping.h"
#include "info.h"
#include "m_array.h"
#include "w_wad.h"

//
// DSDHacked states
//

state_t *states = NULL;
int num_states;
byte *defined_codepointer_args = NULL;
statenum_t *seenstate_tab = NULL;
actionf_t *deh_codepointer = NULL;

void DEH_InitStates(void)
{
    states = original_states;
    num_states = NUMSTATES;

    array_grow(seenstate_tab, num_states);
    memset(seenstate_tab, 0, num_states * sizeof(*seenstate_tab));

    array_grow(deh_codepointer, num_states);
    for (int i = 0; i < num_states; i++)
    {
        deh_codepointer[i] = states[i].action;
    }

    array_grow(defined_codepointer_args, num_states);
    memset(defined_codepointer_args, 0, num_states * sizeof(*defined_codepointer_args));
}

void DEH_FreeStates(void)
{
    array_free(defined_codepointer_args);
    array_free(deh_codepointer);
}

void DEH_StatesEnsureCapacity(int limit)
{
    if (limit < num_states)
    {
        return;
    }

    const int old_num_states = num_states;

    static boolean first_allocation = true;
    if (first_allocation)
    {
        states = NULL;
        array_grow(states, old_num_states + limit);
        memcpy(states, original_states, old_num_states * sizeof(*states));
        first_allocation = false;
    }
    else
    {
        array_grow(states, limit);
    }

    num_states = array_capacity(states);
    const int size_delta = num_states - old_num_states;
    memset(states + old_num_states, 0, size_delta * sizeof(*states));

    array_grow(deh_codepointer, size_delta);
    memset(deh_codepointer + old_num_states, 0, size_delta * sizeof(*deh_codepointer));

    array_grow(defined_codepointer_args, size_delta);
    memset(defined_codepointer_args + old_num_states, 0, size_delta * sizeof(*defined_codepointer_args));

    array_grow(seenstate_tab, size_delta);
    memset(seenstate_tab + old_num_states, 0, size_delta * sizeof(*seenstate_tab));

    for (int i = old_num_states; i < num_states; ++i)
    {
        states[i].sprite = SPR_TNT1;
        states[i].tics = -1;
        states[i].nextstate = i;
    }
}

//
// BEX flag mnemonics with matching bit flags
//

const bex_bitflags_t frame_flags_mbf21[] =
{
    {"SKILL5FAST", STATEF_SKILL5FAST}
};

DEH_BEGIN_MAPPING(state_mapping, state_t)
    DEH_MAPPING("Sprite number", sprite)
    DEH_MAPPING("Sprite subnumber", frame)
    DEH_MAPPING("Duration", tics)
    DEH_MAPPING("Next frame", nextstate)
    DEH_MAPPING("Unknown 1", misc1)
    DEH_MAPPING("Unknown 2", misc2)
    DEH_UNSUPPORTED_MAPPING("Codep frame")
    // mbf21
    DEH_MAPPING("MBF21 Bits", flags)
    DEH_MAPPING("Args1", args[0])
    DEH_MAPPING("Args2", args[1])
    DEH_MAPPING("Args3", args[2])
    DEH_MAPPING("Args4", args[3])
    DEH_MAPPING("Args5", args[4])
    DEH_MAPPING("Args6", args[5])
    DEH_MAPPING("Args7", args[6])
    DEH_MAPPING("Args8", args[7])
    // id24
    DEH_MAPPING("Tranmap", tranmap)
    // mbf2y
    DEH_UNSUPPORTED_MAPPING("MBF2y Bits")
    DEH_UNSUPPORTED_MAPPING("Min brightness")
DEH_END_MAPPING

static void *DEH_FrameStart(deh_context_t *context, char *line)
{
    int frame_number = -1;

    if (sscanf(line, "Frame %i", &frame_number) != 1)
    {
        DEH_Warning(context, "Parse error on section start");
        return NULL;
    }

    if (frame_number < 0)
    {
        DEH_Warning(context, "Invalid frame number: %i", frame_number);
        return NULL;
    }

    // DSDHacked
    DEH_StatesEnsureCapacity(frame_number);

    return &states[frame_number];
}

static void DEH_FrameParseLine(deh_context_t *context, char *line, void *tag)
{
    if (tag == NULL)
    {
        return;
    }

    state_t *state = (state_t *)tag;

    // Parse the assignment
    char *variable_name, *value;
    if (!DEH_ParseAssignment(line, &variable_name, &value))
    {
        DEH_Warning(context, "Failed to parse assignment");
        return;
    }

    // most values are integers
    int ivalue = atoi(value);

    if (!strcasecmp(variable_name, "MBF21 Bits"))
    {
        ivalue = DEH_ParseBexBitFlags(ivalue, value, frame_flags_mbf21, arrlen(frame_flags_mbf21));
    }
    else if (!strcasecmp(variable_name, "Tranmap"))
    {
        state->tranmap = W_CacheLumpName(value, PU_STATIC);
        return;
    }
    else if (!strcasecmp(variable_name, "Args1"))
    {
        defined_codepointer_args[state - states] |= (1u << 0);
    }
    else if (!strcasecmp(variable_name, "Args2"))
    {
        defined_codepointer_args[state - states] |= (1u << 1);
    }
    else if (!strcasecmp(variable_name, "Args3"))
    {
        defined_codepointer_args[state - states] |= (1u << 2);
    }
    else if (!strcasecmp(variable_name, "Args4"))
    {
        defined_codepointer_args[state - states] |= (1u << 3);
    }
    else if (!strcasecmp(variable_name, "Args5"))
    {
        defined_codepointer_args[state - states] |= (1u << 4);
    }
    else if (!strcasecmp(variable_name, "Args6"))
    {
        defined_codepointer_args[state - states] |= (1u << 5);
    }
    else if (!strcasecmp(variable_name, "Args7"))
    {
        defined_codepointer_args[state - states] |= (1u << 6);
    }
    else if (!strcasecmp(variable_name, "Args8"))
    {
        defined_codepointer_args[state - states] |= (1u << 7);
    }

    // set the appropriate field
    DEH_SetMapping(context, &state_mapping, state, variable_name, ivalue);
}

static void DEH_FrameSHA1Sum(sha1_context_t *context)
{
    for (int i = 0; i < num_states; ++i)
    {
        DEH_StructSHA1Sum(context, &state_mapping, &states[i]);
    }
}

deh_section_t deh_section_frame =
{
    "Frame",
    NULL,
    DEH_FrameStart,
    DEH_FrameParseLine,
    NULL,
    DEH_FrameSHA1Sum,
};
