//
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2014 Fabian Greffrath
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
// Parses [HELPER] sections in BEX files
//

#include <stdio.h>
#include <string.h>

#include "deh_defs.h"
#include "deh_io.h"
#include "deh_main.h"
#include "doomstat.h"

static void *DEH_BEXHelperStart(deh_context_t *context, char *line)
{
    char s[9];

    if (sscanf(line, "%8s", s) == 0 || strcmp("[HELPER]", s))
    {
        DEH_Warning(context, "Parse error on section start");
    }

    return NULL;
}

static void DEH_BEXHelperParseLine(deh_context_t *context, char *line, void *tag)
{
    char *type, *value;

    if (!DEH_ParseAssignment(line, &type, &value))
    {
        DEH_Warning(context, "Failed to parse helper assignment");
        return;
    }

    if (strcasecmp(type, "type") != 0)
    {
        DEH_Warning(context, "Unknown helper property %s", type);
        return;
    }

    int index = atoi(type);
    if (index >= 0)
    {
        helper_type = index;
    }
    else
    {
        DEH_Warning(context, "Unknown helper replacemnt %s for %s", value, type);
    }
}

deh_section_t deh_section_bex_helper =
{
    "[HELPER]",
    NULL,
    DEH_BEXHelperStart,
    DEH_BEXHelperParseLine,
    NULL,
    NULL,
};
