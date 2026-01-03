//
// Copyright(C) 2005-2014 Simon Howard
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
// Parses "Cheat" sections in dehacked files
//

#include <stdlib.h>
#include <string.h>

#include "deh_defs.h"
#include "deh_io.h"
#include "deh_main.h"
#include "m_cheat.h"
#include "m_misc.h"

cheat_sequence_t *FindCheatByName(char *name)
{
    cheat_sequence_t *c = &cheats_table[0];
    while (c != NULL)
    {
        if (c->deh_cheat != NULL && !strcasecmp(c->deh_cheat, name))
        {
            return c;
        }
        c++;
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

    cheat_sequence_t *c = FindCheatByName(variable_name);
    if (c == NULL)
    {
        DEH_Warning(context, "Unknown cheat '%s'", variable_name);
        return;
    }

    int i = 0;
    unsigned char *unsvalue = (unsigned char *)value;
    char *placeholder = malloc(sizeof(char) * strlen(value));
    while (unsvalue[i] != 0 && unsvalue[i] != 0xff)
    {
        if (deh_apply_cheats)
        {
            placeholder[i] = unsvalue[i];
        }
        ++i;

        // Absolute limit - don't exceed
        if (i >= MAX_CHEAT_LEN - c->arg)
        {
            DEH_Error(context, "Cheat sequence too long!");
            free(placeholder);
            return;
        }
    }

    if (deh_apply_cheats)
    {
        placeholder[i] = '\0';
        c->sequence = M_StringDuplicate(placeholder);
        c->sequence_len = strlen(placeholder);
    }
    free(placeholder);
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
