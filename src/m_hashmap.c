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

#include <stdint.h>
#include <string.h>

#include "m_hashmap.h"

// The hash map structure
struct hashmap_s
{
    uint64_t *keys;
    char *values; // Contiguous block for values
    int capacity;
    int size;
    size_t value_size;
};

// SplitMix64
inline static uint64_t HashInt64(uint64_t key)
{
    key += UINT64_C(0x9E3779B97F4A7C15);
    key ^= key >> 30;
    key *= UINT64_C(0xbf58476d1ce4e5b9);
    key ^= key >> 27;
    key *= UINT64_C(0x94d049bb133111eb);
    key ^= key >> 31;
    return key;
}

// Get a pointer to the value for a given entry index
inline static void *ValuePtr(const hashmap_t *map, int entry_index)
{
    return map->values + (entry_index * map->value_size);
}

// Finds the index for a key, or the index where it should be inserted.
inline static int FindIndex(const hashmap_t *map, uint64_t key)
{
    if (map->capacity == 0)
    {
        return -1;
    }

    int index = HashInt64(key) & (map->capacity - 1);

    for (int i = 0; i < map->capacity; ++i)
    {
        int try_index = (index + i) & (map->capacity - 1);

        if (!map->keys[try_index] || map->keys[try_index] == key)
        {
            return try_index;
        }
    }
    return -1; // Should not happen if load factor < 1
}

// Resizes the hash map to a new capacity and re-hashes all entries.
static void Resize(hashmap_t *map, int new_capacity)
{
    uint64_t *old_keys = map->keys;
    char *old_values = map->values;
    int old_capacity = map->capacity;

    map->keys = calloc(new_capacity, sizeof(uint64_t));
    map->values = malloc(new_capacity * map->value_size);
    map->capacity = new_capacity;

    for (int i = 0; i < old_capacity; ++i)
    {
        if (old_keys[i])
        {
            int new_index = HashInt64(old_keys[i]) & (new_capacity - 1);
            while (map->keys[new_index])
            {
                new_index = (new_index + 1) & (new_capacity - 1);
            }

            map->keys[new_index] = old_keys[i];
            void *old_value = old_values + i * map->value_size;
            void *new_value = ValuePtr(map, new_index);
            memcpy(new_value, old_value, map->value_size);
        }
    }
    free(old_keys);
    free(old_values);
}

hashmap_t *hashmap_init(int initial_capacity, size_t value_size)
{
    hashmap_t *map = calloc(1, sizeof(hashmap_t));

    int capacity = 16;
    while (capacity < initial_capacity)
    {
        capacity <<= 1;
    }

    map->keys = calloc(capacity, sizeof(uint64_t));
    map->values = malloc(capacity * value_size);
    map->capacity = capacity;
    map->size = 0;
    map->value_size = value_size;

    return map;
}

void hashmap_free(hashmap_t *map)
{
    free(map->keys);
    free(map->values);
    free(map);
}

void hashmap_put(hashmap_t *map, uint64_t key, const void *value)
{
    if (map->size >= map->capacity / 2)
    {
        Resize(map, map->capacity * 2);
    }

    int index = FindIndex(map, key);

    if (!map->keys[index])
    {
        map->keys[index] = key;
        map->size++;
    }

    void *entry_value = ValuePtr(map, index);
    memcpy(entry_value, value, map->value_size);
}

void *hashmap_get(const hashmap_t *map, uint64_t key)
{
    int index = FindIndex(map, key);
    if (index != -1)
    {
        if (map->keys[index])
        {
            return ValuePtr(map, index);
        }
    }
    return NULL;
}

int hashmap_size(const hashmap_t *map)
{
    return map->size;
}

hashmap_t *M_HashMapCopy(const hashmap_t *from)
{
    hashmap_t *to = hashmap_init(from->capacity, from->value_size);
    to->size = from->size;
    memcpy(to->keys, from->keys, from->capacity * sizeof(uint64_t));
    memcpy(to->values, from->values, from->capacity * from->value_size);
    return to;
}

hashmap_iterator_t hashmap_iterator(const hashmap_t *map)
{
    hashmap_iterator_t iter;
    iter.map = map;
    iter.index = -1;
    return iter;
}

void *hashmap_next(hashmap_iterator_t *iter, uint64_t *key_out)
{
    while (++iter->index < iter->map->capacity)
    {
        uint64_t key = iter->map->keys[iter->index];
        if (key)
        {
            if (key_out)
            {
                *key_out = key;
            }
            return ValuePtr(iter->map, iter->index);
        }
    }
    return NULL;
}
