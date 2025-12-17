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
#include "w_wad.h"

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
    DEH_MAPPING_STRING("Tranmap", tranmap)
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
        ivalue = DEH_BexParseBitFlags(ivalue, value, frame_flags_mbf21, arrlen(frame_flags_mbf21));
    }
    else if (!strcasecmp(variable_name, "Tranmap"))
    {
        state->tranmap = W_CacheLumpName(value, PU_STATIC);
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
