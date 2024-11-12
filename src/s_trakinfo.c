//
// Copyright(C) 2024 Roman Fomin
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

#include "s_trakinfo.h"

#include <stdio.h>
#include <string.h>

#include "m_array.h"
#include "m_json.h"
#include "m_misc.h"
#include "sha1.h"

boolean trakinfo_found;

extra_music_t extra_music;

typedef struct
{
    sha1_digest_t sha1key;
    char remix[9];
    char midi[9];
} trakinfo_t;

static trakinfo_t *trakinfo;

void S_ParseTrakInfo(int lumpnum)
{
    json_t *json = JS_OpenOptions(lumpnum, true);

    json_obj_iter_t *iter = JS_ObjectIterator(json);

    json_t *key = NULL, *value = NULL;
    while (JS_ObjectNext(iter, &key, &value))
    {
        trakinfo_t trak = {0};
        const char *string = JS_GetString(key);
        for (int offset = 0; offset < sizeof(sha1_digest_t); ++offset)
        {
            unsigned int i;
            sscanf(string + 2 * offset, "%02x", &i);
            trak.sha1key[offset] = i;
        }
        const char *remix = JS_GetStringValue(value, "Remixed");
        if (remix)
        {
            M_CopyLumpName(trak.remix, remix);
        }
        const char *midi = JS_GetStringValue(value, "MIDI");
        if (midi)
        {
            M_CopyLumpName(trak.midi, midi);
        }
        array_push(trakinfo, trak);
    }

    JS_CloseOptions(lumpnum);
    trakinfo_found = true;
}

char *S_GetRemix(byte *data, int length)
{
    sha1_context_t sha1_context;
    sha1_digest_t digest;

    SHA1_Init(&sha1_context);
    SHA1_Update(&sha1_context, data, length);
    SHA1_Final(digest, &sha1_context);

    trakinfo_t *trak;
    array_foreach(trak, trakinfo)
    {
        if (!memcmp(trak->sha1key, digest, sizeof(sha1_digest_t)))
        {
            if (extra_music == EXMUS_REMIX)
            {
                return trak->remix;
            }
            else
            {
                return trak->midi;
            }
        }
    }
    return NULL;
}
