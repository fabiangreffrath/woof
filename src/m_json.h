
#ifndef M_JSON_H
#define M_JSON_H

#include "doomtype.h"

typedef struct cJSON json_t;

json_t *JS_Parse(const char *data);
void JS_Free(json_t *json);

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
    for (int __index = 0; __index < JS_GetArraySize((array))                  \
                          && ((element) = JS_GetArrayItem((array), __index)); \
         ++__index)

#endif
