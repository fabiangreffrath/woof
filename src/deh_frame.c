//
// Copyright(C) 2005-2014 Simon Howard
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

state_t *states = NULL;
int num_states;
byte *defined_codeptr_args = NULL;
statenum_t *seenstate_tab = NULL;
actionf_t *deh_codeptr = NULL;

void DEH_InitStates(void)
{
    states = original_states;
    num_states = NUMSTATES;

    array_grow(seenstate_tab, num_states);
    memset(seenstate_tab, 0, num_states * sizeof(*seenstate_tab));

    array_grow(deh_codeptr, num_states);
    for (int i = 0; i < num_states; i++)
    {
        deh_codeptr[i] = states[i].action;
    }

    array_grow(defined_codeptr_args, num_states);
    memset(defined_codeptr_args, 0, num_states * sizeof(*defined_codeptr_args));
}

void DEH_FreeStates(void)
{
    array_free(defined_codeptr_args);
    array_free(deh_codeptr);
}

void DEH_EnsureStatesCapacity(int limit)
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

    array_grow(deh_codeptr, size_delta);
    memset(deh_codeptr + old_num_states, 0, size_delta * sizeof(*deh_codeptr));

    array_grow(defined_codeptr_args, size_delta);
    memset(defined_codeptr_args + old_num_states, 0, size_delta * sizeof(*defined_codeptr_args));

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
    int frame_number = 0;

    if (sscanf(line, "Frame %i", &frame_number) != 1)
    {
        DEH_Warning(context, "Parse error on section start");
        return NULL;
    }

    if (frame_number < 0 || frame_number >= NUMSTATES)
    {
        DEH_Warning(context, "Invalid frame number: %i", frame_number);
        return NULL;
    }

    if (frame_number >= DEH_VANILLA_NUMSTATES)
    {
        DEH_Warning(context, "Attempt to modify frame %i: this will cause problems in Vanilla dehacked.", frame_number);
    }

    state_t *state = &states[frame_number];

    return state;
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

    // set the appropriate field
    DEH_SetMapping(context, &state_mapping, state, variable_name, ivalue);
}

static void DEH_FrameSHA1Sum(sha1_context_t *context)
{
    for (int i = 0; i < NUMSTATES; ++i)
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
