//
// Copyright(C) 2026 Roman Fomin
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

#include "info.h"
#include "m_array.h"
#include "m_hashmap.h"
#include "m_misc.h"

char **sprnames = NULL;
int num_sprites;

static hashmap_t *translate;

void DSDH_SpritesInit(void)
{
    num_sprites = NUMSPRITES;

    array_resize(sprnames, NUMSPRITES);
    for (int i = 0; i < NUMSPRITES; ++i)
    {
        sprnames[i] = M_StringDuplicate(original_sprnames[i]);
    }
}

int DSDH_SpriteTranslate(int sprite_number)
{
    if (sprite_number < NUMSPRITES)
    {
        return sprite_number;
    }

    if (!translate)
    {
        translate = hashmap_init(1024);
    }

    int index;
    if (hashmap_get(translate, sprite_number, &index))
    {
        return index;
    }

    index = num_sprites;
    hashmap_put(translate, sprite_number, &index);

    array_push(sprnames, NULL);
    ++num_sprites;

    return index;
}
