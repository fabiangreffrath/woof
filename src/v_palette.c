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

#include <float.h>
#include <string.h>

#include "i_gamma.h"
#include "i_video.h"
#include "m_misc.h"
#include "v_palette.h"
#include "v_srgb.h"
#include "v_trans.h"
#include "w_wad.h"
#include "z_zone.h"

// Palette stuff
int gamma2;
playpal_t playpals[PAL_COUNT];

playpal_t *playpal_global = NULL; // Current palette in use
playpal_t *playpal_base = NULL;   // Expected, normal palette, usually is global
playpal_t *playpal_iwad = NULL;   // Specifically IWAD, for use in repaletting

// Taken from Chocolate Doom chocolate-doom/src/i_video.c:L841-867
// Adapted to use Linear sRGB instead of Gamma sRGB

byte V_GetNearestColor(palette_t pal, const byte red, const byte green,
                       const byte blue)
{
    const double linear_red = sRGB_ByteToLinear(red),
                 linear_green = sRGB_ByteToLinear(green),
                 linear_blue = sRGB_ByteToLinear(blue);

    return V_GetNearestColorLinear(pal, linear_red, linear_green, linear_blue);
}

byte V_GetNearestColorLinear(palette_t pal, const double r, const double g,
                             const double b)
{
    byte best = 0;
    double best_diff = DBL_MAX;

    const lrgb_t *pal_rover = playpals[pal].base_linear;

    for (int i = 0; i < PLAYPAL_SIZE; ++i)
    {
        const double dr = r - pal_rover[i].r, dg = g - pal_rover[i].g,
                     db = b - pal_rover[i].b;

        const double diff = dr * dr + dg * dg + db * db;

        if (diff < best_diff)
        {
            if (!diff)
            {
                return i;
            }

            best = i;
            best_diff = diff;
        }
    }

    return best;
}

static playpal_t *InitPlaypal(palette_t pal, const char *name, int32_t num)
{
    playpal_t *playpal = &playpals[pal];

    M_CopyLumpName(playpal->name, name);
    playpal->num = num;
    playpal->length = W_LumpLength(playpal->num);
    playpal->data = W_CacheLumpNum(playpal->num, PU_STATIC);

    for (size_t i = 0; i < PLAYPAL_SIZE; i++)
    {
        const byte r = playpal->data[i * 3 + 0],
                   g = playpal->data[i * 3 + 1],
                   b = playpal->data[i * 3 + 2];

        playpal->base[i].r = r;
        playpal->base[i].g = g;
        playpal->base[i].b = b;

        playpal->base_linear[i].r = sRGB_ByteToLinear(r);
        playpal->base_linear[i].g = sRGB_ByteToLinear(g);
        playpal->base_linear[i].b = sRGB_ByteToLinear(b);
    }
    playpal->white = V_GetNearestColor(pal, 0xFF, 0xFF, 0xFF);
    playpal->black = V_GetNearestColor(pal, 0x00, 0x00, 0x00);

    for (gammalevel_t g = GAMMA_MIN; g < GAMMA_COUNT; g++)
    {
        playpal->palette[g] = Z_Malloc(playpal->length, PU_STATIC, NULL);

        for (size_t i = 0; i < playpal->length; i++)
        {
            byte b = playpal->data[i];
            playpal->palette[g][i] = gammatable[g][b];
        }
    }

    return playpal;
}

static void InitGlobalPlaypal(void)
{
    const char name[9] = "PLAYPAL";
    int playpal_iwad_num = -1;
    for (int i = 0; i < numlumps; i++)
    {
        if (strcasecmp(lumpinfo[i].name, name) == 0)
        {
            playpal_iwad_num = i;
            break;
        }
    }

    playpal_base = InitPlaypal(PAL_BASE, name, W_CheckNumForName(name));
    playpal_iwad = InitPlaypal(PAL_IWAD, name, playpal_iwad_num);

    playpal_global = playpal_base;
}

void V_ResetPalette(void)
{
    I_SetPalette(PAL_BASE, PAL_LAYER_BASE);
}

void V_SetCustomPalette(const char *name)
{
    playpal_global = InitPlaypal(PAL_CUSTOM, name, W_CheckNumForName(name));
    I_SetPalette(PAL_CUSTOM, PAL_LAYER_BASE);
}

void V_InitPalette(void)
{
    InitGlobalPlaypal();
    V_InitColorTranslation();
}
