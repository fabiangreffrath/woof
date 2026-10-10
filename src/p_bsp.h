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

#include "p_level.h"

struct vertex_s;

void P_InitSubsectorLines(void);
void P_CheckBSPFormat_Binary(map_t *map);
void P_CheckBSPFormat_UDMF(map_t *map);
int P_GetOffset(struct vertex_s *v1, struct vertex_s *v2);

void P_LoadSegs(map_t *map);
void P_LoadSubsectors(map_t *map);
void P_LoadNodes(map_t *map);
void P_LoadSegs_DeePBSPV4(map_t *map);
void P_LoadSubsectors_DeePBSPV4(map_t *map);
void P_LoadNodes_DeePBSPV4(map_t *map);
void P_LoadBSPTree_ZDBSP(map_t *map);

#endif
