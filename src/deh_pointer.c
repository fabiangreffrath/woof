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

#include "deh_defs.h"
#include "deh_io.h"
#include "deh_main.h"
#include "dsdh_main.h"
#include "info.h"

static int DEH_PointerStart(deh_context_t *context, char *line)
{
    // FIXME: can the third argument here be something other than "Frame"
    // or are we ok?

    int frame_number = -1;
    if (sscanf(line, "Pointer %*i (%*s %i)", &frame_number) != 1)
    {
        DEH_Warning(context, "Parse error on section start");
        return -1;
    }

    if (frame_number < 0)
    {
        DEH_Warning(context, "Invalid frame number: %i", frame_number);
        return -1;
    }

    // DSDhacked
    frame_number = DSDH_StateTranslate(frame_number);

    return frame_number;
}

static void DEH_PointerParseLine(deh_context_t *context, char *line, int tag)
{
    if (tag == -1)
    {
        return;
    }

    int frame_number = tag;

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
        ivalue = DSDH_StateTranslate(ivalue);

        if (ivalue < NUMSTATES)
        {
            states[frame_number].action = original_states[ivalue].action;
        }
    }
    else
    {
        DEH_Warning(context, "Unknown variable name '%s'", variable_name);
    }
}

deh_section_t deh_section_pointer =
{
    "Pointer",
    NULL,
    DEH_PointerStart,
    DEH_PointerParseLine,
    NULL,
    NULL,
};
