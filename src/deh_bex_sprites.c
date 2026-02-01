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
#include "info.h"
#include "m_array.h"
#include "m_hashmap.h"
#include "m_misc.h"

//
// DSDHacked Sprites
//

char **sprnames = NULL;
int num_sprites;
static char **deh_spritenames = NULL;
static boolean *sprnames_state;

static hashmap_t *translate;

void DEH_InitSprites(void)
{
    sprnames = original_sprnames;
    num_sprites = NUMSPRITES;

    deh_spritenames = calloc(num_sprites, sizeof(*deh_spritenames));
    for (int i = 0; i < num_sprites; i++)
    {
        deh_spritenames[i] = M_StringDuplicate(sprnames[i]);
    }

    array_resize(sprnames_state, num_sprites);
}

void DEH_FreeSprites(void)
{
    for (int i = 0; i < NUMSPRITES; ++i)
    {
        if (deh_spritenames[i])
        {
            free(deh_spritenames[i]);
        }
    }
    free(deh_spritenames);
    array_free(sprnames_state);
}

int DEH_SpritesTranslate(int sprite_number)
{
    if (sprite_number < NUMSPRITES)
    {
        return sprite_number;
    }

    if (!translate)
    {
        translate = hashmap_init(1024);

        sprnames = NULL;
        array_resize(sprnames, NUMSPRITES);
        memcpy(sprnames, original_sprnames, NUMSPRITES * sizeof(*sprnames));
    }

    int index;
    if (hashmap_get(translate, sprite_number, &index))
    {
        return index;
    }

    index = num_sprites;
    hashmap_put(translate, sprite_number, &index);

    array_push(sprnames, NULL);
    array_push(sprnames_state, false);

    ++num_sprites;

    return index;
}

static int SpritesGetIndex(const char *key)
{
    for (int i = 0; i < NUMSPRITES; ++i)
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
    i = DEH_SpritesTranslate(i);

    return i;
}

//
// The actual parser
//

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
        if (sprnames_state[match])
        {
            free(sprnames[match]);
        }
        sprnames[match] = M_StringDuplicate(sprite_name);
        sprnames_state[match] = true;
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
