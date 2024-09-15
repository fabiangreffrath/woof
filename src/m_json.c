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

#include <stdio.h>
#include <string.h>
#include "i_printf.h"

#include "cjson/cJSON.h"

json_t *JS_Open(const char *type, version_t version, const char *data)
{
    const char *s;

    json_t *json = cJSON_Parse(data);
    if (json == NULL)
    {
        I_Printf(VB_ERROR, "%s: error before %s", type, cJSON_GetErrorPtr());
        return NULL;
    }

    json_t *js_type = JS_GetObject(json, "type");
    if (!JS_IsString(js_type))
    {
        I_Printf(VB_ERROR, "%s: no type string", type);
        return NULL;
    }

    s = JS_GetString(js_type);
    if (strcmp(s, type))
    {
        I_Printf(VB_ERROR, "%s: wrong type %s", type, s);
        return NULL;
    }

    json_t *js_version = JS_GetObject(json, "version");
    if (!JS_IsString(js_version))
    {
        I_Printf(VB_ERROR, "%s: no version string", type);
        return NULL;
    }

    s = JS_GetString(js_version);
    version_t v = {0};
    sscanf(s, "%d.%d.%d", &v.major, &v.minor, &v.revision);
    if (!(v.major == version.major && v.minor == version.minor
          && v.revision == version.revision))
    {
        I_Printf(VB_ERROR, "%s: unsupported version %d.%d.%d", type, v.major,
                 v.minor, v.revision);
        return NULL;
    }

    return json;
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
