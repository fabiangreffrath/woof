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

#include "p_udmf.h"
#include "doomdata.h"
#include "doomdef.h"
#include "doomstat.h"
#include "doomtype.h"
#include "i_printf.h"
#include "i_system.h"
#include "m_argv.h"
#include "m_arena.h"
#include "m_array.h"
#include "m_fixed.h"
#include "m_misc.h"
#include "m_scanner.h"
#include "m_swap.h"
#include "p_extnodes.h"
#include "p_mobj.h"
#include "p_setup.h"
#include "p_spec.h"
#include "r_data.h"
#include "r_state.h"
#include "r_tranmap.h"
#include "tables.h"
#include "w_wad.h"
#include <math.h>

//
// Universal Doom Map Format (UDMF) support
//

typedef enum
{
    UDMF_BASE = (0),

    UDMF_THING_FRIEND  = (1u << 1), // Marine's Best Friend :)
    UDMF_THING_SPECIAL = (1u << 2), // Death/Pickup/etc-activated actions
    UDMF_THING_PARAM   = (1u << 3), // ditto, also customizes some MObjs
    UDMF_THING_ALPHA   = (1u << 4), // opacity percentage
    UDMF_THING_TRANMAP = (1u << 5), // ditto, also customizable LUT

    UDMF_LINE_PARAM    = (1u << 6), // Hexen-style param actions
    UDMF_LINE_PASSUSE  = (1u << 7), // Boom's "Pass Use Through" line flag
    UDMF_LINE_BLOCK    = (1u << 8), // MBF21's entity blocking flag
    UDMF_LINE_3DMIDTEX = (1u << 9), // EE's 3D middle texture
    UDMF_LINE_ALPHA    = (1u << 10), // opacity percentage
    UDMF_LINE_TRANMAP  = (1u << 11), // ditto, also customizable LUT

    UDMF_SIDE_OFFSET   = (1u << 12), // texture X/Y alignment
    UDMF_SIDE_SCROLL   = (1u << 13), // texture scrolling property
    UDMF_SIDE_LIGHT    = (1u << 14), // independent light levels

    UDMF_SEC_ANGLE     = (1u << 15), // plane rotation
    UDMF_SEC_OFFSET    = (1u << 16), // plane X/Y alignment
    UDMF_SEC_EE_SCROLL = (1u << 17), // EE's original plane scrolling property
    UDMF_SEC_SCROLL    = (1u << 18), // DSDA's latter plane scrolling property
    UDMF_SEC_LIGHT     = (1u << 19), // independent light levels

    // Compatibility
    UDMF_COMP_NO_ARG0 = (1u << 31),
} UDMF_Features_t;

typedef struct
{
    // Base spec
    int32_t id;
    int32_t type;
    double x, y;
    double height;
    int32_t angle;
    int32_t options;
    int32_t special;
    int32_t args[5];

    // Extensions
    char tranmap[9];
    double alpha;
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
    int32_t id;
    int32_t v1_id, v2_id;
    int32_t special;
    int32_t args[5];
    int32_t sidefront, sideback;
    int32_t flags;

    // Extensions
    char tranmap[9];
    double alpha;
} UDMF_Linedef_t;

// Important note about line tag/id/arg0, in the Doom/Heretic/Strife namespaces:
// The base UDMF spec makes a distinction between the value used to identify a
// specific line (id), and the value used when an action is executed (arg0),
// as opposed to the Doom map format, that used both as the same (tag).

typedef struct
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

    double offsetx_top,    offsety_top;
    double offsetx_mid,    offsety_mid;
    double offsetx_bottom, offsety_bottom;

    double xscrolltop,    yscrolltop;
    double xscrollmid,    yscrollmid;
    double xscrollbottom, yscrollbottom;
} UDMF_Sidedef_t;

typedef struct
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

    double xpanningfloor,   ypanningfloor;
    double xpanningceiling, ypanningceiling;
    double rotationfloor, rotationceiling;

    double xscrollfloor,   yscrollfloor;
    double xscrollceiling, yscrollceiling;
    int32_t scrollfloormode, scrollceilingmode;

    double scroll_floor_x, scroll_floor_y;
    double scroll_ceil_x,  scroll_ceil_y;
    int32_t scroll_floor_type, scroll_ceil_type;
} UDMF_Sector_t;

static char *const UDMF_Lumps[] = {
    [UDMF_LABEL] = "-",           [UDMF_TEXTMAP] = "TEXTMAP",
    [UDMF_ZNODES] = "ZNODES",     [UDMF_BLOCKMAP] = "BLOCKMAP",
    [UDMF_REJECT] = "REJECT",     [UDMF_BEHAVIOR] = "BEHAVIOR",
    [UDMF_DIALOGUE] = "DIALOGUE", [UDMF_LIGHTMAP] = "LIGHTMAP",
    [UDMF_ENDMAP] = "ENDMAP",
};

static UDMF_Features_t udmf_flags = UDMF_BASE;

static UDMF_Vertex_t *udmf_vertexes = NULL;
static UDMF_Linedef_t *udmf_linedefs = NULL;
static UDMF_Sidedef_t *udmf_sidedefs = NULL;
static UDMF_Sector_t *udmf_sectors = NULL;
static UDMF_Thing_t *udmf_things = NULL;

//
// UDMF parsing utils
//

// Retrieve plain integer
inline static int UDMF_ScanInt(scanner_t *s)
{
    int x = 0;
    SC_MustGetToken(s, '=');
    SC_MustGetToken(s, TK_IntConst);
    x = SC_GetNumber(s);
    SC_MustGetToken(s, ';');
    return x;
}

// Retrieve plain double
inline static double UDMF_ScanDouble(scanner_t *s)
{
    double x = 0;
    SC_MustGetToken(s, '=');
    SC_MustGetToken(s, TK_FloatConst);
    x = SC_GetDecimal(s);
    SC_MustGetToken(s, ';');
    return x;
}

// Sets provided flag on, if true
inline static int UDMF_ScanFlag(scanner_t *s, int f)
{
    int x = 0;
    SC_MustGetToken(s, '=');
    SC_MustGetToken(s, TK_BoolConst);
    if (SC_GetBoolean(s))
    {
        x |= f;
    }
    SC_MustGetToken(s, ';');
    return x;
}

