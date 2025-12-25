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

#include "m_json.h"

#include <string.h>
#include "doomtype.h"
#include "i_printf.h"
#include "m_array.h"
#include "m_misc.h"
#include "w_wad.h"
#include "z_zone.h"

#include "yyjson.h"

boolean JS_GetVersion(json_t *json, version_t *version)
{
    json_t *js_version = JS_GetObject(json, "version");
    if (!JS_IsString(js_version))
    {
        return false;
    }

    const char *string = JS_GetString(js_version);
    if (sscanf(string, "%d.%d.%d", &version->major, &version->minor,
               &version->revision) == 3)
    {
        return true;
    }

    return false;
}

typedef struct
{
    yyjson_doc *json_doc;
    int lumpnum;
} doc_t;

static doc_t *docs;

json_t *JS_OpenOptions(int lumpnum, boolean comments)
{
    char *string = W_CacheLumpNum(lumpnum, PU_CACHE);
    int length = W_LumpLength(lumpnum);

    yyjson_read_flag flag = comments ? YYJSON_READ_ALLOW_COMMENTS : 0;

    yyjson_read_err err;
    yyjson_doc *json_doc = yyjson_read_opts(string, length, flag, NULL, &err);

    if (!json_doc)
    {
        size_t line, col, chr;
        yyjson_locate_pos(string, length, err.pos, &line, &col, &chr);
        char name[9] = {0};
        M_CopyLumpName(name, lumpinfo[lumpnum].name);
        I_Printf(VB_ERROR, "%s(%d:%d): read error: %s\n", name, (int)line,
                 (int)col, err.msg);
        return NULL;
    }

    array_push(docs, ((doc_t){json_doc, lumpnum}));

    json_t *json = yyjson_doc_get_root(json_doc);

    return json;
}

json_t *JS_Open(const char *lump, const char *type, version_t maxversion)
{
    int lumpnum = W_CheckNumForName(lump);
    if (lumpnum < 0)
    {
        return NULL;
    }

    json_t *json = JS_OpenOptions(lumpnum, false);
    if (!json)
    {
        return NULL;
    }

    json_t *js_type = JS_GetObject(json, "type");
    if (!JS_IsString(js_type))
    {
        I_Printf(VB_ERROR, "%s: no type string", lump);
        JS_CloseOptions(lumpnum);
        return NULL;
    }

    const char *s = JS_GetString(js_type);
    if (strcmp(s, type))
    {
        I_Printf(VB_ERROR, "%s: wrong type %s", lump, s);
        JS_CloseOptions(lumpnum);
        return NULL;
    }

    version_t v = {0};
    if (!JS_GetVersion(json, &v))
    {
        I_Printf(VB_ERROR, "%s: no version string", lump);
        JS_CloseOptions(lumpnum);
        return NULL;
    }

    if ((maxversion.major < v.major
         || (maxversion.major <= v.major && maxversion.minor < v.minor)
         || (maxversion.major <= v.major && maxversion.minor <= v.minor
             && maxversion.revision < v.revision)))
    {
        I_Printf(VB_ERROR, "%s: max supported version %d.%d.%d", lump,
                 maxversion.major, maxversion.minor, maxversion.revision);
        JS_CloseOptions(lumpnum);
        return NULL;
    }

    return json;
}

void JS_CloseOptions(int lumpnum)
{
    for (int i = 0; i < array_size(docs); ++i)
    {
        if (docs[i].lumpnum == lumpnum)
        {
            yyjson_doc_free(docs[i].json_doc);
            array_delete(docs, i);
            break;
        }
    }
}

void JS_Close(const char *lump)
{
    int lumpnum = W_CheckNumForName(lump);
    if (lumpnum < 0)
    {
        return;
    }

    JS_CloseOptions(lumpnum);
}

boolean JS_IsObject(json_t *json)
{
    return yyjson_is_obj(json);
}

boolean JS_IsNull(json_t *json)
{
    return yyjson_is_null(json);
}

boolean JS_IsBoolean(json_t *json)
{
    return yyjson_is_bool(json);
}

boolean JS_IsNumber(json_t *json)
{
    return yyjson_is_num(json);
}

boolean JS_IsString(json_t *json)
{
    return yyjson_is_str(json);
}

boolean JS_IsArray(json_t *json)
{
    return yyjson_is_arr(json);
}

json_t *JS_GetObject(json_t *json, const char *string)
{
    return yyjson_obj_get(json, string);
}

int JS_GetArraySize(json_t *json)
{
    return yyjson_arr_size(json);
}

json_t *JS_GetArrayItem(json_t *json, int index)
{
    return yyjson_arr_get(json, index);
}

json_arr_iter_t *JS_ArrayIterator(json_t *json)
{
    json_arr_iter_t *iter = malloc(sizeof(*iter));
    yyjson_arr_iter_init(json, iter);
    return iter;
}

json_t *JS_ArrayNext(json_arr_iter_t *iter)
{
    json_t *value = yyjson_arr_iter_next(iter);
    if (!value)
    {
        free(iter);
    }
    return value;
}

boolean JS_GetBoolean(json_t *json)
{
    return yyjson_get_bool(json);
}

boolean JS_GetBooleanValue(json_t *json, const char *string)
{
    json_t *obj = JS_GetObject(json, string);
    if (JS_IsBoolean(obj))
    {
        return JS_GetBoolean(obj);
    }
    return false;
}

double JS_GetNumber(json_t *json)
{
    return yyjson_get_num(json);
}

double JS_GetNumberValue(json_t *json, const char *string)
{
    json_t *obj = JS_GetObject(json, string);
    if (JS_IsNumber(obj))
    {
        return JS_GetNumber(obj);
    }
    return 0;
}

int JS_GetInteger(json_t *json)
{
    return yyjson_get_int(json);
}

int JS_GetIntegerValue(json_t *json, const char *string)
{
    json_t *obj = JS_GetObject(json, string);
    if (JS_IsNumber(obj))
    {
        return JS_GetInteger(obj);
    }
    return 0;
}

const char *JS_GetString(json_t *json)
{
    return yyjson_get_str(json);
}

const char *JS_GetStringValue(json_t *json, const char *string)
{
    json_t *obj = JS_GetObject(json, string);
    if (JS_IsString(obj))
    {
        return JS_GetString(obj);
    }
    return NULL;
}

json_obj_iter_t *JS_ObjectIterator(json_t *json)
{
    yyjson_obj_iter *iter = malloc(sizeof(*iter));
    if (yyjson_obj_iter_init(json, iter))
    {
        return iter;
    }
    free(iter);
    return NULL;
}

boolean JS_ObjectNext(json_obj_iter_t *iter, json_t **key, json_t **value)
{
    *key = yyjson_obj_iter_next(iter);
    if (*key == NULL)
    {
        free(iter);
        return false;
    }
    *value = yyjson_obj_iter_get_val(*key);
    return true;
}
