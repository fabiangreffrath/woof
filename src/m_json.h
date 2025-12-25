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

#ifndef M_JSON_H
#define M_JSON_H

#include "doomtype.h"

typedef struct yyjson_val json_t;

typedef struct yyjson_arr_iter json_arr_iter_t;

typedef struct yyjson_obj_iter json_obj_iter_t;

typedef struct
{
    int major;
    int minor;
    int revision;
} version_t;

boolean JS_GetVersion(json_t *json, version_t *version);

json_t *JS_Open(const char *lump, const char *type, version_t maxversion);
json_t *JS_OpenOptions(int lumpnum, boolean comments);
void JS_Close(const char *lump);
void JS_CloseOptions(int lumpnum);

boolean JS_IsObject(json_t *json);
boolean JS_IsNull(json_t *json);
boolean JS_IsBoolean(json_t *json);
boolean JS_IsNumber(json_t *json);
boolean JS_IsString(json_t *json);
boolean JS_IsArray(json_t *json);

json_t *JS_GetObject(json_t *json, const char *string);
boolean JS_GetBoolean(json_t *json);
boolean JS_GetBooleanValue(json_t *json, const char *string);
double JS_GetNumber(json_t *json);
double JS_GetNumberValue(json_t *json, const char *string);
int JS_GetInteger(json_t *json);
int JS_GetIntegerValue(json_t *json, const char *string);
const char *JS_GetString(json_t *json);
const char *JS_GetStringValue(json_t *json, const char *string);

int JS_GetArraySize(json_t *json);
json_t *JS_GetArrayItem(json_t *json, int index);
json_arr_iter_t *JS_ArrayIterator(json_t *json);
json_t *JS_ArrayNext(json_arr_iter_t *iter);

#define JS_ArrayForEach(element, array)                    \
    for (json_arr_iter_t *_iter = JS_ArrayIterator(array); \
         ((element) = JS_ArrayNext(_iter));)

json_obj_iter_t *JS_ObjectIterator(json_t *json);
boolean JS_ObjectNext(json_obj_iter_t *iter, json_t **key, json_t **value);

#endif
