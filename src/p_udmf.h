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

#include "p_bsp.h"

typedef enum UDMF_Lumps_e
{
    UDMF_LABEL,
    UDMF_TEXTMAP,
    UDMF_ZNODES,
    UDMF_BLOCKMAP,
    UDMF_REJECT,
    UDMF_BEHAVIOR,
    UDMF_DIALOGUE,
    UDMF_LIGHTMAP,
    UDMF_ENDMAP,
    UDMF_MAXLUMP,
} UDMF_Lumps_t;

typedef struct udmf_lumpnums_s
{
    int znodes;
    int reject;
    int blockmap;
    int behavior;
    int dialogue;
    int lightmap;
} UDMF_Lumpnums_t;

extern void UDMF_ClearMemory(void);
extern void UDMF_LoadMap(int lumpnum, bspformat_t *nodeformat,
                         bmap_format_t *gen_blockmap, int *pad_reject);
extern void UDMF_LoadThings(void);
extern UDMF_Lumpnums_t UDMF_FindLumps(int lumpnum);

#endif