// Retrieve plain string
inline static void UDMF_ScanLumpName(scanner_t *s, char *x)
{
    SC_MustGetToken(s, '=');
    SC_MustGetToken(s, TK_StringConst);
    M_CopyLumpName(x, SC_GetString(s));
    SC_MustGetToken(s, ';');
}

// Property is valid in all namespaces
#define BASE_PROP(keyword) (!strcasecmp(prop, #keyword))

// Property is valid in the current namespace
#define PROP(keyword, flags) \
    (!strcasecmp(prop, #keyword) && (udmf_flags & (flags)))

// Parse specific string properties
inline static int32_t UDMF_ScanSectorScroll(scanner_t *s)
{
    const char *buf;
    int32_t mode = 0;
    SC_MustGetToken(s, '=');
    SC_MustGetToken(s, TK_StringConst);
    buf = SC_GetString(s);
    if (!strcasecmp(buf, "visual"))
      mode = SCROLL_TEXTURE;
    else if (!strcasecmp(buf, "physical"))
      mode = SCROLL_CARRY;
    else if (!strcasecmp(buf, "both"))
      mode = SCROLL_ALL;
    SC_MustGetToken(s, ';');
    return mode;
}

// Skip unknown keyword
static inline void UDMF_SkipScan(scanner_t *s)
{
    if (SC_CheckToken(s, '='))
    {
        while (SC_TokensLeft(s))
        {
            if (SC_CheckToken(s, ';'))
            {
                break;
            }

            SC_GetNextToken(s, true);
        }
        return;
    }

    SC_MustGetToken(s, '{');
    int brace_count = 1;
    while (SC_TokensLeft(s))
    {
        if (SC_CheckToken(s, '}'))
        {
            --brace_count;
        }
        else if (SC_CheckToken(s, '{'))
        {
            ++brace_count;
        }
        if (!brace_count)
        {
            break;
        }
        SC_GetNextToken(s, true);
    }
}

// UDMF namespace
static void UDMF_ParseNamespace(scanner_t *s)
{
    SC_MustGetToken(s, '=');
    SC_MustGetToken(s, TK_StringConst);
    const char *name = SC_GetString(s);
    udmf_flags = UDMF_BASE;

    if (!strcasecmp(name, "doom"))
    {
        udmf_flags |= UDMF_LINE_PASSUSE | UDMF_THING_FRIEND;
    }
    else if (devparm && !strcasecmp(name, "dsda"))
    {
        I_Printf(VB_WARNING, "Loading development-only UDMF namespace: \"%s\"", name);
        udmf_flags |= UDMF_LINE_PASSUSE | UDMF_THING_FRIEND | UDMF_LINE_BLOCK;
        udmf_flags |= UDMF_LINE_PARAM | UDMF_LINE_3DMIDTEX;
        udmf_flags |= UDMF_THING_PARAM | UDMF_THING_ALPHA;
        udmf_flags |= UDMF_SIDE_OFFSET | UDMF_SIDE_SCROLL | UDMF_SIDE_LIGHT;
        udmf_flags |= UDMF_SEC_ANGLE | UDMF_SEC_OFFSET | UDMF_SEC_SCROLL | UDMF_SEC_LIGHT;
    }
    else
    {
        I_Error("Unknown UDMF namespace: \"%s\".", name);
    }

    SC_MustGetToken(s, ';');
}

//
// UDMF vertex pasring
//

static void UDMF_ParseVertex(scanner_t *s)
{
    UDMF_Vertex_t vertex = {0};

    SC_MustGetToken(s, '{');
    while (!SC_CheckToken(s, '}'))
    {
        SC_MustGetToken(s, TK_Identifier);
        const char *prop = SC_GetString(s);
        if (BASE_PROP(x))
        {
            vertex.x = UDMF_ScanDouble(s);
        }
        else if (BASE_PROP(y))
        {
            vertex.y = UDMF_ScanDouble(s);
        }
        else
        {
            UDMF_SkipScan(s);
        }
    }

    array_push(udmf_vertexes, vertex);
}

//
// UDMF linedef loading
//

static void UDMF_ParseLinedef(scanner_t *s)
{
    UDMF_Linedef_t line = {0};
    line.sideback = -1;
    line.tranmap[0] = '-';
    line.alpha = 1.0;

    SC_MustGetToken(s, '{');
    while (!SC_CheckToken(s, '}'))
    {
        SC_MustGetToken(s, TK_Identifier);
        const char *prop = SC_GetString(s);
        if (BASE_PROP(v1))
        {
            line.v1_id = UDMF_ScanInt(s);
        }
        else if (BASE_PROP(v2))
        {
            line.v2_id = UDMF_ScanInt(s);
        }
        else if (BASE_PROP(special))
        {
            line.special = UDMF_ScanInt(s);
        }
        else if (BASE_PROP(id))
        {
            line.id = UDMF_ScanInt(s);
        }
        else if (BASE_PROP(arg0))
        {
            // Tag -> id/arg0 split means arg0 is always enabled
            line.args[0] = UDMF_ScanInt(s);
        }
        else if (PROP(arg1, UDMF_LINE_PARAM))
        {
            line.args[1] = UDMF_ScanInt(s);
        }
        else if (PROP(arg2, UDMF_LINE_PARAM))
        {
            line.args[2] = UDMF_ScanInt(s);
        }
        else if (PROP(arg3, UDMF_LINE_PARAM))
        {
            line.args[3] = UDMF_ScanInt(s);
        }
        else if (PROP(arg4, UDMF_LINE_PARAM))
        {
            line.args[4] = UDMF_ScanInt(s);
        }
        else if (BASE_PROP(sidefront))
        {
            line.sidefront = UDMF_ScanInt(s);
        }
        else if (BASE_PROP(sideback))
        {
            line.sideback = UDMF_ScanInt(s);
        }
        else if (BASE_PROP(blocking))
        {
            line.flags |= UDMF_ScanFlag(s, ML_BLOCKING);
        }
        else if (BASE_PROP(blockmonsters))
        {
            line.flags |= UDMF_ScanFlag(s, ML_BLOCKMONSTERS);
        }
        else if (BASE_PROP(twosided))
        {
            line.flags |= UDMF_ScanFlag(s, ML_TWOSIDED);
        }
        else if (BASE_PROP(dontpegtop))
        {
            line.flags |= UDMF_ScanFlag(s, ML_DONTPEGTOP);
        }
        else if (BASE_PROP(dontpegbottom))
        {
            line.flags |= UDMF_ScanFlag(s, ML_DONTPEGBOTTOM);
        }
        else if (BASE_PROP(secret))
        {
            line.flags |= UDMF_ScanFlag(s, ML_SECRET);
        }
        else if (BASE_PROP(blocksound))
        {
            line.flags |= UDMF_ScanFlag(s, ML_SOUNDBLOCK);
        }
        else if (BASE_PROP(dontdraw))
        {
            line.flags |= UDMF_ScanFlag(s, ML_DONTDRAW);
        }
        else if (BASE_PROP(mapped))
        {
            line.flags |= UDMF_ScanFlag(s, ML_MAPPED);
        }
        else if (PROP(tranmap, UDMF_LINE_TRANMAP))
        {
            UDMF_ScanLumpName(s, line.tranmap);
        }
        else if (PROP(passuse, UDMF_LINE_PASSUSE))
        {
            line.flags |= UDMF_ScanFlag(s, ML_PASSUSE);
        }
        else if (PROP(blocklandmonsters, UDMF_LINE_BLOCK))
        {
            line.flags |= UDMF_ScanFlag(s, ML_BLOCKLANDMONSTERS);
        }
        else if (PROP(blockplayers, UDMF_LINE_BLOCK))
        {
            line.flags |= UDMF_ScanFlag(s, ML_BLOCKPLAYERS);
        }
        else if (PROP(midtex3d, UDMF_LINE_3DMIDTEX))
        {
            line.flags |= UDMF_ScanFlag(s, ML_3DMIDTEX);
        }
        else if (PROP(alpha, UDMF_THING_ALPHA))
        {
            line.alpha = UDMF_ScanDouble(s);
        }
        else
        {
            UDMF_SkipScan(s);
        }
    }
    array_push(udmf_linedefs, line);
}

//
// UDMF sidedef parsing
//

static void UDMF_ParseSidedef(scanner_t *s)
{
    UDMF_Sidedef_t side = {0};
    M_CopyLumpName(side.texturetop, "-");
    M_CopyLumpName(side.texturemiddle, "-");
    M_CopyLumpName(side.texturebottom, "-");

    SC_MustGetToken(s, '{');
    while (!SC_CheckToken(s, '}'))
    {
        SC_MustGetToken(s, TK_Identifier);
        const char *prop = SC_GetString(s);
        if (BASE_PROP(offsetx))
        {
            side.offsetx = UDMF_ScanInt(s);
        }
        else if (BASE_PROP(offsety))
        {
            side.offsety = UDMF_ScanInt(s);
        }
        else if (BASE_PROP(sector))
        {
            side.sector_id = UDMF_ScanInt(s);
        }
        else if (BASE_PROP(texturetop))
        {
            UDMF_ScanLumpName(s, side.texturetop);
        }
        else if (BASE_PROP(texturemiddle))
        {
            UDMF_ScanLumpName(s, side.texturemiddle);
        }
        else if (BASE_PROP(texturebottom))
        {
            UDMF_ScanLumpName(s, side.texturebottom);
        }
        else if (PROP(light, UDMF_SIDE_LIGHT))
        {
            side.light = UDMF_ScanInt(s);
        }
        else if (PROP(light_top, UDMF_SIDE_LIGHT))
        {
            side.light_top = UDMF_ScanInt(s);
        }
        else if (PROP(light_mid, UDMF_SIDE_LIGHT))
        {
            side.light_mid = UDMF_ScanInt(s);
        }
        else if (PROP(light_bottom, UDMF_SIDE_LIGHT))
        {
            side.light_bottom = UDMF_ScanInt(s);
        }
        else if (PROP(lightabsolute, UDMF_SIDE_LIGHT))
        {
            side.flags |= UDMF_ScanFlag(s, SF_ABS_LIGHT);
        }
        else if (PROP(lightabsolute_top, UDMF_SIDE_LIGHT))
        {
            side.flags |= UDMF_ScanFlag(s, SF_ABS_LIGHT_TOP);
        }
        else if (PROP(lightabsolute_mid, UDMF_SIDE_LIGHT))
        {
            side.flags |= UDMF_ScanFlag(s, SF_ABS_LIGHT_MID);
        }
        else if (PROP(lightabsolute_bottom, UDMF_SIDE_LIGHT))
        {
            side.flags |= UDMF_ScanFlag(s, SF_ABS_LIGHT_BOTTOM);
        }
        else if (PROP(nofakecontrast, UDMF_SIDE_LIGHT))
        {
            side.flags |= UDMF_ScanFlag(s, SF_NO_FAKE_CONTRAST);
        }
        else if (PROP(smoothfakecontrast, UDMF_SIDE_LIGHT))
        {
            side.flags |= UDMF_ScanFlag(s, SF_SMOOTH_CONTRAST);
        }
        else if (PROP(offsetx_top, UDMF_SIDE_OFFSET))
        {
            side.offsetx_top = UDMF_ScanDouble(s);
        }
        else if (PROP(offsety_top, UDMF_SIDE_OFFSET))
        {
            side.offsety_top = UDMF_ScanDouble(s);
        }
        else if (PROP(offsetx_mid, UDMF_SIDE_OFFSET))
        {
            side.offsetx_mid = UDMF_ScanDouble(s);
        }
        else if (PROP(offsety_mid, UDMF_SIDE_OFFSET))
        {
            side.offsety_mid = UDMF_ScanDouble(s);
        }
        else if (PROP(offsetx_bottom, UDMF_SIDE_OFFSET))
        {
            side.offsetx_bottom = UDMF_ScanDouble(s);
        }
        else if (PROP(offsety_bottom, UDMF_SIDE_OFFSET))
        {
            side.offsety_bottom = UDMF_ScanDouble(s);
        }
        else if (PROP(xscroll, UDMF_SIDE_SCROLL))
        {
            side.xscroll = UDMF_ScanInt(s);
        }
        else if (PROP(yscroll, UDMF_SIDE_SCROLL))
        {
            side.yscroll = UDMF_ScanInt(s);
        }
        else if (PROP(xscrolltop, UDMF_SIDE_SCROLL))
        {
            side.xscrolltop = UDMF_ScanDouble(s);
        }
        else if (PROP(yscrolltop, UDMF_SIDE_SCROLL))
        {
            side.yscrolltop = UDMF_ScanDouble(s);
        }
        else if (PROP(xscrollmid, UDMF_SIDE_SCROLL))
        {
            side.xscrollmid = UDMF_ScanDouble(s);
        }
        else if (PROP(yscrollmid, UDMF_SIDE_SCROLL))
        {
            side.yscrollmid = UDMF_ScanDouble(s);
        }
        else if (PROP(xscrollbottom, UDMF_SIDE_SCROLL))
        {
            side.xscrollbottom = UDMF_ScanDouble(s);
        }
        else if (PROP(yscrollbottom, UDMF_SIDE_SCROLL))
        {
            side.yscrollbottom = UDMF_ScanDouble(s);
        }
        else
        {
            UDMF_SkipScan(s);
        }
    }

    array_push(udmf_sidedefs, side);
}

//
// UDMF sector parsing
//

static void UDMF_ParseSector(scanner_t *s)
{
    UDMF_Sector_t sector = {0};
    sector.lightlevel = 160;
    M_CopyLumpName(sector.texturefloor, "-");
    M_CopyLumpName(sector.textureceiling, "-");

    SC_MustGetToken(s, '{');
    while (!SC_CheckToken(s, '}'))
    {
        SC_MustGetToken(s, TK_Identifier);
        const char *prop = SC_GetString(s);
        if (BASE_PROP(heightfloor))
        {
            sector.heightfloor = UDMF_ScanInt(s);
        }
        else if (BASE_PROP(heightceiling))
        {
            sector.heightceiling = UDMF_ScanInt(s);
        }
        else if (BASE_PROP(texturefloor))
        {
            UDMF_ScanLumpName(s, sector.texturefloor);
        }
        else if (BASE_PROP(textureceiling))
        {
            UDMF_ScanLumpName(s, sector.textureceiling);
        }
        else if (BASE_PROP(lightlevel))
        {
            sector.lightlevel = UDMF_ScanInt(s);
        }
        else if (BASE_PROP(special))
        {
            sector.special = UDMF_ScanInt(s);
        }
        else if (BASE_PROP(id))
        {
            sector.tag = UDMF_ScanInt(s);
        }
        else if (PROP(rotationfloor, UDMF_SEC_ANGLE))
        {
            sector.rotationfloor = UDMF_ScanDouble(s);
        }
        else if (PROP(rotationceiling, UDMF_SEC_ANGLE))
        {
            sector.rotationceiling = UDMF_ScanDouble(s);
        }
        else if (PROP(xpanningfloor, UDMF_SEC_OFFSET))
        {
            sector.xpanningfloor = UDMF_ScanDouble(s);
        }
        else if (PROP(ypanningfloor, UDMF_SEC_OFFSET))
        {
            sector.ypanningfloor = UDMF_ScanDouble(s);
        }
        else if (PROP(xpanningceiling, UDMF_SEC_OFFSET))
        {
            sector.xpanningceiling = UDMF_ScanDouble(s);
        }
        else if (PROP(ypanningceiling, UDMF_SEC_OFFSET))
        {
            sector.ypanningceiling = UDMF_ScanDouble(s);
        }
        else if (PROP(scroll_floor_x, UDMF_SEC_EE_SCROLL))
        {
            sector.scroll_floor_x = UDMF_ScanDouble(s);
        }
        else if (PROP(scroll_floor_, UDMF_SEC_EE_SCROLL))
        {
            sector.scroll_floor_y = UDMF_ScanDouble(s);
        }
        else if (PROP(scroll_floor_type, UDMF_SEC_EE_SCROLL))
        {
            sector.scroll_floor_type = UDMF_ScanSectorScroll(s);
        }
        else if (PROP(scroll_ceil_x, UDMF_SEC_EE_SCROLL))
        {
            sector.scroll_ceil_x = UDMF_ScanDouble(s);
        }
        else if (PROP(scroll_ceil_y, UDMF_SEC_EE_SCROLL))
        {
            sector.scroll_ceil_y = UDMF_ScanDouble(s);
        }
        else if (PROP(scroll_ceil_type, UDMF_SEC_EE_SCROLL))
        {
            sector.scroll_ceil_type = UDMF_ScanSectorScroll(s);
        }
        else if (PROP(xscrollfloor, UDMF_SEC_SCROLL))
        {
            sector.xscrollfloor = UDMF_ScanDouble(s);
        }
        else if (PROP(yscrollfloor, UDMF_SEC_SCROLL))
        {
            sector.yscrollfloor = UDMF_ScanDouble(s);
        }
        else if (PROP(xscrollceiling, UDMF_SEC_SCROLL))
        {
            sector.xscrollceiling = UDMF_ScanDouble(s);
        }
        else if (PROP(yscrollceiling, UDMF_SEC_SCROLL))
        {
            sector.yscrollceiling = UDMF_ScanDouble(s);
        }
        else if (PROP(scrollfloormode, UDMF_SEC_SCROLL))
        {
            sector.scrollfloormode = UDMF_ScanInt(s);
        }
        else if (PROP(scrollceilingmode, UDMF_SEC_SCROLL))
        {
            sector.scrollceilingmode = UDMF_ScanInt(s);
        }
        else if (PROP(lightfloor, UDMF_SEC_LIGHT))
        {
            sector.lightfloor = UDMF_ScanInt(s);
        }
        else if (PROP(lightceiling, UDMF_SEC_LIGHT))
        {
            sector.lightceiling = UDMF_ScanInt(s);
        }
        else if (PROP(lightfloorabsolute, UDMF_SEC_LIGHT))
        {
            sector.flags |= UDMF_ScanFlag(s, SECF_ABS_LIGHT_FLOOR);
        }
        else if (PROP(lightceilingabsolute, UDMF_SEC_LIGHT))
        {
            sector.flags |= UDMF_ScanFlag(s, SECF_ABS_LIGHT_CEIL);
        }
        else
        {
            UDMF_SkipScan(s);
        }
    }

    array_push(udmf_sectors, sector);
}

//
// UDMF thing loading
//

static void UDMF_ParseThing(scanner_t *s)
{
    UDMF_Thing_t thing = {0};
    thing.options |= MTF_NOTSINGLE | MTF_NOTCOOP | MTF_NOTDM;
    thing.tranmap[0] = '-';
    thing.alpha = 1.0;

    SC_MustGetToken(s, '{');
    while (!SC_CheckToken(s, '}'))
    {
        SC_MustGetToken(s, TK_Identifier);
        const char *prop = SC_GetString(s);
        if (BASE_PROP(type))
        {
            thing.type = UDMF_ScanInt(s);
        }
        else if (PROP(id, UDMF_THING_PARAM))
        {
            thing.id = UDMF_ScanInt(s);
        }
        else if (BASE_PROP(x))
        {
            thing.x = UDMF_ScanDouble(s);
        }
        else if (BASE_PROP(y))
        {
            thing.y = UDMF_ScanDouble(s);
        }
        else if (BASE_PROP(height))
        {
            thing.height = UDMF_ScanDouble(s);
        }
        else if (BASE_PROP(angle))
        {
            thing.angle = UDMF_ScanInt(s);
        }
        else if (BASE_PROP(skill1))
        {
            thing.options |= UDMF_ScanFlag(s, MTF_SKILL1);
        }
        else if (BASE_PROP(skill2))
        {
            thing.options |= UDMF_ScanFlag(s, MTF_SKILL2);
        }
        else if (BASE_PROP(skill3))
        {
            thing.options |= UDMF_ScanFlag(s, MTF_SKILL3);
        }
        else if (BASE_PROP(skill4))
        {
            thing.options |= UDMF_ScanFlag(s, MTF_SKILL4);
        }
        else if (BASE_PROP(skill5))
        {
            thing.options |= UDMF_ScanFlag(s, MTF_SKILL5);
        }
        else if (BASE_PROP(ambush))
        {
            thing.options |= UDMF_ScanFlag(s, MTF_AMBUSH);
        }
        else if (BASE_PROP(single))
        {
            thing.options &= ~UDMF_ScanFlag(s, MTF_NOTSINGLE);
        }
        else if (BASE_PROP(dm))
        {
            thing.options &= ~UDMF_ScanFlag(s, MTF_NOTDM);
        }
        else if (BASE_PROP(coop))
        {
            thing.options &= ~UDMF_ScanFlag(s, MTF_NOTCOOP);
        }
        else if (PROP(friend, UDMF_THING_FRIEND))
        {
            thing.options |= UDMF_ScanFlag(s, MTF_FRIEND);
        }
        else if (PROP(special, UDMF_THING_SPECIAL))
        {
            thing.special = UDMF_ScanInt(s);
        }
        else if (PROP(arg0, UDMF_THING_SPECIAL|UDMF_THING_PARAM))
        {
            thing.args[0] = UDMF_ScanInt(s);
        }
        else if (PROP(arg1, UDMF_THING_PARAM))
        {
            thing.args[1] = UDMF_ScanInt(s);
        }
        else if (PROP(arg2, UDMF_THING_PARAM))
        {
            thing.args[2] = UDMF_ScanInt(s);
        }
        else if (PROP(arg3, UDMF_THING_PARAM))
        {
            thing.args[3] = UDMF_ScanInt(s);
        }
        else if (PROP(arg4, UDMF_THING_PARAM))
        {
            thing.args[4] = UDMF_ScanInt(s);
        }
        else if (PROP(alpha, UDMF_THING_ALPHA))
        {
            thing.alpha = UDMF_ScanDouble(s);
        }
        else
        {
            UDMF_SkipScan(s);
        }
    }

    array_push(udmf_things, thing);
}

//
// UDMF textmap loading
//

static void UDMF_ParseTextMap(int lumpnum)
{
    scanner_t *s =
        SC_Open("TEXTMAP", W_CacheLumpNum(lumpnum + UDMF_TEXTMAP, PU_CACHE),
                W_LumpLength(lumpnum + UDMF_TEXTMAP));

    const char *toplevel = NULL;
    while (SC_TokensLeft(s))
    {
        SC_MustGetToken(s, TK_Identifier);
        toplevel = SC_GetString(s);

        if (!strcasecmp(toplevel, "namespace"))
        {
            UDMF_ParseNamespace(s);
        }
        else if (!strcasecmp(toplevel, "vertex"))
        {
            UDMF_ParseVertex(s);
        }
        else if (!strcasecmp(toplevel, "linedef"))
        {
            UDMF_ParseLinedef(s);
        }
        else if (!strcasecmp(toplevel, "sidedef"))
        {
            UDMF_ParseSidedef(s);
        }
        else if (!strcasecmp(toplevel, "sector"))
        {
            UDMF_ParseSector(s);
        }
        else if (!strcasecmp(toplevel, "thing"))
        {
            UDMF_ParseThing(s);
        }
        else
        {
            UDMF_SkipScan(s);
        }
    }

    if (array_size(udmf_vertexes) == 0 || array_size(udmf_linedefs) == 0
        || array_size(udmf_sidedefs) == 0 || array_size(udmf_sectors) == 0
        || array_size(udmf_things) == 0)
    {
        SC_Error(s, "Not enough UDMF data. Check your TEXTMAP.");
    }

    SC_Close(s);
}

static void UDMF_LoadVertexes(void)
{
    numvertexes = array_size(udmf_vertexes);
    vertexes = arena_alloc_num(world_arena, vertex_t, numvertexes);
    memset(vertexes, 0, numvertexes * sizeof(vertex_t));

    for (int i = 0; i < numvertexes; i++)
    {
        vertexes[i].x = DoubleToFixed(udmf_vertexes[i].x);
        vertexes[i].y = DoubleToFixed(udmf_vertexes[i].y);

        vertexes[i].r_x = vertexes[i].x;
        vertexes[i].r_y = vertexes[i].y;
    }
}

static void UDMF_LoadSectors(void)
{
    numsectors = array_size(udmf_sectors);
    sectors = arena_alloc_num(world_arena, sector_t, numsectors);
    memset(sectors, 0, numsectors * sizeof(sector_t));

    for (int i = 0; i < numsectors; i++)
    {
        sectors[i].floorheight = IntToFixed(udmf_sectors[i].heightfloor);
        sectors[i].ceilingheight = IntToFixed(udmf_sectors[i].heightceiling);
        sectors[i].floorpic = R_FlatNumForName(udmf_sectors[i].texturefloor);
        sectors[i].ceilingpic = R_FlatNumForName(udmf_sectors[i].textureceiling);
        sectors[i].lightlevel = udmf_sectors[i].lightlevel;
        sectors[i].tag = udmf_sectors[i].tag;

        sectors[i].flags = udmf_sectors[i].flags;
        sectors[i].lightfloor = udmf_sectors[i].lightfloor;
        sectors[i].lightceiling = udmf_sectors[i].lightceiling;

        sectors[i].floor_rotation =
            FixedToAngle(DoubleToFixed(udmf_sectors[i].rotationfloor));
        sectors[i].ceiling_rotation =
            FixedToAngle(DoubleToFixed(udmf_sectors[i].rotationceiling));

        sectors[i].floor_xoffs = DoubleToFixed(udmf_sectors[i].xpanningfloor);
        sectors[i].floor_yoffs = DoubleToFixed(udmf_sectors[i].ypanningfloor);
        sectors[i].ceiling_xoffs = DoubleToFixed(udmf_sectors[i].xpanningceiling);
        sectors[i].ceiling_yoffs = DoubleToFixed(udmf_sectors[i].ypanningceiling);

        if (udmf_sectors[i].scroll_floor_type && (udmf_sectors[i].scroll_floor_x || udmf_sectors[i].scroll_floor_y))
        {
            Add_EESectorScroller(udmf_sectors[i].scroll_floor_type, i, false,
                                 udmf_sectors[i].scroll_floor_x,
                                 udmf_sectors[i].scroll_floor_y);
        }

        if (udmf_sectors[i].scroll_ceil_type && (udmf_sectors[i].scroll_ceil_x || udmf_sectors[i].scroll_ceil_y))
        {
            Add_EESectorScroller(udmf_sectors[i].scroll_floor_type, i, true,
                                 udmf_sectors[i].scroll_floor_x,
                                 udmf_sectors[i].scroll_floor_y);
        }

        if (udmf_sectors[i].scrollfloormode && (udmf_sectors[i].xscrollfloor || udmf_sectors[i].yscrollfloor))
        {
            Add_ParamSectorScroller(
                udmf_sectors[i].scrollfloormode, i, false,
                DoubleToFixed(udmf_sectors[i].xscrollfloor),
                DoubleToFixed(udmf_sectors[i].yscrollfloor));
        }

        if (udmf_sectors[i].scrollceilingmode && (udmf_sectors[i].xscrollceiling || udmf_sectors[i].yscrollceiling))
        {
            Add_ParamSectorScroller(
                udmf_sectors[i].scrollceilingmode, i, true,
                DoubleToFixed(udmf_sectors[i].xscrollceiling),
                DoubleToFixed(udmf_sectors[i].yscrollceiling));
        }

        P_SectorInit(&sectors[i]);
    }
}

static void UDMF_LoadSideDefs(void)
{
    numsides = array_size(udmf_sidedefs);
    sides = arena_alloc_num(world_arena, side_t, numsides);
    memset(sides, 0, numsides * sizeof(side_t));

    for (int i = 0; i < numsides; i++)
    {
        sides[i].sector = &sectors[udmf_sidedefs[i].sector_id];
        sides[i].textureoffset = IntToFixed(udmf_sidedefs[i].offsetx);
        sides[i].rowoffset = IntToFixed(udmf_sidedefs[i].offsety);

        sides[i].offsetx_top = DoubleToFixed(udmf_sidedefs[i].offsetx_top);
        sides[i].offsety_top = DoubleToFixed(udmf_sidedefs[i].offsety_top);
        sides[i].offsetx_mid = DoubleToFixed(udmf_sidedefs[i].offsetx_mid);
        sides[i].offsety_mid = DoubleToFixed(udmf_sidedefs[i].offsety_mid);
        sides[i].offsetx_bottom = DoubleToFixed(udmf_sidedefs[i].offsetx_bottom);
        sides[i].offsety_bottom = DoubleToFixed(udmf_sidedefs[i].offsety_bottom);

        sides[i].flags = udmf_sidedefs[i].flags;
        sides[i].light = udmf_sidedefs[i].light;
        sides[i].light_top = udmf_sidedefs[i].light_top;
        sides[i].light_mid = udmf_sidedefs[i].light_mid;
        sides[i].light_bottom = udmf_sidedefs[i].light_bottom;

        if (udmf_sidedefs[i].xscroll || udmf_sidedefs[i].yscroll)
        {
            Add_ScrollerStatic(sc_side, i,
                               DoubleToFixed(udmf_sidedefs[i].xscroll),
                               DoubleToFixed(udmf_sidedefs[i].yscroll));
        }

        if (udmf_sidedefs[i].xscrolltop || udmf_sidedefs[i].yscrolltop)
        {
            Add_ScrollerStatic(sc_side_top, i,
                               DoubleToFixed(udmf_sidedefs[i].xscrolltop),
                               DoubleToFixed(udmf_sidedefs[i].yscrolltop));
        }

        if (udmf_sidedefs[i].xscrollmid || udmf_sidedefs[i].yscrollmid)
        {
            Add_ScrollerStatic(sc_side_mid, i,
                               DoubleToFixed(udmf_sidedefs[i].xscrollmid),
                               DoubleToFixed(udmf_sidedefs[i].yscrollmid));
        }

        if (udmf_sidedefs[i].xscrollbottom || udmf_sidedefs[i].yscrollbottom)
        {
            Add_ScrollerStatic(sc_side_bottom, i,
                               DoubleToFixed(udmf_sidedefs[i].xscrollbottom),
                               DoubleToFixed(udmf_sidedefs[i].yscrollbottom));
        }
        P_SidedefInit(&sides[i]);
    }
}

static void UDMF_LoadLineDefs(void)
{
    numlines = array_size(udmf_linedefs);
    lines = arena_alloc_num(world_arena, line_t, numlines);
    memset(lines, 0, numlines * sizeof(line_t));

    for (int i = 0; i < numlines; i++)
    {
        lines[i].v1 = &vertexes[udmf_linedefs[i].v1_id];
        lines[i].v2 = &vertexes[udmf_linedefs[i].v2_id];
        lines[i].sidenum[0] = udmf_linedefs[i].sidefront;
        lines[i].sidenum[1] = udmf_linedefs[i].sideback;

        lines[i].flags = udmf_linedefs[i].flags;
        lines[i].special = udmf_linedefs[i].special;
        lines[i].id = udmf_linedefs[i].id;
        lines[i].args[0] = udmf_linedefs[i].args[0];
        lines[i].args[1] = udmf_linedefs[i].args[1];
        lines[i].args[2] = udmf_linedefs[i].args[2];
        lines[i].args[3] = udmf_linedefs[i].args[3];
        lines[i].args[4] = udmf_linedefs[i].args[4];

        // Woof! currently does not support parameterized line specials
        if (udmf_flags & UDMF_LINE_PARAM)
        {
            udmf_linedefs[i].special = 0;
        }

        // Support for namespaces that do not make the tag -> arg0/id split
        if (udmf_flags & UDMF_COMP_NO_ARG0)
        {
            lines[i].args[0] = lines[i].id;
        }

        P_LinedefInit(&lines[i]);

        if (udmf_linedefs[i].alpha < 1.0)
        {
          const int32_t alpha = (int32_t)floorf(udmf_linedefs[i].alpha * 100.0);
          lines[i].tranmap = GetNormalTranMap(alpha);
        }

        int32_t lump = W_CheckNumForName(udmf_linedefs[i].tranmap);
        if (lump >= 0 && W_LumpLength(lump) == 256 * 256)
        {
            lines[i].tranmap = W_CacheLumpNum(lump, PU_CACHE);
        }

        // killough 11/98: fix common wad errors (missing sidedefs):
        // Substitute dummy sidedef for missing right side
        if (lines[i].sidenum[0] == NO_INDEX)
        {
            lines[i].sidenum[0] = 0;
        }

        // Clear 2s flag for missing left side
        if (lines[i].sidenum[1] == NO_INDEX && !demo_compatibility)
        {
            lines[i].flags &= ~ML_TWOSIDED;
        }

        if (lines[i].sidenum[0] != NO_INDEX)
        {
            side_t *frontside = &sides[lines[i].sidenum[0]];
            lines[i].frontsector = frontside->sector;
            frontside->special = lines[i].special;
        }

        if (lines[i].sidenum[1] != NO_INDEX)
        {
            side_t *backside = &sides[lines[i].sidenum[1]];
            lines[i].backsector = backside->sector;
        }
    }
}

static void UDMF_LoadSideDefs_Post(void)
{
    for (int i = 0; i < numsides; i++)
    {
        P_ProcessSideDefs(&sides[i], i, udmf_sidedefs[i].texturebottom,
                          udmf_sidedefs[i].texturemiddle,
                          udmf_sidedefs[i].texturetop);
    }
}

static void UDMF_LoadLineDefs_Post(void)
{
    for (int i = 0; i < numlines; i++)
    {
        // killough 4/11/98: handle special types
        switch (lines[i].special)
        {
            // killough 4/11/98: translucent 2s textures
            case 260:
            {
                // translucency from sidedef
                int32_t lump = sides[*lines[i].sidenum].special;
                const byte *tranmap =
                    !lump ? main_tranmap : W_CacheLumpNum(lump - 1, PU_STATIC);
                if (!lines[i].args[0])
                {
                    // if tag==0, affect this linedef only
                    lines[i].tranmap = tranmap;
                }
                else
                {
                    for (int j = 0; j < numlines; j++)
                    {
                        if (lines[j].id == lines[i].args[0])
                        {
                            // if tag!=0, affect all matching linedefs
                            lines[i].tranmap = tranmap;
                        }
                    }
                }
                break;
            }
        }
    }
}

void UDMF_LoadThings(void)
{
    for (int i = 0; i < array_size(udmf_things); i++)
    {
        // Do not spawn cool, new monsters if !commercial
        if (gamemode != commercial)
        {
            switch (udmf_things[i].type)
            {
                case 68: // Arachnotron
                case 64: // Archvile
                case 88: // Boss Brain
                case 89: // Boss Shooter
                case 69: // Hell Knight
                case 67: // Mancubus
                case 71: // Pain Elemental
                case 65: // Former Human Commando
                case 66: // Revenant
                case 84: // Wolf SS
                    continue;
            }
        }

        // Do spawn all other stuff.

        mapthing_t mt = {0};
        mt.x = DoubleToFixed(udmf_things[i].x);
        mt.y = DoubleToFixed(udmf_things[i].y);
        mt.height = DoubleToFixed(udmf_things[i].height);
        mt.angle = CLAMP(udmf_things[i].angle, 0, 360);
        mt.type = udmf_things[i].type;
        mt.options = udmf_things[i].options;

        mt.id = udmf_things[i].id;
        mt.special = udmf_things[i].special;
        mt.args[0] = udmf_things[i].args[0];
        mt.args[1] = udmf_things[i].args[1];
        mt.args[2] = udmf_things[i].args[2];
        mt.args[3] = udmf_things[i].args[3];
        mt.args[4] = udmf_things[i].args[4];

        if (udmf_things[i].alpha < 1.0)
        {
          const int32_t alpha = (int32_t)floor(udmf_things[i].alpha * 100.0);
          mt.tranmap = GetNormalTranMap(alpha);
        }

        int32_t lump = W_CheckNumForName(udmf_things[i].tranmap);
        if (lump >= 0 && W_LumpLength(lump) == 256 * 256)
        {
            mt.tranmap = W_CacheLumpNum(lump, PU_CACHE);
        }

        P_SpawnMapThing(&mt);
    }
}

static boolean UDMF_LoadBlockMap(int blockmap_num)
{
    long count;
    boolean ret = true;

    // [FG] always rebuild too short blockmaps
    if (M_CheckParm("-blockmap")
        || (count = W_LumpLengthWithName(blockmap_num, "BLOCKMAP") / 2)
               >= 0x10000
        || count < 4)
    {
        P_CreateBlockMap();
    }
    else
    {
        long i;
        short *wadblockmaplump = W_CacheLumpNum(blockmap_num, PU_LEVEL);
        blockmaplump = Z_Malloc(sizeof(*blockmaplump) * count, PU_LEVEL, 0);

        // killough 3/1/98: Expand wad blockmap into larger internal one,
        // by treating all offsets except -1 as unsigned and zero-extending
        // them. This potentially doubles the size of blockmaps allowed,
        // because Doom originally considered the offsets as always signed.

        blockmaplump[0] = SHORT(wadblockmaplump[0]);
        blockmaplump[1] = SHORT(wadblockmaplump[1]);
        blockmaplump[2] = (long)(SHORT(wadblockmaplump[2]))&FRACMASK;
        blockmaplump[3] = (long)(SHORT(wadblockmaplump[3]))&FRACMASK;

        for (i = 4; i < count; i++)
        {
            short t = SHORT(wadblockmaplump[i]); // killough 3/1/98
            blockmaplump[i] = t == -1 ? -1l : (long)t & FRACMASK;
        }

        Z_Free(wadblockmaplump);

        bmaporgx = blockmaplump[0] << FRACBITS;
        bmaporgy = blockmaplump[1] << FRACBITS;
        bmapwidth = blockmaplump[2];
        bmapheight = blockmaplump[3];

        ret = false;

        P_SetSkipBlockStart();
    }

    // clear out mobj chains
    blocklinks_size = sizeof(*blocklinks) * bmapwidth * bmapheight;
    blocklinks = M_ArenaAlloc(world_arena, blocklinks_size, alignof(mobj_t *));
    memset(blocklinks, 0, blocklinks_size);
    blockmap = blockmaplump + 4;

    return ret;
}

static boolean UDMF_LoadReject(int reject_num)
{
    // Calculate the size that the REJECT lump *should* be.
    int minlength = (numsectors * numsectors + 7) / 8;
    int lumplen = W_LumpLengthWithName(reject_num, "REJECT");
    boolean ret;

    // If the lump meets the minimum length, it can be loaded directly.
    // Otherwise, we need to allocate a buffer of the correct size
    // and pad it with appropriate data.

    if (lumplen >= minlength)
    {
        rejectmatrix = W_CacheLumpNum(reject_num, PU_LEVEL);
        ret = false;
    }
    else
    {
        unsigned int padvalue = 0x00;

        rejectmatrix = Z_Malloc(minlength, PU_LEVEL, (void **)&rejectmatrix);
        if (reject_num >= 0)
        {
            W_ReadLumpSize(reject_num, rejectmatrix, minlength);
        }

        if (M_CheckParm("-reject_pad_with_ff"))
        {
            padvalue = 0xff;
        }

        memset(rejectmatrix + lumplen, padvalue, minlength - lumplen);

        // No overflow emulation in UDMF

        ret = true;
    }

    return ret;
}

void UDMF_LoadMap(int lumpnum, nodeformat_t *nodeformat, int *gen_blockmap,
                  int *pad_reject)
{
    int znodes_num = -1;
    int reject_num = -1;
    int blockmap_num = -1;

    // +2 skips label, and TEXTMAP
    for (int i = lumpnum + 2; i < numlumps; ++i)
    {
        char *name = lumpinfo[i].name;
        if (!strcasecmp(name, UDMF_Lumps[UDMF_ZNODES]))
        {
            znodes_num = i;
        }
        else if (!strcasecmp(name, UDMF_Lumps[UDMF_REJECT]))
        {
            reject_num = i;
        }
        else if (!strcasecmp(name, UDMF_Lumps[UDMF_BLOCKMAP]))
        {
            blockmap_num = i;
        }
        else if (!strcasecmp(name, UDMF_Lumps[UDMF_ENDMAP]))
        {
            break;
        }
    }

    if (znodes_num < 0)
    {
        I_Error("Could not find ZNODES lump for UDMF map: %s.",
                lumpinfo[lumpnum].name);
    }

    *nodeformat = P_CheckUDMFNodeFormat(znodes_num);
    if (*nodeformat == NFMT_NANO)
    {
        I_Error("Invalid format found on ZNODES lump for UDMF map: %s",
                lumpinfo[lumpnum].name);
    }

    // Clear everything
    array_free(udmf_vertexes);
    array_free(udmf_linedefs);
    array_free(udmf_sidedefs);
    array_free(udmf_sectors);
    array_free(udmf_things);

    UDMF_ParseTextMap(lumpnum);

    // note: most of this ordering is important
    UDMF_LoadVertexes();
    UDMF_LoadSectors();
    UDMF_LoadSideDefs();      // <- This needs Sectors
    UDMF_LoadLineDefs();      // <- this needs Sides
    UDMF_LoadSideDefs_Post(); // <- this needs side_t::special
    UDMF_LoadLineDefs_Post(); // <- this needs Sides Post Processing

    *gen_blockmap = UDMF_LoadBlockMap(blockmap_num);
    P_LoadNodes_ZDoom(znodes_num, *nodeformat);
    P_GroupLines();
    *pad_reject = UDMF_LoadReject(reject_num);
}
