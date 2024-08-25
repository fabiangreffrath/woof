//
// Copyright (C) 2013 James Haley et al.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//

// haleyjd: DOSDoom-style single translucency lookup-up table
// generation code. This code has a 32k (plus a bit more)
// footprint but allows a much wider range of translucency effects
// than BOOM-style translucency. This will be used for particles,
// for variable mapthing trans levels, and for screen patches.

// haleyjd: Updated 06/21/08 to use 32k lookup, mainly to fix
// additive translucency.
// MaxW: As GZDoom is now GPLv3, this code is no longer dual-licenced:
//
// Copyright 1998-2012 (C) Marisa Heit
//

#include "v_flextran.h"

#include "i_video.h"
#include "w_wad.h"
#include "z_zone.h"

unsigned int Col2RGB8[65][256];
unsigned int *Col2RGB8_LessPrecision[65];
byte RGB32k[32][32][32];

static unsigned int Col2RGB8_2[63][256];

#define MAKECOLOR(a) (((a) << 3) | ((a) >> 2))

typedef struct
{
    unsigned int r, g, b;
} tpalcol_t;

void V_InitFlexTranTable(void)
{
    int i, r, g, b, x, y;
    tpalcol_t *tempRGBpal;
    const byte *palRover;

    byte *palette = W_CacheLumpName("PLAYPAL", PU_STATIC);

    tempRGBpal = Z_Malloc(256 * sizeof(*tempRGBpal), PU_STATIC, 0);

    for (i = 0, palRover = palette; i < 256; i++, palRover += 3)
    {
        tempRGBpal[i].r = palRover[0];
        tempRGBpal[i].g = palRover[1];
        tempRGBpal[i].b = palRover[2];
    }

    // build RGB table
    for (r = 0; r < 32; ++r)
    {
        for (g = 0; g < 32; ++g)
        {
            for (b = 0; b < 32; ++b)
            {
                RGB32k[r][g][b] = I_GetNearestColor(palette, MAKECOLOR(r),
                                                    MAKECOLOR(g), MAKECOLOR(b));
            }
        }
    }

    // build lookup table
    for (x = 0; x < 65; ++x)
    {
        for (y = 0; y < 256; ++y)
        {
            Col2RGB8[x][y] = (((tempRGBpal[y].r * x) >> 4) << 20)
                             | ((tempRGBpal[y].g * x) >> 4)
                             | (((tempRGBpal[y].b * x) >> 4) << 10);
        }
    }

    // build a secondary lookup with red and blue lsbs masked out for additive
    // blending; otherwise, the overflow messes up the calculation and you get
    // something very ugly.
    for (x = 1; x < 64; ++x)
    {
        Col2RGB8_LessPrecision[x] = Col2RGB8_2[x - 1];

        for (y = 0; y < 256; ++y)
        {
            Col2RGB8_2[x - 1][y] = Col2RGB8[x][y] & 0x3feffbff;
        }
    }
    Col2RGB8_LessPrecision[0] = Col2RGB8[0];
    Col2RGB8_LessPrecision[64] = Col2RGB8[64];

    Z_Free(tempRGBpal);
    Z_ChangeTag(palette, PU_CACHE);
}
