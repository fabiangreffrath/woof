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

#include "i_system.h"
#include "m_hashmap.h"
#include "m_misc.h"

typedef enum
{
    KEY_TYPE_UINT64,
    KEY_TYPE_STRING
} hashmap_key_type_t;

// An entry in the hash map
typedef struct
{
    uint64_t key_or_hash;
    char *string_key;
    boolean occupied;

    union
    {
        int value_index;
        char value_embedded[sizeof(void *)];
    } v;
} hashmap_entry_t;

// The hash map structure
struct hashmap_s
{
    hashmap_key_type_t key_type;
    hashmap_entry_t *entries;
    char *values; // Contiguous block for values if not embedded
    int capacity;
    int values_capacity;
    int size;
    int value_size;
    boolean values_are_packed;
};

// SplitMix64
inline static uint64_t HashInt64(uint64_t key)
{
    key += UINT64_C(0x9e3779b97f4a7c15);
    key ^= key >> 30;
    key *= UINT64_C(0xbf58476d1ce4e5b9);
    key ^= key >> 27;
    key *= UINT64_C(0x94d049bb133111eb);
    key ^= key >> 31;
    return key;
}

// djb2
inline static uint64_t HashString(const char *key)
{
    uint64_t hash = 5381;
    int c;

    while ((c = *key++))
    {
        hash = ((hash << 5) + hash) + c;
    }

    return hash;
}

// Get a pointer to the value for a given entry index
inline static void *ValuePtr(const hashmap_t *map, int entry_index)
{
    const hashmap_entry_t *entry = &map->entries[entry_index];
    if (map->values_are_packed)
    {
        return map->values + (entry->v.value_index * map->value_size);
    }
    else
    {
        return (void *)entry->v.value_embedded;
    }
}

// Finds the index for a key, or the index where it should be inserted.
inline static int FindIndex(const hashmap_t *map, uint64_t key)
{
    if (map->capacity == 0)
    {
        return -1;
    }

    uint64_t hash = HashInt64(key);
    int index = hash & (map->capacity - 1);

    for (int i = 0; i < map->capacity; ++i)
    {
        int try_index = (index + i) & (map->capacity - 1);
        const hashmap_entry_t *entry = &map->entries[try_index];

        if (!entry->occupied || entry->key_or_hash == key)
        {
            return try_index;
        }
    }
    return -1; // Should not happen if load factor < 1
}

// Finds the index for a string key, or the index where it should be inserted.
inline static int FindIndexStr(const hashmap_t *map, const char *key,
                               uint64_t hash)
{
    if (map->capacity == 0)
    {
        return -1;
    }

    int index = hash & (map->capacity - 1);

    for (int i = 0; i < map->capacity; ++i)
    {
        int try_index = (index + i) & (map->capacity - 1);
        const hashmap_entry_t *entry = &map->entries[try_index];

        if (!entry->occupied
            || (entry->key_or_hash == hash
                && strcmp(entry->string_key, key) == 0))
        {
            return try_index;
        }
    }
    return -1; // Should not happen if load factor < 1
}

// Resizes the hash map's main table and re-hashes all entries.
static void Resize(hashmap_t *map, int new_capacity)
{
    hashmap_entry_t *old_entries = map->entries;
    int old_capacity = map->capacity;

    map->entries = calloc(new_capacity, sizeof(hashmap_entry_t));
    map->capacity = new_capacity;

    for (int i = 0; i < old_capacity; ++i)
    {
        if (old_entries[i].occupied)
        {
            uint64_t key_or_hash = old_entries[i].key_or_hash;
            uint64_t hash;
            if (map->key_type == KEY_TYPE_STRING)
            {
                hash = key_or_hash;
            }
            else
            {
                hash = HashInt64(key_or_hash);
            }
            int new_index = hash & (new_capacity - 1);

            while (map->entries[new_index].occupied)
            {
                new_index = (new_index + 1) & (new_capacity - 1);
            }
            map->entries[new_index] = old_entries[i];
        }
    }

    free(old_entries);
}

static hashmap_t *Init(int initial_capacity, size_t value_size,
                       hashmap_key_type_t key_type)
{
    hashmap_t *map = calloc(1, sizeof(hashmap_t));

    int capacity = 16;
    while (capacity < initial_capacity)
    {
        capacity <<= 1;
    }

    map->key_type = key_type;
    map->entries = calloc(capacity, sizeof(hashmap_entry_t));
    map->capacity = capacity;
    map->size = 0;
    map->value_size = value_size;
    map->values_are_packed = value_size > sizeof(void *);

    if (map->values_are_packed)
    {
        map->values_capacity = capacity;
        map->values = malloc(map->values_capacity * value_size);
    }
    else
    {
        map->values_capacity = 0;
        map->values = NULL;
    }

    return map;
}

