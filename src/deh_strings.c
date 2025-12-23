//
// Copyright(C) 2005-2014 Simon Howard
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
#include <string.h>
#include <string.h>

#include "deh_strings.h"
#include "doomtype.h"
#include "z_zone.h"

typedef struct
{
    char *from_text;
    char *to_text;
} deh_substitution_t;

static deh_substitution_t **hash_table = NULL;
static int hash_table_entries;
static int hash_table_length = -1;

// This is the algorithm used by glib
static unsigned int strhash(const char *s)
{
    const char *p = s;
    unsigned int h = *p;

    if (h)
    {
        for (p += 1; *p; p++)
        {
            h = (h << 5) - h + *p;
        }
    }

    return h;
}

static deh_substitution_t *SubstitutionForString(const char *s)
{
    // Fallback if we have not initialized the hash table yet
    if (hash_table_length < 0)
    {
        return NULL;
    }

    int entry = strhash(s) % hash_table_length;

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
    return DEH_String(s) != s;
}

static void InitHashTable(void)
{
    // init hash table
    hash_table_entries = 0;
    hash_table_length = 16;
    hash_table = Z_Malloc(sizeof(deh_substitution_t *) * hash_table_length, PU_STATIC, NULL);
    memset(hash_table, 0, sizeof(deh_substitution_t *) * hash_table_length);
}

static void DEH_AddToHashtable(deh_substitution_t *sub);

static void IncreaseHashtable(void)
{
    // save the old table
    deh_substitution_t **old_table = hash_table;
    int old_table_length = hash_table_length;

    // double the size
    hash_table_length *= 2;
    hash_table = Z_Malloc(sizeof(deh_substitution_t *) * hash_table_length, PU_STATIC, NULL);
    memset(hash_table, 0, sizeof(deh_substitution_t *) * hash_table_length);

    // go through the old table and insert all the old entries
    for (int i = 0; i < old_table_length; ++i)
    {
        if (old_table[i] != NULL)
        {
            DEH_AddToHashtable(old_table[i]);
        }
    }

    // free the old table
    Z_Free(old_table);
}

static void DEH_AddToHashtable(deh_substitution_t *sub)
{
    // if the hash table is more than 60% full, increase its size
    if ((hash_table_entries * 10) / hash_table_length > 6)
    {
        IncreaseHashtable();
    }

    // find where to insert it
    int entry = strhash(sub->from_text) % hash_table_length;

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
    size_t len;

    if (sub != NULL)
    {
        Z_Free(sub->to_text);

        len = strlen(to_text) + 1;
        sub->to_text = Z_Malloc(len, PU_STATIC, NULL);
        memcpy(sub->to_text, to_text, len);
    }
    else
    {
        // We need to allocate a new substitution.
        sub = Z_Malloc(sizeof(*sub), PU_STATIC, 0);

        // We need to create our own duplicates of the provided strings.
        len = strlen(from_text) + 1;
        sub->from_text = Z_Malloc(len, PU_STATIC, NULL);
        memcpy(sub->from_text, from_text, len);

        len = strlen(to_text) + 1;
        sub->to_text = Z_Malloc(len, PU_STATIC, NULL);
        memcpy(sub->to_text, to_text, len);

        DEH_AddToHashtable(sub);
    }
}
