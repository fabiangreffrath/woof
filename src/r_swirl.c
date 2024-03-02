//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2000, 2005-2014 Simon Howard
// Copyright(C) 2019 Fabian Greffrath
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
// DESCRIPTION:
//	[crispy] add support for SMMU swirling flats
//

// [crispy] adapted from smmu/r_ripple.c, by Simon Howard

#include "doomstat.h"
#include "doomtype.h"
#include "i_system.h"
#include "m_fixed.h"
#include "tables.h"
#include "w_wad.h"
#include "z_zone.h"

// swirl factors determine the number of waves per flat width

// 1 cycle per 64 units
#define swirlfactor  (FINEANGLES / 64)

// 1 cycle per 32 units (2 in 64)
#define swirlfactor2 (FINEANGLES / 32)

#define SEQUENCE     256
#define FLATSIZE     (64 * 64)

static int *offsets = NULL;
static int *offset;

#define AMP   2
#define AMP2  2
#define SPEED 32

static void R_InitDistortedFlats()
{
    int i;

    offsets = I_Realloc(offsets, SEQUENCE * FLATSIZE * sizeof(*offsets));
    offset = offsets;

    for (i = 0; i < SEQUENCE; i++)
    {
        int x, y;

        for (x = 0; x < 64; x++)
        {
            for (y = 0; y < 64; y++)
            {
                int x1, y1;
                int sinvalue, sinvalue2;

                sinvalue = (y * swirlfactor + i * SPEED * 5 + 900) & FINEMASK;
                sinvalue2 = (x * swirlfactor2 + i * SPEED * 4 + 300) & FINEMASK;
                x1 = x + 128 + ((finesine[sinvalue] * AMP) >> FRACBITS)
                     + ((finesine[sinvalue2] * AMP2) >> FRACBITS);

                sinvalue = (x * swirlfactor + i * SPEED * 3 + 700) & FINEMASK;
                sinvalue2 = (y * swirlfactor2 + i * SPEED * 4 + 1200) & FINEMASK;
                y1 = y + 128 + ((finesine[sinvalue] * AMP) >> FRACBITS)
                     + ((finesine[sinvalue2] * AMP2) >> FRACBITS);

                x1 &= 63;
                y1 &= 63;

                offset[(y << 6) + x] = (y1 << 6) + x1;
            }
        }

        offset += FLATSIZE;
    }
}

byte *R_DistortedFlat(int flatnum)
{
    static int swirltic = -1;
    static int swirlflat = -1;
    static byte distortedflat[FLATSIZE];

    if (!offsets)
    {
        R_InitDistortedFlats();
    }

    if (swirltic != leveltime)
    {
        if (!frozen_mode)
        {
            offset = offsets + ((leveltime & (SEQUENCE - 1)) * FLATSIZE);
        }
        else
        {
            offset = offsets;
        }

        swirltic = leveltime;
        swirlflat = -1;
    }

    if (swirlflat != flatnum)
    {
        char *normalflat;
        int i;

        normalflat = W_CacheLumpNum(flatnum, PU_STATIC);

        for (i = 0; i < FLATSIZE; i++)
        {
            distortedflat[i] = normalflat[offset[i]];
        }

        Z_ChangeTag(normalflat, PU_CACHE);

        swirlflat = flatnum;
    }

    return distortedflat;
}
