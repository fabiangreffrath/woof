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
#include <string.h>

#include "deh_strings.h"
#include "doomtype.h"
#include "m_array.h"
#include "m_misc.h"

const char * const mapnames[] =
{
    HUSTR_E1M1, HUSTR_E1M2, HUSTR_E1M3, HUSTR_E1M4, HUSTR_E1M5,
    HUSTR_E1M6, HUSTR_E1M7, HUSTR_E1M8, HUSTR_E1M9,

    HUSTR_E2M1, HUSTR_E2M2, HUSTR_E2M3, HUSTR_E2M4, HUSTR_E2M5,
    HUSTR_E2M6, HUSTR_E2M7, HUSTR_E2M8, HUSTR_E2M9,

    HUSTR_E3M1, HUSTR_E3M2, HUSTR_E3M3, HUSTR_E3M4, HUSTR_E3M5,
    HUSTR_E3M6, HUSTR_E3M7, HUSTR_E3M8, HUSTR_E3M9,

    HUSTR_E4M1, HUSTR_E4M2, HUSTR_E4M3, HUSTR_E4M4, HUSTR_E4M5,
    HUSTR_E4M6, HUSTR_E4M7, HUSTR_E4M8, HUSTR_E4M9,
};

const char * const mapnames2[] =
{
    HUSTR_1,  HUSTR_2,  HUSTR_3,  HUSTR_4,  HUSTR_5,
    HUSTR_6,  HUSTR_7,  HUSTR_8,  HUSTR_9,  HUSTR_10,
    HUSTR_11, HUSTR_12, HUSTR_13, HUSTR_14, HUSTR_15,
    HUSTR_16, HUSTR_17, HUSTR_18, HUSTR_19, HUSTR_20,
    HUSTR_21, HUSTR_22, HUSTR_23, HUSTR_24, HUSTR_25,
    HUSTR_26, HUSTR_27, HUSTR_28, HUSTR_29, HUSTR_30,
    HUSTR_31, HUSTR_32,
};

const char * const mapnamesp[] =
{
    PHUSTR_1,  PHUSTR_2,  PHUSTR_3,  PHUSTR_4,  PHUSTR_5,
    PHUSTR_6,  PHUSTR_7,  PHUSTR_8,  PHUSTR_9,  PHUSTR_10,
    PHUSTR_11, PHUSTR_12, PHUSTR_13, PHUSTR_14, PHUSTR_15,
    PHUSTR_16, PHUSTR_17, PHUSTR_18, PHUSTR_19, PHUSTR_20,
    PHUSTR_21, PHUSTR_22, PHUSTR_23, PHUSTR_24, PHUSTR_25,
    PHUSTR_26, PHUSTR_27, PHUSTR_28, PHUSTR_29, PHUSTR_30,
    PHUSTR_31, PHUSTR_32,
};

const char * const mapnamest[] =
{
    THUSTR_1,  THUSTR_2,  THUSTR_3,  THUSTR_4,  THUSTR_5,
    THUSTR_6,  THUSTR_7,  THUSTR_8,  THUSTR_9,  THUSTR_10,
    THUSTR_11, THUSTR_12, THUSTR_13, THUSTR_14, THUSTR_15,
    THUSTR_16, THUSTR_17, THUSTR_18, THUSTR_19, THUSTR_20,
    THUSTR_21, THUSTR_22, THUSTR_23, THUSTR_24, THUSTR_25,
    THUSTR_26, THUSTR_27, THUSTR_28, THUSTR_29, THUSTR_30,
    THUSTR_31, THUSTR_32,
};

const char * const strings_players[] =
{
    HUSTR_PLRGREEN, HUSTR_PLRINDIGO, HUSTR_PLRBROWN, HUSTR_PLRRED,
};

const char * const strings_quit_messages[] = 
{
    QUITMSG,   QUITMSG1,  QUITMSG2,  QUITMSG3, QUITMSG4,  QUITMSG5,
    QUITMSG6,  QUITMSG7,  QUITMSG8,  QUITMSG9, QUITMSG10, QUITMSG11,
    QUITMSG12, QUITMSG13, QUITMSG14,
};

// killough 1/18/98: remove hardcoded limit and replace with var (silly hack):
const int num_quit_mnemonics = arrlen(strings_quit_messages) - 1;

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
