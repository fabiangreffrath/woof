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

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_HASHMAP_VALUE_T
  #define M_HASHMAP_VALUE_T int
#endif

#ifndef M_HASHMAP_KEY_T
  #define M_HASHMAP_KEY_T int
#endif

typedef struct
{
    M_HASHMAP_VALUE_T value;
    M_HASHMAP_KEY_T key;
} hashmap_entry_t;

typedef struct
{
    hashmap_entry_t *entries;
    int capacity;
    int size;
} hashmap_t;

typedef struct
{
    const hashmap_t *map;
    int index;
} hashmap_iterator_t;

// FNV-1a hash
inline static uint64_t M_HashKey(M_HASHMAP_KEY_T key)
{
    uint64_t hash = 0xcbf29ce484222325;
    for (int i = 0; i < sizeof(key); ++i)
    {
        hash ^= (key >> (i * 8)) & 0xff;
        hash *= 0x100000001b3;
    }
    return hash;
}

static hashmap_t *hashmap_init(int initial_capacity)
{
    hashmap_t *map = calloc(1, sizeof(hashmap_t));

    // Round up to nearest power of 2 for efficient modulo
    int capacity = 16;
    while (capacity < initial_capacity)
    {
        capacity <<= 1;
    }

    map->entries = calloc(capacity, sizeof(hashmap_entry_t));
    map->capacity = capacity;
    return map;
}

static void hashmap_free(hashmap_t *map)
{
    free(map->entries);
    free(map);
}

static void M_HashMapResize(hashmap_t *map, int new_capacity)
{
    hashmap_entry_t *new_entries = calloc(new_capacity, sizeof(hashmap_entry_t));

    for (int i = 0; i < map->capacity; ++i)
    {
        hashmap_entry_t *entry = &map->entries[i];
        if (entry->key != 0)
        {
            int index = M_HashKey(entry->key) & (new_capacity - 1);
            while (new_entries[index].key != 0)
            {
                index = (index + 1) & (new_capacity - 1);
            }
            new_entries[index] = *entry;
        }
    }

    free(map->entries);
    map->entries = new_entries;
    map->capacity = new_capacity;
}

static void hashmap_put(hashmap_t *map, M_HASHMAP_KEY_T key,
                        M_HASHMAP_VALUE_T *value)
{
    if (map->size > map->capacity / 2)
    {
        M_HashMapResize(map, map->capacity * 2);
    }

    int index = M_HashKey(key) & (map->capacity - 1);
    // Linear probing to find slot for key
    while (map->entries[index].key != 0 && map->entries[index].key != key)
    {
        index = (index + 1) & (map->capacity - 1);
    }

    map->size++;

    map->entries[index].key = key;
    if (value)
    {
        map->entries[index].value = *value;
    }
}

inline static boolean hashmap_get(hashmap_t *map, M_HASHMAP_KEY_T key,
                                  M_HASHMAP_VALUE_T *value)
{
    int i = M_HashKey(key) & (map->capacity - 1);
    while (map->entries[i].key != 0)
    {
        if (map->entries[i].key == key)
        {
            if (value)
            {
                *value = map->entries[i].value;
            }
            return true;
        }
        i = (i + 1) & (map->capacity - 1);
    }
    return false;
}

inline static hashmap_iterator_t hashmap_iterator(const hashmap_t *map)
{
    hashmap_iterator_t iter;
    iter.map = map;
    iter.index = -1;
    return iter;
}

inline static boolean hashmap_next(hashmap_iterator_t *iter,
                                   M_HASHMAP_KEY_T *key,
                                   M_HASHMAP_VALUE_T *value)
{
    int capacity = iter->map->capacity;
    while (++iter->index < capacity)
    {
        if (iter->map->entries[iter->index].key)
        {
            if (key)
            {
                *key = iter->map->entries[iter->index].key;
            }
            if (value)
            {
                *value = iter->map->entries[iter->index].value;
            }
            return true;
        }
    }
    return false;
}

static hashmap_t *M_HashMapCopy(const hashmap_t *from)
{
    hashmap_t *to = calloc(1, sizeof(hashmap_t));
    to->capacity = from->capacity;
    to->size = from->size;
    to->entries = malloc(from->capacity * sizeof(hashmap_entry_t));
    memcpy(to->entries, from->entries, from->capacity * sizeof(hashmap_entry_t));
    return to;
}

#endif // M_HASHMAP_H
