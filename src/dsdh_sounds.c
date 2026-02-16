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

#include "m_array.h"
#include "m_hashmap.h"
#include "m_misc.h"
#include "sounds.h"
#include <string.h>

sfxinfo_t *S_sfx = NULL;
int num_sfx;

static int max_sfx_number;

static hashmap_t *translate;

void DSDH_SoundsInit(void)
{
    num_sfx = NUMSFX;
    max_sfx_number = NUMSFX - 1;

    array_resize(S_sfx, NUMSFX);
    memcpy(S_sfx, original_S_sfx, NUMSFX * sizeof(*S_sfx));
    for (int i = 0; i < NUMSFX; ++i)
    {
        if (original_S_sfx[i].name)
        {
            S_sfx[i].name = M_StringDuplicate(original_S_sfx[i].name);
        }
    }
}

int DSDH_SoundTranslate(int sfx_number)
{
    max_sfx_number = MAX(max_sfx_number, sfx_number);

    if (sfx_number < NUMSFX)
    {
        return sfx_number;
    }

    if (!translate)
    {
        translate = hashmap_init(1024, sizeof(int));
    }

    int *index = hashmap_get(translate, sfx_number);
    if (index)
    {
        return *index;
    }

    int new_index = num_sfx;
    hashmap_put(translate, sfx_number, &new_index);

    sfxinfo_t sfx = {.priority = 127, .lumpnum = -1};
    array_push(S_sfx, sfx);
    ++num_sfx;

    return new_index;
}

int DSDH_SoundsGetNewIndex(void)
{
    ++max_sfx_number;
    return DSDH_SoundTranslate(max_sfx_number);
}
