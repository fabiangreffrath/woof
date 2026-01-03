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
// Parses Text substitution sections in dehacked files
//

#include <stdio.h>
#include <stdlib.h>

#include "deh_defs.h"
#include "deh_io.h"
#include "deh_strings.h"
#include "doomtype.h"

// [crispy] support INCLUDE NOTEXT directive in BEX files
boolean bex_notext = false;

static void *DEH_TextStart(deh_context_t *context, char *line)
{
    int from_len, to_len;

    if (sscanf(line, "Text %i %i", &from_len, &to_len) != 2)
    {
        DEH_Warning(context, "Parse error on section start");
        return NULL;
    }

    // Skip text section
    if (bex_notext)
    {
        for (int i = 0; i < from_len; i++)
        {
            DEH_GetChar(context);
        }
        for (int i = 0; i < to_len; i++)
        {
            DEH_GetChar(context);
        }
        return NULL;
    }

    char *from_text = malloc(from_len + 1);
    char *to_text = malloc(to_len + 1);

    // read in the "from" text
    for (int i = 0; i < from_len; ++i)
    {
        from_text[i] = DEH_GetChar(context);
    }
    from_text[from_len] = '\0';

    // read in the "to" text
    for (int i = 0; i < to_len; ++i)
    {
        to_text[i] = DEH_GetChar(context);
    }
    to_text[to_len] = '\0';

    DEH_AddStringReplacement(from_text, to_text);

    free(from_text);
    free(to_text);

    return NULL;
}

static void DEH_TextParseLine(deh_context_t *context, char *line, void *tag)
{
    // not used
}

deh_section_t deh_section_text =
{
    "Text",
    NULL,
    DEH_TextStart,
    DEH_TextParseLine,
    NULL,
    NULL,
};
