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

hashmap_t *hashmap_init(int initial_capacity);
void hashmap_free(hashmap_t *map);

void hashmap_put(hashmap_t *map, uintptr_t key, int size, int align);
boolean hashmap_get(hashmap_t *map, uintptr_t key, int *size, int *align);
int hashmap_get_index(const hashmap_t *map, uintptr_t key);
int hashmap_get_size(const hashmap_t *map);

typedef struct
{
    const hashmap_t *map;
    int index;
} hashmap_iterator_t;

hashmap_iterator_t hashmap_iterator(const hashmap_t *map);
boolean hashmap_next(hashmap_iterator_t *iter, uintptr_t *key, int *index);

hashmap_t *M_HashMapCopy(const hashmap_t *map);

#endif // M_HASHMAP_H
