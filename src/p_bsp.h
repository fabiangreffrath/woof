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
//      support extended nodes
//
//-----------------------------------------------------------------------------

#ifndef __P_BSP__
#define __P_BSP__

#include "doomdata.h"

struct vertex_s;

extern void P_CheckBSPFormat_Binary(map_t *map);
extern void P_CheckBSPFormat_UDMF(map_t *map);
extern int P_GetOffset(struct vertex_s *v1, struct vertex_s *v2);

extern void P_LoadSegs(int lump);
extern void P_LoadSubsectors(int lump);
extern void P_LoadNodes(int lump);
extern void P_LoadSegs_DeePBSPV4(int lump);
extern void P_LoadSubsectors_DeePBSPV4(int lump);
extern void P_LoadNodes_DeePBSPV4(int lump);
extern void P_LoadBSPTree_ZDBSP(int lump, bsp_format_t format);

#endif
