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

#include <stdlib.h>
#include <string.h>

#include "d_iwad.h"
#include "doomdef.h"
#include "doomstat.h"
#include "doomtype.h"
#include "i_exit.h"
#include "i_printf.h"
#include "i_video.h"
#include "m_argv.h"
#include "m_io.h"
#include "m_misc.h"
#include "md5.h"
#include "r_srgb.h"
#include "r_tranmap.h"
#include "w_wad.h"
#include "z_zone.h"

//
// R_InitTranMap
//
// Initialize translucency filter map
//
// By Lee Killough 2/21/98
//

static const int playpal_base_layer = 256 * 3;    // RGB triplets
static const int tranmap_lump_length = 256 * 256; // Plain RAW graphic
static const int default_tranmap_alpha = 66;      // Keep it simple, only do alpha of 66

static byte playpal_digest[16];
static char playpal_string[33];
static char *tranmap_dir, *playpal_dir;
static byte *normal_tranmap[100];

const byte *tranmap;      // translucency filter maps 256x256   // phares
const byte *main_tranmap; // killough 4/11/98
const byte *main_addimap; // Some things look better with added luminosity :)

//
// Blending algorthims!
//

enum
{
    r,
    g,
    b
};

//
// The heart of the calculation, the blending algorithm. Currently supported:
// * Normal -- applies standard alpha interpolation
// * Additive -- alpha is a foreground multiplier, added to unmodified background
//
// TODO, tentative additions:
// * Subtractive -- alpha is a foreground multiplier, subtracted from unmodifed background
//

typedef const byte (blendfunc_t)(const byte, const byte, const double);

inline static const byte BlendChannelNormal(const byte bg, const byte fg, const double a)
{
    const double fg_linear = byte_to_linear(fg);
    const double bg_linear = byte_to_linear(bg);
    const double r_linear = (fg_linear * a) + (bg_linear * (1.0 - a));
    return linear_to_byte(r_linear);
}

inline static const byte BlendChannelAdditive(const byte bg, const byte fg, const double a)
{
    const double fg_linear = byte_to_linear(fg);
    const double bg_linear = byte_to_linear(bg);
    const double r_linear = (fg_linear * a) + bg_linear;
    return linear_to_byte(r_linear);
}

inline static const byte ColorBlend(byte *playpal, blendfunc_t blendfunc,
                                    const byte *bg, const byte *fg,
                                    const double alpha)
{
    int blend[3] = {0};
    blend[r] = blendfunc(bg[r], fg[r], alpha);
    blend[g] = blendfunc(bg[g], fg[g], alpha);
    blend[b] = blendfunc(bg[b], fg[b], alpha);
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
    M_DigestToString(playpal_digest, playpal_string, sizeof(playpal_digest));
}

static void CreateTranMapBaseDir(void)
{
    const char *data_root = D_DoomPrefDir();
    const int length = strlen(data_root) + sizeof("/tranmaps");

    tranmap_dir = Z_Malloc(length, PU_STATIC, 0);
    M_snprintf(tranmap_dir, length, "%s/tranmaps", data_root);

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
    M_snprintf(playpal_dir, length, "%s/%s", tranmap_dir, playpal_string);

    M_MakeDirectory(playpal_dir);
}

//
// The heart of it all
//

static byte *GenerateTranmapData(blendfunc_t blendfunc, double alpha)
{
    byte *playpal = W_CacheLumpName("PLAYPAL", PU_STATIC);

    // killough 4/11/98
    byte *buffer = Z_Malloc(tranmap_lump_length, PU_STATIC, 0);
    byte *tp = buffer;

    // Background
    for (int i = 0; i < 256; i++)
    {
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
            const byte *bg = playpal + 3 * i;
            const byte *fg = playpal + 3 * j;

            *tp++ = ColorBlend(playpal, blendfunc, bg, fg, alpha);
        }
    }

    return buffer;
}

byte *R_NormalTranMap(int alpha, boolean force)
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
        M_snprintf(filename, length, "%s/tranmap_%02d.dat", playpal_dir, alpha);

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
            buffer = GenerateTranmapData(BlendChannelNormal, alpha/100.0);
            M_WriteFile(filename, buffer, tranmap_lump_length);
        }

        normal_tranmap[alpha] = buffer;
    }

    // Use cached translucency filter if it's available
    return normal_tranmap[alpha];
}

void R_InitTranMap(void)
{
    //!
    // @category mod
    //
    // Forces the (re-)building of the translucency table.
    //
    const int force_rebuild = M_CheckParm("-tranmap");
    const int lump = W_CheckNumForName("TRANMAP");

    if (lump != -1 && !force_rebuild)
    {
        main_tranmap = W_CacheLumpNum(lump, PU_STATIC);
    }
    else
    {
        main_tranmap = R_NormalTranMap(default_tranmap_alpha, true);
    }

    // Some things look better with added luminosity :)
    main_addimap = strictmode ? main_tranmap
                 : GenerateTranmapData(BlendChannelAdditive, 1.0);

    I_Printf(VB_INFO, "Playpal checksum: %s", playpal_string);

    //!
    // @category mod
    // @arg <alpha> <name>
    //
    // Dump tranmap lump, given an alpha level (opacity percentage).
    // Valid values are 0 through to 99.
    //
    const int p = M_CheckParmWithArgs("-dumptranmap", 2);
    if (p > 0)
    {
        const int alpha = CLAMP(M_ParmArgToInt(p), 0, 99);
        const byte *tranmap = R_NormalTranMap(alpha, true);
        char *path = AddDefaultExtension(myargv[p + 2], ".lmp");
        M_WriteFile(path, tranmap, tranmap_lump_length);
        free(path);

        I_SafeExit(0);
    }
}
