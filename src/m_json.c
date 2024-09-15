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

#include "cjson/cJSON.h"

json_t *JS_Open(const char *data)
{
    return cJSON_Parse(data);
}

void JS_Close(json_t *json)
{
    cJSON_Delete(json);
}

boolean JS_IsNull(json_t *json)
{
    return cJSON_IsNull(json);
}

boolean JS_IsNumber(json_t *json)
{
    return cJSON_IsNumber(json);
}

boolean JS_IsString(json_t *json)
{
    return cJSON_IsString(json);
}

boolean JS_IsArray(json_t *json)
{
    return cJSON_IsArray(json);
}

json_t *JS_GetObject(json_t *json, const char *string)
{
    return cJSON_GetObjectItemCaseSensitive(json, string);
}

int JS_GetArraySize(json_t *json)
{
    return cJSON_GetArraySize(json);
}

json_t *JS_GetArrayItem(json_t *json, int index)
{
    return cJSON_GetArrayItem(json, index);
}

double JS_GetNumber(json_t *json)
{
    return json->valuedouble;
}

int JS_GetInteger(json_t *json)
{
    return json->valueint;
}

const char *JS_GetString(json_t *json)
{
    return json->valuestring;
}
