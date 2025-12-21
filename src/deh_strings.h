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
// Dehacked string replacements
//

#ifndef DEH_STRINGS_H
#define DEH_STRINGS_H

#include <stdio.h>

#include "doomtype.h"
#include "d_englsh.h" // In order to not 'double include' in other files

// Used to do dehacked text substitutions throughout the program

const char *DEH_String(const char *s) PRINTF_ARG_ATTR(1);
void DEH_snprintf(char *buffer, size_t len, const char *fmt, ...) PRINTF_ATTR(3, 4);
void DEH_AddStringReplacement(const char *from_text, const char *to_text);
boolean DEH_HasStringReplacement(const char *s);

typedef struct bex_string_s bex_string_t;

extern const struct bex_string_s
{
    const char *macro;
    char *string;
} bex_mnemonic_strings[];

extern const char * const mapnames[];
extern const char * const mapnames2[];
extern const char * const mapnamesp[];
extern const char * const mapnamest[];
extern const char * const mnemonics_players[];
extern const char * const mnemonics_quit_messages[]; // killough 1/18/98 const added

// killough 1/18/98:
// replace hardcoded limit with extern var (silly hack, I know)
extern const int num_quit_mnemonics;


#endif /* #ifndef DEH_STR_H */
