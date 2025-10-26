//
// Copyright(C) 2025 Roman Fomin
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

#ifndef M_HASHMAP_H
#define M_HASHMAP_H

#include "doomtype.h"

typedef struct hashmap_s hashmap_t;

typedef struct
{
    int index;
    int size;
    int align;
} hashmap_value_t;

hashmap_t* M_HashMapInit(void);
void M_HashMapFree(hashmap_t *map);
hashmap_t *M_HashMapCopy(const hashmap_t *map);

void M_HashMapPut(hashmap_t *map, uintptr_t key, hashmap_value_t value);
boolean M_HashMapGet(hashmap_t *map, uintptr_t key, hashmap_value_t *value);
int M_HashMapGetIndex(const hashmap_t *map, uintptr_t key);

typedef struct
{
    const hashmap_t *map;
    int index;
} hashmap_iterator_t;

hashmap_iterator_t M_HashMapIterator(const hashmap_t *map);
boolean M_HashMapNext(hashmap_iterator_t *iter, uintptr_t *key, int *index);

uintptr_t *M_HashMapTable(const hashmap_t *map);
int M_HashMapSize(const hashmap_t *map);

#endif // M_HASHMAP_H
