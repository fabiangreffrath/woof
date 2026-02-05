//
// Copyright(C) 2026 Roman Fomin
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
//
//
// Generic hash map implementation for integer keys and fixed-size values.
//

#ifndef M_HASHMAP_H
#define M_HASHMAP_H

#include <stdint.h>
#include <stdlib.h>

typedef struct hashmap_s hashmap_t;

typedef struct
{
    const hashmap_t *map;
    int index;
} hashmap_iterator_t;

hashmap_t *hashmap_init(int initial_capacity, size_t value_size);

void hashmap_free(hashmap_t *map);

void hashmap_put(hashmap_t *map, uint64_t key, const void *value);

// Return pointer to value, NULL if not found
void *hashmap_get(const hashmap_t *map, uint64_t key);

int hashmap_size(const hashmap_t *map);

hashmap_iterator_t hashmap_iterator(const hashmap_t *map);

// Advances the iterator to the next element.
// Returns NULL if the end is reached.
void *hashmap_next(hashmap_iterator_t *iter, uint64_t *key_out);

hashmap_t *M_HashMapCopy(const hashmap_t *map);

#endif // M_HASHMAP_H
