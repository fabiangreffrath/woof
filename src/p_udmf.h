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

#include "p_level.h"
#include "r_defs.h"

typedef struct UDMF_Thing_s
{
    // Base spec
    int32_t tid;
    int32_t type;
    double x, y;
    double height;
    int32_t angle;
    int32_t options;
    int32_t special;
    int32_t args[5];

    // Extensions
    double health;
    char tranmap[9];
    double alpha;
    char tint[9];
} UDMF_Thing_t;

typedef struct UDMF_Vertex_s
{
    // Base spec
    double x;
    double y;
} UDMF_Vertex_t;

typedef struct UDMF_Linedef_s
{
    // Base spec
    int32_t id;
    int32_t v1_id, v2_id;
    int32_t special;
    int32_t args[5];
    int32_t sidefront, sideback;
    int32_t flags;

    // Extensions
    char tranmap[9];
    double alpha;
    amls_t amls;
} UDMF_Linedef_t;

// Important note about line tag/id/arg0, in the Doom/Heretic/Strife namespaces:
// The base UDMF spec makes a distinction between the value used to identify a
// specific line (id), and the value used when an action is executed (arg0),
// as opposed to the Doom map format, that used both as the same (tag).

typedef struct UDMF_Sidedef_s
{
    // Base spec
    int32_t sector_id;
    char texturetop[9];
    char texturemiddle[9];
    char texturebottom[9];
    int32_t offsetx, offsety;

    // Extensions
    int32_t flags;

    int32_t xscroll, yscroll;

    int32_t light;
    int32_t light_top;
    int32_t light_mid;
    int32_t light_bottom;

    char tint[9];

    double offsetx_top, offsety_top;
    double offsetx_mid, offsety_mid;
    double offsetx_bottom, offsety_bottom;

    double xscrolltop, yscrolltop;
    double xscrollmid, yscrollmid;
    double xscrollbottom, yscrollbottom;
} UDMF_Sidedef_t;

typedef struct UDMF_Sector_s
{
    // Base spec
    int32_t tag;
    int32_t heightfloor;
    int32_t heightceiling;
    char texturefloor[9];
    char textureceiling[9];
    int32_t lightlevel;
    int32_t special;

    // Extensions
    int32_t flags;

    int32_t lightfloor, lightceiling;

    char colormap[9];
    char tint[9], tintceiling[9], tintfloor[9];

    double xpanningfloor, ypanningfloor;
    double xpanningceiling, ypanningceiling;
    double rotationfloor, rotationceiling;

    double xscrollfloor, yscrollfloor;
    double xscrollceiling, yscrollceiling;
    int32_t scrollfloormode, scrollceilingmode;

    double scroll_floor_x, scroll_floor_y;
    double scroll_ceil_x, scroll_ceil_y;
    int32_t scroll_floor_type, scroll_ceil_type;
} UDMF_Sector_t;

void P_ClearMemory_UDMF(map_t *map);
void P_ParseTextMap(map_t *map);

#endif
