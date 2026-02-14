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
hashmap_t *hashmap_init_str(int initial_capacity, size_t value_size);

void hashmap_free(hashmap_t *map);

void hashmap_put(hashmap_t *map, uint64_t key, const void *value);

// Put a value with a string key
void hashmap_put_str(hashmap_t *map, const char *key, const void *value);

// Return pointer to value, NULL if not found
void *hashmap_get(const hashmap_t *map, uint64_t key);

// Return pointer to value, NULL if not found
void *hashmap_get_str(const hashmap_t *map, const char *key);

int hashmap_size(const hashmap_t *map);

hashmap_iterator_t hashmap_iterator(const hashmap_t *map);

// Advances the iterator to the next element.
// Returns NULL if the end is reached.
void *hashmap_next(hashmap_iterator_t *iter, uint64_t *key_out);

// Advances the iterator to the next string-keyed element.
// Returns NULL if the end is reached.
void *hashmap_next_str(hashmap_iterator_t *iter, const char **key_out);

#define hashmap_foreach(value, map) \
    for (hashmap_iterator_t _iter = hashmap_iterator(map); \
         (value = hashmap_next(&_iter, NULL));)

#define hashmap_foreach_str(value, map) \
    for (hashmap_iterator_t _iter = hashmap_iterator(map); \
         (value = hashmap_next_str(&_iter, NULL));)

hashmap_t *M_HashMapCopy(const hashmap_t *map);

#endif // M_HASHMAP_H
