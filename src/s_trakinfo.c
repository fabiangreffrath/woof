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

#include <stdio.h>
#include <string.h>

#include "m_array.h"
#include "m_json.h"
#include "m_misc.h"
#include "sha1.h"

typedef struct
{
    char sha1key[41];
    char remix[9];
} trakinfo_t;

static trakinfo_t *trakinfo;

boolean trakinfo_found;

void S_ParseTrakInfo(int lumpnum)
{
    json_doc_t *doc = JS_ReadDocNum(lumpnum);
    json_t *json = JS_GetRoot(doc);

    json_obj_iter_t *iter = JS_ObjectIterator(json);

    json_t *key = NULL, *value = NULL;
    while (JS_ObjectNext(iter, &key, &value))
    {
        trakinfo_t trak = {0};
        strcpy(trak.sha1key, JS_GetString(key));
        const char *remix = JS_GetStringValue(value, "Remixed");
        if (remix)
        {
            M_CopyLumpName(trak.remix, remix);
        }
        array_push(trakinfo, trak);
    }

    JS_FreeDoc(doc);
    trakinfo_found = true;
}

char *S_GetRemix(byte *data, int length)
{
    sha1_context_t sha1_context;
    sha1_digest_t digest;

    SHA1_Init(&sha1_context);
    SHA1_Update(&sha1_context, data, length);
    SHA1_Final(digest, &sha1_context);

    char result[41];
    for (int offset = 0; offset < sizeof(sha1_digest_t); ++offset)
    {
        sprintf((result + (2 * offset)), "%02x", digest[offset] & 0xff);
    }

    trakinfo_t *trak;
    array_foreach(trak, trakinfo)
    {
        if (!strcmp(trak->sha1key, result))
        {
            return trak->remix;
        }
    }
    return NULL;
}
