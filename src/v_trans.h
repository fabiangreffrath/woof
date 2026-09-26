//
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2014 Paul Haeberli
// Copyright(C) 2014-2023 Fabian Greffrath
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
// Color translation tables
//

#ifndef __V_TRANS__
#define __V_TRANS__

#include "doomtype.h"
#include "v_palette.h"

// [FG] dark/shaded color translation table
extern byte *cr_dark;
extern byte *cr_shaded;
extern byte invul_gray[];

// symbolic indices into color translation table pointer array
typedef enum xlat_index_e
{
    CR_ORIG = -1,
    CR_BRICK,
    CR_TAN,
    CR_GRAY,
    CR_GREEN,
    CR_BROWN,
    CR_GOLD,
    CR_RED,
    CR_BLUE1,
    CR_ORANGE,
    CR_YELLOW,
    CR_BLUE2,
    CR_BLACK,
    CR_PURPLE,
    CR_WHITE,
    CR_BRIGHT,
    CR_NONE,
    CR_LIMIT,
} xlat_index_t;

typedef struct
{
    const char *name;
    const char *str;
    byte *lump;
    byte *table;
} xlat_t;

extern xlat_t xlat[CR_LIMIT];

#define ORIG_S "\x1b\x2f"

xlat_index_t V_CRByName(const char *name);
xlat_index_t V_BloodColor(int blood);
void V_InitColorTranslation(void);

#endif // __V_TRANS__
