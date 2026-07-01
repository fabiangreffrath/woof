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
#include "m_misc.h"

typedef struct yyjson_val     json_t;
typedef struct yyjson_mut_val json_mut_t;
typedef struct yyjson_mut_doc json_mut_doc_t;
typedef struct yyjson_arr_iter json_arr_iter_t;
typedef struct yyjson_obj_iter json_obj_iter_t;

boolean JS_GetVersion(json_t *json, version_t *version);

json_t *JS_OpenString(char *string, size_t length);
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

// Write API

json_mut_doc_t *JS_NewDoc(void);
void            JS_FreeDoc(json_mut_doc_t *doc);

json_mut_t *JS_NewObject(json_mut_doc_t *doc);
json_mut_t *JS_NewArray(json_mut_doc_t *doc);
void        JS_SetRoot(json_mut_doc_t *doc, json_mut_t *root);

void JS_SetInt(json_mut_doc_t *doc, json_mut_t *obj,
               const char *key, int val);
void JS_SetObject(json_mut_doc_t *doc, json_mut_t *parent,
                  const char *key, json_mut_t *child);
void JS_SetArray(json_mut_doc_t *doc, json_mut_t *parent,
                 const char *key, json_mut_t *arr);

void JS_ArrayAddInt(json_mut_doc_t *doc, json_mut_t *arr, int val);
void JS_ArrayAddObject(json_mut_doc_t *doc, json_mut_t *arr, json_mut_t *obj);

char *JS_DocWriteString(json_mut_doc_t *doc, size_t *len);

#endif
