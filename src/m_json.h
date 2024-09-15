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

typedef struct cJSON json_t;

typedef struct
{
    int major;
    int minor;
    int revision;
} version_t;

json_t *JS_Open(const char *type, version_t version, const char *data);
void JS_Close(json_t *json);

json_t *JS_GetObject(json_t *json, const char *string);

boolean JS_IsNull(json_t *json);
boolean JS_IsNumber(json_t *json);
boolean JS_IsString(json_t *json);
boolean JS_IsArray(json_t *json);

double JS_GetNumber(json_t *json);
int JS_GetInteger(json_t *json);
const char *JS_GetString(json_t *json);

int JS_GetArraySize(json_t *json);
json_t *JS_GetArrayItem(json_t *json, int index);

#define JS_ArrayForEach(element, array)                                       \
    for (int __index = 0, __size = JS_GetArraySize((array));                  \
         __index < __size && ((element) = JS_GetArrayItem((array), __index)); \
         ++__index)

#endif
