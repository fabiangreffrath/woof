//
//  Copyright (C) 1999 by
//  id Software, Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman
//  Copyright(C) 2015-2020 Fabian Greffrath
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
//      support maps with NODES in uncompressed XNOD/XGLN or compressed
//      ZNOD/ZGLN formats, or DeePBSP format
//
//-----------------------------------------------------------------------------

#ifndef __P_EXTNODES__
#define __P_EXTNODES__

#include "doomtype.h"

struct vertex_s;

typedef enum
{
    MFMT_DOOM,
    MFMT_DEEP,
    MFMT_XNOD,
    MFMT_ZNOD,
    MFMT_XGLN,
    MFMT_ZGLN,
    MFMT_XGL2,
    MFMT_ZGL2,
    MFMT_XGL3,
    MFMT_ZGL3,

    MFMT_UNSUPPORTED = MFMT_XGL2
} mapformat_t;

extern mapformat_t P_CheckMapFormat(int lumpnum);
extern int P_GetOffset(struct vertex_s *v1, struct vertex_s *v2);

extern void P_LoadSegs_DEEP(int lump);
extern void P_LoadSubsectors_DEEP(int lump);
extern void P_LoadNodes_DEEP(int lump);
extern void P_LoadNodes_XNOD(int lump, boolean compressed, boolean glnodes);

#endif
