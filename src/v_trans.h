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
    CR_BRICK,  // 0
    CR_TAN,    // 1
    CR_GRAY,   // 2
    CR_GREEN,  // 3
    CR_BROWN,  // 4
    CR_GOLD,   // 5
    CR_RED,    // 6
    CR_BLUE1,  // 7
    CR_ORANGE, // 8
    CR_YELLOW, // 9
    CR_BLUE2,  // 10
    CR_BLACK,  // 11
    CR_PURPLE, // 12
    CR_WHITE,  // 13
    CR_BRIGHT, // 14
    CR_NONE,   // 15 // [FG] dummy
    CR_LIMIT   // 16 //jff 2/27/98 added for range check
} xlat_index_t;

typedef struct
{
    const char *name;
    const char *str;
    byte *lump;
    byte *table;
} xlat_t;

extern xlat_t xlat[CR_LIMIT];

#define ORIG_S  "\x1b\x2f"
#define GRAY_S  "\x1b\x32"
#define GREEN_S "\x1b\x33"
#define BROWN_S "\x1b\x34"
#define GOLD_S  "\x1b\x35"
#define BLUE1_S "\x1b\x37"
#define BLUE2_S "\x1b\x3a"

xlat_index_t V_CRByName(const char *name);
xlat_index_t V_BloodColor(int blood);
void V_InitColorTranslation(void);

#endif // __V_TRANS__
