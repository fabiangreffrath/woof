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
// Parses Action Pointer entries in dehacked files
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "deh_defs.h"
#include "deh_frame.h"
#include "deh_io.h"
#include "deh_main.h"
#include "info.h"

static int CodePointerIndex(actionf_t *ptr)
{
    for (int i = 0; i < num_states; ++i)
    {
        if (!memcmp(&deh_codepointer[i], ptr, sizeof(actionf_t)))
        {
            return i;
        }
    }

    return -1;
}

static void *DEH_PointerStart(deh_context_t *context, char *line)
{
    // FIXME: can the third argument here be something other than "Frame"
    // or are we ok?

    int frame_number = -1;
    if (sscanf(line, "Pointer %*i (%*s %i)", &frame_number) != 1)
    {
        DEH_Warning(context, "Parse error on section start");
        return NULL;
    }

    if (frame_number < 0)
    {
        DEH_Warning(context, "Invalid frame number: %i", frame_number);
        return NULL;
    }

    // DSDhacked
    DEH_StatesEnsureCapacity(frame_number);

    return &states[frame_number];
}

static void DEH_PointerParseLine(deh_context_t *context, char *line, void *tag)
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

    // set the appropriate field
    if (!strcasecmp(variable_name, "Codep frame"))
    {
        // all values are integers
        int ivalue = atoi(value);
        if (ivalue < 0)
        {
            DEH_Warning(context, "Invalid state '%i'", ivalue);
        }

        // DSDHacked
        DEH_StatesEnsureCapacity(ivalue);

        state->action = deh_codepointer[ivalue];
    }
    else
    {
        DEH_Warning(context, "Unknown variable name '%s'", variable_name);
    }
}

static void DEH_PointerSHA1Sum(sha1_context_t *context)
{
    for (int i = 0; i < num_states; ++i)
    {
        SHA1_UpdateInt32(context, CodePointerIndex(&states[i].action));
    }
}

deh_section_t deh_section_pointer =
{
    "Pointer",
    NULL,
    DEH_PointerStart,
    DEH_PointerParseLine,
    NULL,
    DEH_PointerSHA1Sum,
};
