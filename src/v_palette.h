//
//  Copyright (C) 1999 by
//  id Software, Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//

#include "doomtype.h"

#ifndef __V_PALETTE__
#define __V_PALETTE__

// Palette stuff
#define PLAYPAL_SIZE  (256)
#define PLAYPAL_BYTES (PLAYPAL_SIZE * 3)

typedef enum palette_e
{
    PAL_GLOBAL,
    PAL_IWAD,
    PAL_CUSTOM,
    PAL_COUNT,
} palette_t;

typedef enum palette_layer_e
{
    LAYER_BASE,
    LAYER_DAMAGE0,
    LAYER_DAMAGE1,
    LAYER_DAMAGE2,
    LAYER_DAMAGE3,
    LAYER_DAMAGE4,
    LAYER_DAMAGE5,
    LAYER_DAMAGE6,
    LAYER_DAMAGE7,
    LAYER_ITEM0,
    LAYER_ITEM1,
    LAYER_ITEM2,
    LAYER_ITEM3,
    LAYER_RADSUIT,
    LAYER_COUNT,

    LAYER_DAMAGE_COUNT = 8,
    LAYER_ITEM_COUNT = 4,
} palette_layer_t;

typedef struct rgb_s
{
    byte r, g, b;
} rgb_t;

typedef struct rgb_linear_s
{
    double r, g, b;
} lrgb_t;

typedef struct playpal_s
{
    char name[9];
    size_t num;
    size_t length;
    byte *data;

    rgb_t base[PLAYPAL_SIZE];
    lrgb_t base_linear[PLAYPAL_SIZE];

    byte white;
    byte black;
} playpal_t;

extern int gamma2;
extern playpal_t list_playpal[PAL_COUNT];
extern playpal_t *playpal_global;

void V_InitPalette(void);
void V_ResetPalette(void);
byte V_GetNearestColor(palette_t pal, const byte r, const byte g, const byte b);
byte V_GetNearestColorLinear(palette_t pal, const double r, const double g,
                             const double b);

#endif // __V_PALETTE__
