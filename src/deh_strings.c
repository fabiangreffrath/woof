//
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2025 Guilherme Miranda
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
// Parses Text substitution sections in dehacked files
//

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "deh_strings.h"
#include "doomtype.h"
#include "m_array.h"
#include "m_misc.h"

typedef struct
{
    char *from_text;
    char *to_text;
} deh_substitution_t;

static deh_substitution_t **hash_table = NULL;
static int hash_table_entries;
static int hash_table_length = -1;

// [Woof!] Colorized messages
static deh_substitution_t *color_strings = NULL;
static int color_count = 0;

// 64bit variant FNV-1a hash
static uint64_t hash_str(const char *s)
{
    uint64_t hash = 0xcbf29ce484222325;
    for (; *s; ++s)
    {
        hash ^= (uint8_t)*s;
        hash *= 0x100000001b3;
    }
    return hash;
}

static deh_substitution_t *SubstitutionForString(const char *s)
{
    // Fallback if we have not initialized the hash table yet
    if (hash_table_length < 0)
    {
        return NULL;
    }

    // We only use power-of-2 sizes, yeah? Use a bit mask
    int entry = hash_str(s) & (hash_table_length - 1);

    while (hash_table[entry] != NULL)
    {
        if (!strcmp(hash_table[entry]->from_text, s))
        {
            // substitution found!
            return hash_table[entry];
        }

        entry = (entry + 1) % hash_table_length;
    }

    // no substitution found
    return NULL;
}

// Look up a string to see if it has been replaced with something else
// This will be used throughout the program to substitute text
const char *DEH_String(const char *s)
{
    deh_substitution_t *subst = SubstitutionForString(s);

    if (subst != NULL)
    {
        return subst->to_text;
    }
    else
    {
        return s;
    }
}

// [crispy] returns true if a string has been substituted
boolean DEH_HasStringReplacement(const char *s)
{
    return SubstitutionForString(s) != NULL;
}

static void InitHashTable(void)
{
    // init hash table
    hash_table_entries = 0;
    hash_table_length = 16;
    hash_table = calloc(sizeof(deh_substitution_t *), hash_table_length);
}

static void DEH_AddToHashtable(deh_substitution_t *sub);

static void IncreaseHashtable(void)
{
    // save the old table
    deh_substitution_t **old_table = hash_table;
    int old_table_length = hash_table_length;

    // double the size
    hash_table_length *= 2;
    hash_table = calloc(sizeof(deh_substitution_t *), hash_table_length);

    // go through the old table and insert all the old entries
    for (int i = 0; i < old_table_length; ++i)
    {
        if (old_table[i] != NULL)
        {
            DEH_AddToHashtable(old_table[i]);
        }
    }

    // free the old table
    free(old_table);
}

static void DEH_AddToHashtable(deh_substitution_t *sub)
{
    // if the hash table is more than 60% full, increase its size
    if ((hash_table_entries * 10) / hash_table_length > 6)
    {
        IncreaseHashtable();
    }

    // find where to insert it
    // ... by bit-masking the power-of-2
    int entry = hash_str(sub->from_text) & (hash_table_length - 1);

    while (hash_table[entry] != NULL)
    {
        entry = (entry + 1) % hash_table_length;
    }

    hash_table[entry] = sub;
    ++hash_table_entries;
}

void DEH_AddStringReplacement(const char *from_text, const char *to_text)
{
    // Initialize the hash table if this is the first time
    if (hash_table_length < 0)
    {
        InitHashTable();
    }

    // Check to see if there is an existing substitution already in place.
    deh_substitution_t *sub = SubstitutionForString(from_text);

    if (sub != NULL)
    {
        free(sub->to_text);

        size_t len = strlen(to_text) + 1;
        sub->to_text = malloc(len);
        memcpy(sub->to_text, to_text, len);
    }
    else
    {
        // We need to allocate a new substitution.
        sub = malloc(sizeof(deh_substitution_t));

        // We need to create our own duplicates of the provided strings.
        size_t from_len = strlen(from_text) + 1;
        sub->from_text = malloc(from_len);
        memcpy(sub->from_text, from_text, from_len);

        size_t to_len = strlen(to_text) + 1;
        sub->to_text = malloc(to_len);
        memcpy(sub->to_text, to_text, to_len);

        DEH_AddToHashtable(sub);
    }
}

PRINTF_ARG_ATTR(1) const char *DEH_StringColorized(const char *s)
{
    if (message_colorized)
    {
        for (int i = 0; i < color_count; i++)
        {
            if (!strcasecmp(s, color_strings[i].from_text))
            {
                return color_strings[i].to_text;
            }
        }
    }
    return DEH_String(s);
}

void DEH_AddStringColorizedReplacement(const char *from_text, const char *color_text)
{
    deh_substitution_t sub = {0};
    sub.from_text = M_StringDuplicate(from_text);
    sub.to_text = M_StringDuplicate(color_text);
    // Not going to make a whole hash table just for a handful of strings :p
    array_push(color_strings, sub);
    color_count += 1;
}
