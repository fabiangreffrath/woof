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

#include "kf_hashmap.h"

#include "doomtype.h"
#include "m_array.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>

struct hashmap_entry_s
{
    uintptr_t key;
    int number;
    boolean occupied;
};

static void hashmap_grow(hashmap_t *map);

hashmap_t *hashmap_create(int capacity)
{
    hashmap_t *map = calloc(1, sizeof(hashmap_t));

    // Round up to nearest power of 2 for efficient modulo
    int actual_capacity = 8;
    while (actual_capacity < capacity)
    {
        actual_capacity <<= 1;
    }

    array_resize(map->entries, actual_capacity);
    return map;
}

void hashmap_free(hashmap_t *map)    
{
    if (map)
    {
        array_free(map->entries);
        free(map);
    }
}

inline static int hash(uintptr_t key, int capacity)
{
#if UINTPTR_MAX == UINT64_MAX
    // 64-bit optimized constant (golden ratio)
    return (key * 11400714819323198485ull) & (capacity - 1);
#else
    // 32-bit optimized constant
    return (key * 2654435761ul) & (capacity - 1);
#endif
}

// Linear probing to find slot for key
static int find_slot(hashmap_t *map, uintptr_t key)
{
    int capacity = array_capacity(map->entries);
    int index = hash(key, capacity);
    int start_index = index;

    while (map->entries[index].occupied)
    {
        if (map->entries[index].key == key)
        {
            return index;
        }
        index = (index + 1) & (capacity - 1);
        if (index == start_index)
        {
            break; // Full table
        }
    }
    return index;
}

static void hashmap_grow(hashmap_t *map)
{
    int old_capacity = array_capacity(map->entries);
    hashmap_entry_t *old_entries = map->entries;

    int new_capacity = old_capacity * 2;

    hashmap_entry_t *new_entries = NULL;
    array_resize(new_entries, new_capacity);

    // Rehash
    for (int i = 0; i < old_capacity; ++i)
    {
        if (old_entries[i].occupied)
        {
            int index = hash(old_entries[i].key, new_capacity);
            while (new_entries[index].occupied)
            {
                index = (index + 1) & (new_capacity - 1);
            }

            new_entries[index].key = old_entries[i].key;
            new_entries[index].number = old_entries[i].number;
            new_entries[index].occupied = true;
        }
    }

    array_free(old_entries);
    map->entries = new_entries;
}

boolean hashmap_put(hashmap_t *map, uintptr_t key)
{
    if (map->size + 1 > 3 * array_capacity(map->entries) / 4)
    {
        hashmap_grow(map);
    }

    size_t index = find_slot(map, key);
    if (!map->entries[index].occupied)
    {
        map->entries[index].key = key;
        map->entries[index].occupied = true;
        map->entries[index].number = map->size;
        map->size++;
        return true;
    }
    return false;
}

int hashmap_get(hashmap_t *map, uintptr_t key)
{
    size_t index = find_slot(map, key);
    if (map->entries[index].occupied && map->entries[index].key == key)
    {
        return map->entries[index].number;
    }
    I_Error("Not found %" PRIuPTR, key);
}

hashmap_iterator_t hashmap_iterator(hashmap_t *map)
{
    hashmap_iterator_t iter;
    iter.map = map;
    iter.index = -1;
    return iter;
}

boolean hashmap_next(hashmap_iterator_t *iter, uintptr_t *key, int *number)
{
    int capacity = array_capacity(iter->map->entries);
    while (++iter->index < capacity)
    {
        if (iter->map->entries[iter->index].occupied)
        {
            if (key)
            {
                *key = iter->map->entries[iter->index].key;
            }
            if (number)
            {
                *number = iter->map->entries[iter->index].number;
            }
            return true;
        }
    }
    return false;
}
