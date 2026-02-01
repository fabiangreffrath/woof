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

#include "d_think.h"
#include "deh_defs.h"
#include "deh_io.h"
#include "deh_main.h"
#include "deh_mapping.h"
#include "info.h"
#include "m_array.h"
#include "m_hashmap.h"
#include "m_misc.h"
#include "w_wad.h"

//
// DSDHacked states
//

state_t *states = NULL;
int num_states;
byte *defined_codepointer_args = NULL;
actionf_t *deh_codepointer = NULL;

static hashmap_t *translate;

void DEH_InitStates(void)
{
    states = original_states;
    num_states = NUMSTATES;

    array_resize(deh_codepointer, num_states);
    for (int i = 0; i < num_states; i++)
    {
        deh_codepointer[i] = states[i].action;
    }

    array_resize(defined_codepointer_args, num_states);
}

void DEH_FreeStates(void)
{
    array_free(deh_codepointer);
    array_free(defined_codepointer_args);
    if (translate)
    {
        hashmap_free(translate);
    }
}

int DEH_FrameTranslate(int frame_number)
{
    if (frame_number < NUMSTATES)
    {
        return frame_number;
    }

    if (!translate)
    {
        translate = hashmap_init(2048);

        states = NULL;
        array_resize(states, num_states);
        memcpy(states, original_states, num_states * sizeof(state_t));
    }

    int index;
    if (hashmap_get(translate, frame_number, &index))
    {
        return index;
    }

    index = num_states;
    hashmap_put(translate, frame_number, &index);

    state_t state = {.sprite = SPR_TNT1, .tics = -1, .nextstate = index};
    array_push(states, state);
    array_push(deh_codepointer, state.action);
    array_push(defined_codepointer_args, 0);

    ++num_states;

    return index;
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

static int DEH_FrameStart(deh_context_t *context, char *line)
{
    int frame_number = -1;

    if (sscanf(line, "Frame %i", &frame_number) != 1)
    {
        DEH_Warning(context, "Parse error on section start");
        return -1;
    }

    if (frame_number < 0)
    {
        DEH_Warning(context, "Invalid frame number: %i", frame_number);
        return -1;
    }

    // DSDHacked
    frame_number = DEH_FrameTranslate(frame_number);

    return frame_number;
}

static void DEH_FrameParseLine(deh_context_t *context, char *line, int tag)
{
    if (tag == -1)
    {
        return;
    }

    int frame_number = tag;

    state_t *state = &states[frame_number];

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
    else if (!strcasecmp(variable_name, "Next frame"))
    {
        ivalue = DEH_FrameTranslate(ivalue);
        state = &states[frame_number];
    }
    else
    {
        M_StringToLower(variable_name);
        int num;
        if (sscanf(variable_name, "args%i", &num) == 1)
        {
            if (num >= 1 && num <= 8)
            {
                --num;
                defined_codepointer_args[frame_number] |= (1u << num);
            }
        }
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
