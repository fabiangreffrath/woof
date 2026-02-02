//
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2014 Fabian Greffrath
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
// Parses [SPRITES] sections in BEX files
//

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "deh_defs.h"
#include "deh_io.h"
#include "deh_main.h"
#include "dsdh_main.h"
#include "info.h"
#include "m_misc.h"

static int SpritesGetIndex(const char *key)
{
    for (int i = 0; i < NUMSPRITES; ++i)
    {
        if (original_sprnames[i]
            && !strncasecmp(original_sprnames[i], key, 4))
        {
            return i;
        }
    }

    // is it a number?
    for (const char *c = key; *c; c++)
    {
        if (!isdigit(*c))
        {
            return -1;
        }
    }

    int i = atoi(key);
    i = DSDH_SpriteTranslate(i);

    return i;
}

static int DEH_BEXSpritesStart(deh_context_t *context, char *line)
{
    char s[10];

    if (sscanf(line, "%9s", s) == 0 || strcmp("[SPRITES]", s))
    {
        DEH_Warning(context, "Parse error on section start");
    }

    return 0;
}

static void DEH_BEXSpritesParseLine(deh_context_t *context, char *line, int tag)
{
    char *sprite_key, *sprite_name;

    if (!DEH_ParseAssignment(line, &sprite_key, &sprite_name))
    {
        DEH_Warning(context, "Failed to parse sound assignment");
        return;
    }

    const int len = strlen(sprite_name);
    if (len != 4)
    {
        DEH_Warning(context, "Invalid sprite string length");
        return;
    }

    const int match = SpritesGetIndex(sprite_key);
    if (match >= 0)
    {
        if (sprnames[match])
        {
            free(sprnames[match]);
        }
        sprnames[match] = M_StringDuplicate(sprite_name);
    }
}

deh_section_t deh_section_bex_sprites =
{
    "[SPRITES]",
    NULL,
    DEH_BEXSpritesStart,
    DEH_BEXSpritesParseLine,
    NULL,
    NULL,
};
