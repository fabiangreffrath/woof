//
//  Copyright (C) 1999 by
//   id Software, Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman
//  Copyright (C) 2023 by
//   Ryan Krafnick
//  Copyright (C) 2025 by
//   Guilherme Miranda
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
// DESCRIPTION:
//   Generation of transparency lookup tables.
//

#include <stdio.h>
#include <string.h>

#include "d_iwad.h"
#include "doomdef.h"
#include "doomstat.h"
#include "doomtype.h"
#include "i_printf.h"
#include "i_video.h"
#include "m_argv.h"
#include "m_io.h"
#include "m_misc.h"
#include "md5.h"
#include "r_data.h"
#include "r_tranmap.h"
#include "w_wad.h"

byte *tranmap;      // translucency filter maps 256x256   // phares
byte *main_tranmap; // killough 4/11/98
int32_t main_alpha;

//
// R_InitTranMap
//
// Initialize translucency filter map
//
// By Lee Killough 2/21/98
//

static const int playpal_base_layer = 256 * 3; // RGB triplets
static const int tranmap_lump_length = 256 * 256;
static const int default_tranmap_alpha = 66;
int tranmap_alpha = 66;

static byte playpal_digest[16];
static char playpal_string[33];
static char *tranmap_dir, *playpal_dir;
static byte *normal_tranmap[100];

//
// Blending algorthims!
//

enum
{
    r,
    g,
    b
};

#define ChannelBlend_Normal(blend, bg, fg, alpha) \
    (blend = ((alpha * fg) + ((100 - alpha) * bg)) / 100)

static const byte CrispyBlend_Normal(byte *playpal, const byte *bg,
                                     const byte *fg, const int alpha)
{
    int blend[3] = {0};
    ChannelBlend_Normal(blend[r], bg[r], fg[r], alpha);
    ChannelBlend_Normal(blend[g], bg[g], fg[g], alpha);
    ChannelBlend_Normal(blend[b], bg[b], fg[b], alpha);
    return I_GetNearestColor(playpal, blend[r], blend[g], blend[b]);
}

//
// Util functions to handle caching in the form of local tranmap file
//

static void CalculatePlaypalChecksum(void)
{
    const int lump = W_GetNumForName("PLAYPAL");
    struct MD5Context md5;

    MD5Init(&md5);
    MD5Update(&md5, W_CacheLumpNum(lump, PU_STATIC), playpal_base_layer);
    MD5Final(playpal_digest, &md5);

    for (int i = 0; i < sizeof(playpal_digest); ++i)
    {
        sprintf(&playpal_string[i * 2], "%02x", playpal_digest[i]);
    }
    playpal_string[32] = '\0';
}

static void CreateTranMapBaseDir(void)
{
    const char *data_root = D_DoomPrefDir();
    const int length = strlen(data_root) + sizeof("/tranmaps");

    tranmap_dir = Z_Malloc(length, PU_STATIC, 0);
    snprintf(tranmap_dir, length, "%s/tranmaps", data_root);

    M_MakeDirectory(tranmap_dir);
}

static void CreateTranMapPaletteDir(void)
{
    if (!tranmap_dir)
    {
        CreateTranMapBaseDir();
    }

    if (!playpal_string[0])
    {
        CalculatePlaypalChecksum();
    }

    int length = strlen(tranmap_dir) + sizeof(playpal_string) + 1;
    playpal_dir = Z_Malloc(length, PU_STATIC, 0);
    snprintf(playpal_dir, length, "%s/%s", tranmap_dir, playpal_string);

    M_MakeDirectory(playpal_dir);
}

//
// The heart of it all
//

static byte *GenerateNormalTranmapData(int alpha, boolean progress)
{
    byte *playpal = W_CacheLumpName("PLAYPAL", PU_STATIC);

    // killough 4/11/98
    byte *buffer = Z_Malloc(tranmap_lump_length, PU_STATIC, 0);
    byte *tp = buffer;

    // Background
    for (int i = 0; i < 256; i++)
    {
        const byte *bg = playpal + 3 * i;

        if (!(i & 31) && progress)
        {
            I_PutChar(VB_INFO, '.');
        }

        // killough 10/98: display flashing disk
        if (!(~i & 15))
        {
            if (i & 32)
            {
                I_EndRead();
            }
            else
            {
                I_BeginRead(DISK_ICON_THRESHOLD);
            }
        }

        // Foreground
        for (int j = 0; j < 256; j++)
        {
            // [crispy] shortcut: identical foreground and background
            if (i == j)
            {
                *tp++ = i;
                continue;
            }

            const byte *fg = playpal + 3 * j;

            *tp++ = CrispyBlend_Normal(playpal, bg, fg, alpha);
        }
    }

    // [FG] finish progress line
    if (progress)
    {
        I_PutChar(VB_INFO, '\n');
    }

    return buffer;
}

byte *R_NormalTranMap(int alpha, boolean progress, boolean force)
{
    if (alpha > 99)
    {
        return NULL;
    }

    if (force || !normal_tranmap[alpha])
    {
        if (!playpal_dir)
        {
            CreateTranMapPaletteDir();
        }

        const int length = strlen(playpal_dir) + sizeof("/tranmap_XY.dat");
        char *filename = Z_Malloc(length, PU_STATIC, 0);
        snprintf(filename, length, "%s/tranmap_%02d.dat", playpal_dir, alpha);

        byte *buffer = NULL;
        if (!force && M_FileExistsNotDir(filename))
        {
            const int file_length = M_ReadFile(filename, &buffer);
            if (buffer && file_length != tranmap_lump_length)
            {
                Z_Free(buffer);
                buffer = NULL;
            }
        }

        if (force || !buffer)
        {
            buffer = GenerateNormalTranmapData(alpha, progress);
            M_WriteFile(filename, buffer, tranmap_lump_length);
        }

        normal_tranmap[alpha] = buffer;
    }

    // Use cached translucency filter if it's available
    return normal_tranmap[alpha];
}

void R_InitTranMap(boolean progress)
{
    //!
    // @category mod
    //
    // Forces the (re-)building of the translucency table.
    //
    const int force_rebuild = M_CheckParm("-tranmap");

    //!
    // @category mod
    //
    // Dumps translucency tables for all alpha values (0-99)
    //
    const int build_all_alphas = M_CheckParm("-dumptranmap");

    const int lump = W_CheckNumForName("TRANMAP");

    if (build_all_alphas)
    {
        for (int alpha = 0; alpha < 100; ++alpha)
        {
            R_NormalTranMap(alpha, false, true);
        }
    }

    main_alpha = strictmode ? default_tranmap_alpha : tranmap_alpha;

    if (lump != -1 && !force_rebuild && !build_all_alphas)
    {
        main_tranmap = W_CacheLumpNum(lump, PU_STATIC); // killough 4/11/98
    }
    else
    {
        // Only do alpha of 66 in strictmode, also force rebuild
        main_tranmap = R_NormalTranMap(main_alpha, progress, strictmode);
        if (progress)
        {
            I_Printf(VB_INFO, "........");
        }
    }

    if (progress)
    {
        I_Printf(VB_DEBUG, "Playpal checksum: %s", playpal_string);
    }
}
