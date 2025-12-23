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
    int fromlen, tolen;

    if (sscanf(line, "Text %i %i", &fromlen, &tolen) != 2)
    {
        DEH_Warning(context, "Parse error on section start");
        return NULL;
    }

    // We do not care about any Text blocks outside of music and sprite names
    if (fromlen > 6 || tolen > 6)
    {
        DEH_Warning(context, "Unsupported text replacement, invalid string size.");
        return NULL;
    }

    if (!bex_notext)
    {
        char *from_text = malloc(fromlen + 1);
        char *to_text = malloc(tolen + 1);

        // read in the "from" text
        for (int i = 0; i < fromlen; ++i)
        {
            from_text[i] = DEH_GetChar(context);
        }
        from_text[fromlen] = '\0';

        // read in the "to" text
        for (int i = 0; i < tolen; ++i)
        {
            to_text[i] = DEH_GetChar(context);
        }
        to_text[tolen] = '\0';

        DEH_AddStringReplacement(from_text, to_text);

        free(from_text);
        free(to_text);
    }

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
