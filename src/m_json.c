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
#include "doomtype.h"
#include "i_printf.h"
#include "m_misc.h"
#include "w_wad.h"
#include "z_zone.h"

#include "cjson/cJSON.h"

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

json_t *JS_Open(const char *lump, const char *type, version_t maxversion)
{
    int lumpnum = W_CheckNumForName(lump);
    if (lumpnum < 0)
    {
        return NULL;
    }

    json_t *json = cJSON_ParseWithLength(W_CacheLumpNum(lumpnum, PU_CACHE), W_LumpLength(lumpnum));
    if (json == NULL)
    {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr)
        {
            I_Printf(VB_ERROR, "%s: parsing error before %s", lump, error_ptr);
        }
        return NULL;
    }

    json_t *js_type = JS_GetObject(json, "type");
    if (!JS_IsString(js_type))
    {
        I_Printf(VB_ERROR, "%s: no type string", lump);
        return NULL;
    }

    const char *s = JS_GetString(js_type);
    if (strcmp(s, type))
    {
        I_Printf(VB_ERROR, "%s: wrong type %s", lump, s);
        return NULL;
    }

    version_t v = {0};
    if (!JS_GetVersion(json, &v))
    {
        I_Printf(VB_ERROR, "%s: no version string", lump);
        return NULL;
    }

    if ((maxversion.major < v.major
         || (maxversion.major <= v.major && maxversion.minor < v.minor)
         || (maxversion.major <= v.major && maxversion.minor <= v.minor
             && maxversion.revision < v.revision)))
    {
        I_Printf(VB_ERROR, "%s: max supported version %d.%d.%d", lump,
                 maxversion.major, maxversion.minor, maxversion.revision);
        return NULL;
    }

    return json;
}

void JS_Close(json_t *json)
{
    cJSON_Delete(json);
}

boolean JS_IsObject(json_t *json)
{
    return cJSON_IsObject(json);
}

boolean JS_IsNull(json_t *json)
{
    return cJSON_IsNull(json);
}

boolean JS_IsBoolean(json_t *json)
{
    return cJSON_IsBool(json);
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

boolean JS_GetBoolean(json_t *json)
{
    return !!json->valueint;
}

double JS_GetNumber(json_t *json)
{
    return json->valuedouble;
}

double JS_GetNumberValue(json_t *json, const char *string)
{
    json_t *obj = JS_GetObject(json, string);
    if (JS_IsNumber(obj))
    {
        return obj->valuedouble;
    }
    return 0;
}

int JS_GetInteger(json_t *json)
{
    return json->valueint;
}

int JS_GetIntegerValue(json_t *json, const char *string)
{
    json_t *obj = JS_GetObject(json, string);
    if (JS_IsNumber(obj))
    {
        return obj->valueint;
    }
    return 0;
}

const char *JS_GetString(json_t *json)
{
    return json->valuestring;
}

const char *JS_GetStringValue(json_t *json, const char *string)
{
    json_t *obj = JS_GetObject(json, string);
    if (JS_IsString(obj))
    {
        return obj->valuestring;
    }
    return NULL;
}
