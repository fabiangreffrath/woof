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
    PAL_LAYER_BASE,
    PAL_LAYER_DAMAGE0,
    PAL_LAYER_DAMAGE1,
    PAL_LAYER_DAMAGE2,
    PAL_LAYER_DAMAGE3,
    PAL_LAYER_DAMAGE4,
    PAL_LAYER_DAMAGE5,
    PAL_LAYER_DAMAGE6,
    PAL_LAYER_DAMAGE7,
    PAL_LAYER_ITEM0,
    PAL_LAYER_ITEM1,
    PAL_LAYER_ITEM2,
    PAL_LAYER_ITEM3,
    PAL_LAYER_RADSUIT,
    PAL_LAYER_COUNT,

    PAL_LAYER_DAMAGE_COUNT = 8,
    PAL_LAYER_ITEM_COUNT = 4,
} palette_layer_t;

typedef struct rgb_s
{
    byte r, g, b;
} rgb_t;

typedef struct rgb_linear_s
{
    double r, g, b;
} lrgb_t;

typedef enum gammalevel_e
{
    GAMMA_MIN = 0,
    GAMMA_MAX = 17,
    GAMMA_COUNT = 18,
} gammalevel_t;

typedef struct playpal_s
{
    char name[9];
    size_t num;
    size_t length;
    byte *data;

    byte *palette[GAMMA_COUNT];

    rgb_t base[PLAYPAL_SIZE];
    lrgb_t base_linear[PLAYPAL_SIZE];

    byte white;
    byte black;
} playpal_t;

extern int gamma2;
extern playpal_t playpals[PAL_COUNT];
extern playpal_t *playpal_global;
extern playpal_t *playpal_iwad;

void V_InitPalette(void);
void V_ResetPalette(void);
void V_SetCustomPalette(const char *name);
byte V_GetNearestColor(palette_t pal, const byte r, const byte g, const byte b);
byte V_GetNearestColorLinear(palette_t pal, const double r, const double g,
                             const double b);

#endif // __V_PALETTE__
