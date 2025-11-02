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

#include "m_hashmap.h"

#include "doomtype.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// FNV-1a hash
static uint64_t hash_key(uintptr_t key)
{
    uint64_t hash = 0xcbf29ce484222325;
    for (int i = 0; i < sizeof(key); ++i)
    {
        hash ^= (key >> (i * 8)) & 0xff;
        hash *= 0x100000001b3;
    }
    return hash;
}

typedef struct
{
    uintptr_t key;
    int size;
    int align;
    int index;
} hashmap_entry_t;

struct hashmap_s
{
    hashmap_entry_t *entries;
    int capacity;
    int size;
};

hashmap_t *hashmap_init(int initial_capacity)
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

void hashmap_free(hashmap_t *map)
{
    free(map->entries);
    free(map);
}

static void resize(hashmap_t *map, int new_capacity)
{
    hashmap_entry_t *new_entries = calloc(new_capacity, sizeof(hashmap_entry_t));

    for (int i = 0; i < map->capacity; ++i)
    {
        hashmap_entry_t *entry = &map->entries[i];
        if (entry->key != 0)
        {
            int index = hash_key(entry->key) & (new_capacity - 1);
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

void hashmap_put(hashmap_t *map, uintptr_t key, int size, int align)
{
    if (map->size > map->capacity / 2)
    {
        resize(map, map->capacity * 2);
    }

    int index = hash_key(key) & (map->capacity - 1);
    // Linear probing to find slot for key
    while (map->entries[index].key != 0 && map->entries[index].key != key)
    {
        index = (index + 1) & (map->capacity - 1);
    }

    if (map->entries[index].key == 0)
    {
        map->entries[index].index = map->size;
        map->size++;
    }

    map->entries[index].key = key;
    map->entries[index].size = size;
    map->entries[index].align = align;
}

static boolean get(const hashmap_t *map, uintptr_t key, int *index, int *size,
                   int *align)
{
    int i = hash_key(key) & (map->capacity - 1);
    while (map->entries[i].key != 0)
    {
        if (map->entries[i].key == key)
        {
            if (index)
            {
                *index = map->entries[i].index;
            }
            if (size)
            {
                *size = map->entries[i].size;
            }
            if (align)
            {
                *align = map->entries[i].align;
            }
            return true;
        }
        i = (i + 1) & (map->capacity - 1);
    }
    return false;
}

boolean hashmap_get(hashmap_t *map, uintptr_t key, int *size, int *align)
{
    return get(map, key, NULL, size, align);
}

int hashmap_get_index(const hashmap_t *map, uintptr_t key)
{
    int index;
    if (get(map, key, &index, NULL, NULL))
    {
        return index;
    }
    return -1;
}

int hashmap_get_size(const hashmap_t *map)
{
    return map->size;
}

hashmap_iterator_t hashmap_iterator(const hashmap_t *map)
{
    hashmap_iterator_t iter;
    iter.map = map;
    iter.index = -1;
    return iter;
}

boolean hashmap_next(hashmap_iterator_t *iter, uintptr_t *key, int *index)
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
            if (index)
            {
                *index = iter->map->entries[iter->index].index;
            }
            return true;
        }
    }
    return false;
}

hashmap_t *M_HashMapCopy(const hashmap_t *from)
{
    hashmap_t *to = calloc(1, sizeof(hashmap_t));
    to->capacity = from->capacity;
    to->size = from->size;
    to->entries = malloc(from->capacity * sizeof(hashmap_entry_t));
    memcpy(to->entries, from->entries, from->capacity * sizeof(hashmap_entry_t));
    return to;
}
