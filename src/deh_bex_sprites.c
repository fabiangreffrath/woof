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
#include <string.h>

#include "deh_defs.h"
#include "deh_io.h"
#include "deh_main.h"
#include "info.h"
#include "m_array.h"

//
// DSDHacked Sprites
//

char **sprnames = NULL;
int num_sprites;
static char **deh_spritenames = NULL;
static byte *sprnames_state = NULL;

void DEH_InitSprites(void)
{
    sprnames = original_sprnames;
    num_sprites = NUMSPRITES;

    array_grow(deh_spritenames, num_sprites);
    for (int i = 0; i < num_sprites; i++)
    {
        deh_spritenames[i] = strdup(sprnames[i]);
    }

    array_grow(sprnames_state, num_sprites);
    memset(sprnames_state, 0, num_sprites * sizeof(*sprnames_state));
}

void DEH_FreeSprites(void)
{
    for (int i = 0; i < array_capacity(deh_spritenames); i++)
    {
        if (deh_spritenames[i])
        {
            free(deh_spritenames[i]);
        }
    }
    array_free(deh_spritenames);
    array_free(sprnames_state);
}

static void SpritesEnsureCapacity(int limit)
{
    if (limit < num_sprites)
    {
        return;
    }

    const int old_num_sprites = num_sprites;

    static boolean first_allocation = true;
    if (first_allocation)
    {
        sprnames = NULL;
        array_grow(sprnames, old_num_sprites + limit);
        memcpy(sprnames, original_sprnames, old_num_sprites * sizeof(*sprnames));
        first_allocation = false;
    }
    else
    {
        array_grow(sprnames, limit);
    }

    num_sprites = array_capacity(sprnames);
    const int size_delta = num_sprites - old_num_sprites;
    memset(sprnames + old_num_sprites, 0, size_delta * sizeof(*sprnames));

    array_grow(sprnames_state, size_delta);
    memset(sprnames_state + old_num_sprites, 0, size_delta * sizeof(*sprnames_state));
}

int DEH_SpritesGetIndex(const char *key)
{
    for (int i = 0; i < num_sprites; ++i)
    {
        if (sprnames[i]
            && !strncasecmp(sprnames[i], key, 4)
            && !sprnames_state[i])
        {
            sprnames_state[i] = true; // sprite has been edited
            return i;
        }
    }

    return -1;
}

int DEH_SpritesGetOriginalIndex(const char *key)
{
    for (int i = 0; i < array_capacity(deh_spritenames); ++i)
    {
        if (deh_spritenames[i] && !strncasecmp(deh_spritenames[i], key, 4))
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
    SpritesEnsureCapacity(i);

    return i;
}

//
// The actual parser
//

static void *DEH_BEXSpritesStart(deh_context_t *context, char *line)
{
    char s[10];

    if (sscanf(line, "%9s", s) == 0 || strcmp("[SPRITES]", s))
    {
        DEH_Warning(context, "Parse error on section start");
    }

    return NULL;
}

static void DEH_BEXSpritesParseLine(deh_context_t *context, char *line, void *tag)
{
    char *spritenum, *value;

    if (!DEH_ParseAssignment(line, &spritenum, &value))
    {
        DEH_Warning(context, "Failed to parse sound assignment");
        return;
    }

    const int len = strlen(value);
    if (len != 4)
    {
        DEH_Warning(context, "Invalid sprite string length");
        return;
    }

    const int match = DEH_SpritesGetOriginalIndex(spritenum);
    if (match >= 0)
    {
        sprnames[match] = strdup(value);
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
