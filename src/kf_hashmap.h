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

#ifndef KF_HASHMAP_H
#define KF_HASHMAP_H

#include "doomtype.h"

typedef struct hashmap_entry_s hashmap_entry_t;

typedef struct
{
    hashmap_entry_t *entries;
    int size;
    int capacity;
} hashmap_t;

hashmap_t *hashmap_create(int capacity);
void hashmap_free(hashmap_t *map);

boolean hashmap_put(hashmap_t *map, uintptr_t key);

int hashmap_get(hashmap_t *map, uintptr_t key);

typedef struct
{
    hashmap_t *map;
    int index;
} hashmap_iterator_t;

hashmap_iterator_t hashmap_iterator(hashmap_t *map);
boolean hashmap_next(hashmap_iterator_t *iter, uintptr_t *key, int *number);

#endif