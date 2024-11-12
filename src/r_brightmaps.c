//
// Copyright(C) 2024 Roman Fomin
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.

#include <stdlib.h>
#include <string.h>

#include "doomtype.h"
#include "info.h"
#include "m_misc.h"
#include "m_swap.h"
#include "r_defs.h"
#include "v_fmt.h"
#include "w_wad.h"
#include "z_zone.h"

#define MAX_FRAMES 29
#define MAX_ROTATIONS 9

boolean brightmaps;

static patch_t *spr_brightmaps[NUMSPRITES][MAX_FRAMES][MAX_ROTATIONS];

patch_t *R_GetBrightmap(int sprite, int frame, int rotation)
{
    if (!brightmaps)
    {
        return NULL;
    }

    patch_t *patch = spr_brightmaps[sprite][frame][rotation];
    return patch;
}

static void BrightmapToPatch(patch_t *patch, byte *image)
{
    int width = SHORT(patch->width);

    for (int x = 0; x < width; x++)
    {
        int offset = LONG(patch->columnofs[x]);
        byte *rover = (byte *)patch + offset;

        while (*rover != 0xff)
        {
            int y = *rover++;
            int count = *rover++;

            *rover++ = 0;

            while (count--)
            {
                *rover++ = image[y * width + x];
                ++y;
            }

            *rover++ = 0;
        }
    }
}

static boolean LoadBrightmap(int sprite, int frame, int rotation)
{
    spr_brightmaps[sprite][frame][rotation] = NULL;

    char frame_ch = 'A' + frame;

    if (frame_ch == '\\')
    {
        frame_ch = '^';
    }

    char lumpname[9] = {0};

    boolean flip = false;

    if (rotation == 0)
    {
        M_snprintf(lumpname, sizeof(lumpname), "%s%c0", sprnames[sprite], frame_ch);
    }
    else if (rotation == 1 || rotation == 5)
    {
        M_snprintf(lumpname, sizeof(lumpname), "%s%c%1d", sprnames[sprite], frame_ch, rotation);
    }
    else
    {
        flip = true;
        M_snprintf(lumpname, sizeof(lumpname), "%s%c%1d%c%1d",
            sprnames[sprite], frame_ch, rotation, frame_ch, 10 - rotation);
    }

    int lumpnum = (W_CheckNumForName)(lumpname, ns_brightmaps);

    if (lumpnum < 0)
    {
        return false;
    }

    byte *buffer = W_CacheLumpNum(lumpnum, PU_STATIC);
    int size = W_LumpLength(lumpnum);

    byte *image = V_DecodeBrightMap(buffer, size);

    Z_Free(buffer);

    if (!image)
    {
        return false;
    }

    lumpnum = (W_CheckNumForName)(lumpname, ns_sprites);

    if (lumpnum < 0)
    {
        free(image);
        return false;
    }

    patch_t *original = V_CachePatchNum(lumpnum, PU_STATIC);
    size = V_LumpSize(lumpnum);
    patch_t *patch = malloc(size);
    memcpy(patch, original, size);
    Z_Free(original);

    BrightmapToPatch(patch, image);

    free(image);

    spr_brightmaps[sprite][frame][rotation] = patch;

    if (flip)
    {
        spr_brightmaps[sprite][frame][10 - rotation] = patch;
    }

    return true;
}

void R_InitBrightmaps(void)
{
    for (int sprite = 0; sprite < NUMSPRITES; ++sprite)
    {
        for (int frame = 0; frame < MAX_FRAMES; ++frame)
        {
            for (int rotation = 0; rotation < MAX_ROTATIONS; ++rotation)
            {
                LoadBrightmap(sprite, frame, rotation);
            }
        }
    }
}
