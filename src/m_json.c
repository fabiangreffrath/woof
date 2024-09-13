
#include "m_json.h"

#include "cjson/cJSON.h"

json_t *JS_Parse(const char *data)
{
    return cJSON_Parse(data);
}

void JS_Free(json_t *json)
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
