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

#include "doomtype.h"

// Used to do dehacked text substitutions throughout the program

const char *DEH_String(const char *s) PRINTF_ARG_ATTR(1);
void DEH_AddStringReplacement(const char *from_text, const char *to_text);
boolean DEH_HasStringReplacement(const char *s);

#endif /* #ifndef DEH_STR_H */
