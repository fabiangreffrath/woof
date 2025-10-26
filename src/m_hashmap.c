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
#include "m_array.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define INITIAL_CAPACITY 1024

// FNV-1a hash
static uint64_t hash_key(uintptr_t key)
{
    uint64_t hash = 0xcbf29ce484222325;
    for (size_t i = 0; i < sizeof(key); ++i)
    {
        hash ^= (key >> (i * 8)) & 0xff;
        hash *= 0x100000001b3;
    }
    return hash;
}

typedef struct
{
    uintptr_t key;
    hashmap_value_t value;
    int index;
} hashmap_entry_t;

struct hashmap_s
{
    hashmap_entry_t *entries;
    int capacity;
    int size;
};

hashmap_t *M_HashMapInit(void)
{
    hashmap_t *map = calloc(1, sizeof(hashmap_t));
    map->entries = calloc(INITIAL_CAPACITY, sizeof(hashmap_entry_t));
    map->capacity = INITIAL_CAPACITY;
    return map;
}

void M_HashMapFree(hashmap_t *map)
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

void M_HashMapPut(hashmap_t *map, uintptr_t key, hashmap_value_t value)
{
    if (map->size > map->capacity / 2)
    {
        resize(map, map->capacity * 2);
    }

    int index = hash_key(key) & (map->capacity - 1);
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
    map->entries[index].value = value;
}

static boolean get(const hashmap_t *map, uintptr_t key, int *index, 
                   hashmap_value_t *value)
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

boolean M_HashMapGet(hashmap_t *map, uintptr_t key, hashmap_value_t *value)
{
    return get(map, key, NULL, value);
}

int M_HashMapGetIndex(const hashmap_t *map, uintptr_t key)
{
    int index;
    if (get(map, key, &index, NULL))
    {
        return index;
    }
    return -1;
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

hashmap_iterator_t M_HashMapIterator(const hashmap_t *map)
{
    hashmap_iterator_t iter;
    iter.map = map;
    iter.index = -1;
    return iter;
}

boolean M_HashMapNext(hashmap_iterator_t *iter, uintptr_t *key, int *index)
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

uintptr_t *M_HashMapTable(const hashmap_t *map)
{
    uintptr_t *table = NULL;
    array_resize(table, map->size);

    hashmap_iterator_t iter = M_HashMapIterator(map);
    uintptr_t key;
    int index;
    while (M_HashMapNext(&iter, &key, &index))
    {
        table[index] = key;
    }

    return table;
}

int M_HashMapSize(const hashmap_t *map)
{
    return map->size;
}