hashmap_t *hashmap_init(int initial_capacity, size_t value_size)
{
    return Init(initial_capacity, value_size, KEY_TYPE_UINT64);
}

hashmap_t *hashmap_init_str(int initial_capacity, size_t value_size)
{
    return Init(initial_capacity, value_size, KEY_TYPE_STRING);
}

void hashmap_free(hashmap_t *map)
{
    if (!map)
    {
        return;
    }

    if (map->key_type == KEY_TYPE_STRING)
    {
        for (int i = 0; i < map->capacity; ++i)
        {
            if (map->entries[i].occupied)
            {
                free(map->entries[i].string_key);
            }
        }
    }
    free(map->entries);
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
    hashmap_entry_t *entry = &map->entries[index];

    if (!entry->occupied)
    {
        entry->occupied = true;
        entry->key_or_hash = key;
        if (map->values_are_packed)
        {
            if (map->size >= map->values_capacity)
            {
                map->values_capacity *= 2;
                map->values = realloc(map->values,
                                      map->values_capacity * map->value_size);
            }
            entry->v.value_index = map->size;
        }
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
        if (map->entries[index].occupied)
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
    hashmap_t *to = calloc(1, sizeof(hashmap_t));

    to->key_type = from->key_type;
    to->capacity = from->capacity;
    to->values_capacity = from->values_capacity;
    to->size = from->size;
    to->value_size = from->value_size;
    to->values_are_packed = from->values_are_packed;

    to->entries = malloc(from->capacity * sizeof(hashmap_entry_t));
    memcpy(to->entries, from->entries, from->capacity * sizeof(hashmap_entry_t));

    if (from->key_type == KEY_TYPE_STRING)
    {
        for (int i = 0; i < to->capacity; i++)
        {
            if (to->entries[i].occupied)
            {
                to->entries[i].string_key =
                    M_StringDuplicate(from->entries[i].string_key);
            }
        }
    }

    if (from->values_are_packed)
    {
        to->values = malloc(from->values_capacity * from->value_size);
        memcpy(to->values, from->values, from->size * from->value_size);
    }

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
        const hashmap_entry_t *entry = &iter->map->entries[iter->index];
        if (entry->occupied)
        {
            if (key_out && iter->map->key_type == KEY_TYPE_UINT64)
            {
                *key_out = entry->key_or_hash;
            }
            return ValuePtr(iter->map, iter->index);
        }
    }
    return NULL;
}

void hashmap_put_str(hashmap_t *map, const char *key, const void *value)
{
    if (map->key_type != KEY_TYPE_STRING)
    {
        I_Error("This hashmap was not initialized for string keys.");
        return;
    }

    if (map->size >= map->capacity / 2)
    {
        Resize(map, map->capacity * 2);
    }

    uint64_t hash = HashString(key);
    int index = FindIndexStr(map, key, hash);
    hashmap_entry_t *entry = &map->entries[index];

    if (!entry->occupied)
    {
        entry->occupied = true;
        entry->key_or_hash = hash;
        entry->string_key = M_StringDuplicate(key);
        if (map->values_are_packed)
        {
            if (map->size >= map->values_capacity)
            {
                map->values_capacity *= 2;
                map->values = realloc(map->values,
                                      map->values_capacity * map->value_size);
            }
            entry->v.value_index = map->size;
        }
        map->size++;
    }

    void *entry_value = ValuePtr(map, index);
    memcpy(entry_value, value, map->value_size);
}

void *hashmap_get_str(const hashmap_t *map, const char *key)
{
    if (map->key_type != KEY_TYPE_STRING)
    {
        I_Error("This hashmap was not initialized for string keys.");
        return NULL;
    }

    uint64_t hash = HashString(key);
    int index = FindIndexStr(map, key, hash);

    if (index != -1)
    {
        if (map->entries[index].occupied)
        {
            return ValuePtr(map, index);
        }
    }

    return NULL;
}

void *hashmap_next_str(hashmap_iterator_t *iter, const char **key_out)
{
    while (++iter->index < iter->map->capacity)
    {
        const hashmap_entry_t *entry = &iter->map->entries[iter->index];
        if (entry->occupied && iter->map->key_type == KEY_TYPE_STRING)
        {
            if (key_out)
            {
                *key_out = entry->string_key;
            }
            return ValuePtr(iter->map, iter->index);
        }
    }
    return NULL;
}
