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
// Parses "Cheat" sections in dehacked files
//

#include <stdlib.h>
#include <string.h>

#include "deh_defs.h"
#include "deh_io.h"
#include "deh_main.h"
#include "m_cheat.h"

extern cheat_sequence_t cheat_seq[];

cheat_sequence_t *FindCheatByName(char *name)
{
    for (size_t i = 0; i < arrlen(cheat_seq); ++i)
    {
        if (!strcasecmp(cheat_seq[i].deh_cheat, name))
        {
            return &cheat_seq[i];
        }
    }

    return NULL;
}

static void *DEH_CheatStart(deh_context_t *context, char *line)
{
    return NULL;
}

static void DEH_CheatParseLine(deh_context_t *context, char *line, void *tag)
{
    char *variable_name, *value;
    if (!DEH_ParseAssignment(line, &variable_name, &value))
    {
        // Failed to parse
        DEH_Warning(context, "Failed to parse assignment");
        return;
    }

    cheat_sequence_t *cheat_seq = FindCheatByName(variable_name);
    if (cheat_seq == NULL)
    {
        DEH_Warning(context, "Unknown cheat '%s'", variable_name);
        return;
    }

    if (!deh_apply_cheats)
    {
        return;
    }

    int i = 0;
    unsigned char *unsvalue = (unsigned char*)value;
    while (unsvalue[i] != 0 && unsvalue[i] != 0xff)
    {
        cheat_seq->sequence[i] = unsvalue[i];
        ++i;
    }
    cheat_seq->sequence[i] = '\0';
}

deh_section_t deh_section_cheat =
{
    "Cheat",
    NULL,
    DEH_CheatStart,
    DEH_CheatParseLine,
    NULL,
    NULL,
};
