//
//  Copyright (C) 2025 Guilherme Miranda
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
//  DESCRIPTION:
//    Universal Doom Map Format support.
//

#ifndef __P_UDMF__
#define __P_UDMF__

#include "p_extnodes.h"

//
// Universal Doom Map Format (UDMF) support
//

typedef enum
{
    UDMF_LABEL,
    UDMF_TEXTMAP,
    UDMF_ZNODES,
    UDMF_BLOCKMAP,
    UDMF_REJECT,
    UDMF_BEHAVIOR, // known of, but not supported
    UDMF_DIALOGUE, //
    UDMF_LIGHTMAP, //
    UDMF_ENDMAP,
} UDMF_Lumps_t;

typedef enum
{
    UDMF_BASE = (0),

    // Base games
    UDMF_DOOM = (1 << 0),
    UDMF_HERETIC = (1 << 1),
    UDMF_HEXEN = (1 << 2),
    UDMF_STRIFE = (1 << 3),

    // Doom extensions
    UDMF_BOOM = (1 << 4),
    UDMF_MBF = (1 << 5),
    UDMF_MBF21 = (1 << 6),
    UDMF_ID24 = (1 << 7),

    // Port exclusivity
    UDMF_WOOF = (1 << 31),
} UDMF_Features_t;

typedef struct
{
    // Base spec
    int id;
    int type;
    double x;
    double y;
    double height;
    int angle;
    int options;
} UDMF_Thing_t;

typedef struct
{
    // Base spec
    double x;
    double y;
} UDMF_Vertex_t;

typedef struct
{
    // Base spec
    int id;
    int v1_id;
    int v2_id;
    int special;
    int sidefront;
    int sideback;
    int flags;

    // Woof!
    char tranmap[9];
} UDMF_Linedef_t;

typedef struct
{
    // Base spec
    int sector_id;
    char texturetop[9];
    char texturemiddle[9];
    char texturebottom[9];
    double offsetx;
    double offsety;
} UDMF_Sidedef_t;

typedef struct
{
    // Base spec
    int tag;
    int heightfloor;
    int heightceiling;
    char texturefloor[9];
    char textureceiling[9];
    int lightlevel;
    int special;
} UDMF_Sector_t;

extern void UDMF_LoadMap(int lumpnum, nodeformat_t *nodeformat,
                         int *gen_blockmap, int *pad_reject);
extern void UDMF_LoadThings(void);

#endif
