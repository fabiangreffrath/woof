//
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2014 Fabian Greffrath
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
// Parses [STRINGS] sections in BEX files
//

#ifndef DEH_BEX_STRINGS_H
#define DEH_BEX_STRINGS_H

#include "doomtype.h"

typedef struct bex_string_s bex_string_t;

extern struct bex_string_s
{
    const char *mnemonic;
    const char *original_string;
} *bex_mnemonic_table;

void DEH_InitMnemonic(void);
const char *DEH_StringForMnemonic(const char *s) PRINTF_ARG_ATTR(1);

#endif
