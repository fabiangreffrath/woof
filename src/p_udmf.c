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
#include "doomdef.h"
#include "i_system.h"
#include "m_array.h"
#include "m_misc.h"
#include "m_scanner.h"
#include "p_spec.h"
#include "r_tranmap.h"
#include "w_wad.h"

//
// Universal Doom Map Format (UDMF) support
//

typedef enum
{
    UDMF_BASE = (0), // shut compiler up, also reset when parsing new map

    UDMF_THING_FRIEND  = (1u << 0), // Marine's Best Friend :)
    UDMF_THING_SPECIAL = (1u << 1), // Death/Pickup/etc-activated actions
    UDMF_THING_PARAM   = (1u << 2), // ditto, also customizes some MObjs
    UDMF_THING_HEALTH  = (1u << 3), // positive is a multiple, negative is override
    UDMF_THING_GRAVITY = (1u << 4), // per-mobj custom gravity
    UDMF_THING_ALPHA   = (1u << 5), // opacity percentage
    UDMF_THING_TRANMAP = (1u << 6), // ditto, also customizable LUT
    UDMF_THING_TINT    = (1u << 7), // view-agnostic colormap for the given mobj

    UDMF_LINE_PARAM    = (1u << 8), // Hexen-style param actions
    UDMF_LINE_PASSUSE  = (1u << 9), // Boom's "Pass Use Through" line flag
    UDMF_LINE_BLOCK    = (1u << 10), // MBF21's entity blocking flags
    UDMF_LINE_3DMIDTEX = (1u << 11), // EE's 3D middle texture
    UDMF_LINE_ALPHA    = (1u << 12), // opacity percentage
    UDMF_LINE_TRANMAP  = (1u << 13), // ditto, also customizable LUT
    UDMF_LINE_STYLE    = (1u << 14), // custom automap style

    UDMF_SIDE_OFFSET   = (1u << 15), // texture X/Y alignment
    UDMF_SIDE_SCROLL   = (1u << 16), // texture scrolling property
    UDMF_SIDE_LIGHT    = (1u << 17), // independent light levels
    UDMF_SIDE_TINT     = (1u << 18), // view-agnostic colormap for the given sidedef

    UDMF_SEC_ANGLE     = (1u << 19), // plane rotation
    UDMF_SEC_OFFSET    = (1u << 20), // plane X/Y alignment
    UDMF_SEC_EE_SCROLL = (1u << 21), // EE's original plane scrolling property
    UDMF_SEC_SCROLL    = (1u << 22), // DSDA's later plane scrolling property
    UDMF_SEC_LIGHT     = (1u << 23), // independent light levels
    UDMF_SEC_GRAVITY   = (1u << 24), // WIP
    UDMF_SEC_COLORMAP  = (1u << 25), // viewplayer's colormap on this given frame
    UDMF_SEC_TINT      = (1u << 26), // view-agnostic colormap for the given sector
    UDMF_SEC_SILENCE   = (1u << 27), // WIP

    // Compatibility
    UDMF_COMP_NO_ARG0  = (1u << 31),
} UDMF_Features_t;

static UDMF_Features_t udmf_flags = UDMF_BASE;

void P_ClearMemory_UDMF(map_t *map)
{
    array_free(map->udmf_vertexes);
    array_free(map->udmf_linedefs);
    array_free(map->udmf_sidedefs);
    array_free(map->udmf_sectors);
    array_free(map->udmf_things);
}

//
// UDMF parsing utils
//

// Retrieve plain integer
inline static int ScanInt(scanner_t *s)
{
    int x = 0;
    SC_MustGetToken(s, '=');
    x = SC_GetNegativeInteger(s);
    SC_MustGetToken(s, ';');
    return x;
}

// Retrieve plain double
inline static double ScanDouble(scanner_t *s)
{
    double x = 0;
    SC_MustGetToken(s, '=');
    x = SC_GetNegativeDecimal(s);
    SC_MustGetToken(s, ';');
    return x;
}

// Sets provided flag on, if true
inline static int ScanFlag(scanner_t *s, int f)
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
inline static void ScanLumpName(scanner_t *s, char *x)
{
    SC_MustGetToken(s, '=');
    SC_MustGetToken(s, TK_StringConst);
    M_CopyLumpName(x, SC_GetString(s));
    SC_MustGetToken(s, ';');
}

// Property is valid in all namespaces
#define BASE_PROP(keyword)   (!strcasecmp(prop, #keyword))

// Property is valid in the current namespace
#define PROP(keyword, flags) ((udmf_flags & (flags)) && !strcasecmp(prop, #keyword))

// Parse specific string properties
inline static int32_t ScanSectorScroll(scanner_t *s)
{
    int32_t mode = 0;
    SC_MustGetToken(s, '=');
    SC_MustGetToken(s, TK_StringConst);
    const char *buf = SC_GetString(s);
    M_StringToLower((char *)buf);
    if (!strcasecmp(buf, "visual"))
    {
        mode = SCROLL_TEXTURE;
    }
    else if (!strcasecmp(buf, "physical"))
    {
        mode = SCROLL_CARRY;
    }
    else if (!strcasecmp(buf, "both"))
    {
        mode = SCROLL_ALL;
    }
    SC_MustGetToken(s, ';');
    return mode;
}

// Skip unknown keyword
static inline void SkipScan(scanner_t *s)
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
static void ParseNamespace(scanner_t *s)
{
    SC_MustGetToken(s, '=');
    SC_MustGetToken(s, TK_StringConst);
    const char *name = SC_GetString(s);
    udmf_flags = UDMF_BASE;

    if (!strcasecmp(name, "doom"))
    {
        udmf_flags |= UDMF_LINE_PASSUSE | UDMF_THING_FRIEND;
    }
    else if (!strcasecmp(name, "woof"))
    {
        // clang-format off
        udmf_flags |= UDMF_LINE_PASSUSE | UDMF_LINE_BLOCK | UDMF_LINE_ALPHA | UDMF_LINE_TRANMAP;
        udmf_flags |= UDMF_THING_FRIEND | UDMF_THING_PARAM | UDMF_THING_HEALTH | UDMF_THING_ALPHA | UDMF_THING_TRANMAP | UDMF_THING_TINT;
        udmf_flags |= UDMF_SIDE_OFFSET | UDMF_SIDE_SCROLL | UDMF_SIDE_LIGHT | UDMF_SIDE_TINT;
        udmf_flags |= UDMF_SEC_ANGLE | UDMF_SEC_OFFSET | UDMF_SEC_SCROLL | UDMF_SEC_LIGHT | UDMF_SEC_COLORMAP | UDMF_SEC_TINT;
        // clang-format on
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

static void ParseVertex(scanner_t *s, map_t *map)
{
    UDMF_Vertex_t vertex = {0};

    SC_MustGetToken(s, '{');
    while (!SC_CheckToken(s, '}'))
    {
        SC_MustGetToken(s, TK_Identifier);
        const char *prop = SC_GetString(s);
        M_StringToLower((char *)prop);
        if (BASE_PROP(x))
        {
            vertex.x = ScanDouble(s);
        }
        else if (BASE_PROP(y))
        {
            vertex.y = ScanDouble(s);
        }
        else
        {
            SkipScan(s);
        }
    }

    array_push(map->udmf_vertexes, vertex);
}

//
// UDMF linedef loading
//

static void ParseLinedef(scanner_t *s, map_t *map)
{
    UDMF_Linedef_t line = {0};
    line.sideback = -1;
    M_CopyLumpName(line.tranmap, "-");
    line.alpha = 1.0;

    SC_MustGetToken(s, '{');
    while (!SC_CheckToken(s, '}'))
    {
        SC_MustGetToken(s, TK_Identifier);
        const char *prop = SC_GetString(s);
        M_StringToLower((char *)prop);
        if (BASE_PROP(v1))
        {
            line.v1_id = ScanInt(s);
        }
        else if (BASE_PROP(v2))
        {
            line.v2_id = ScanInt(s);
        }
        else if (BASE_PROP(special))
        {
            line.special = ScanInt(s);
        }
        else if (BASE_PROP(id))
        {
            line.id = ScanInt(s);
        }
        else if (BASE_PROP(arg0))
        {
            // Tag -> id/arg0 split means arg0 is always enabled
            line.args[0] = ScanInt(s);
        }
        else if (PROP(arg1, UDMF_LINE_PARAM))
        {
            line.args[1] = ScanInt(s);
        }
        else if (PROP(arg2, UDMF_LINE_PARAM))
        {
            line.args[2] = ScanInt(s);
        }
        else if (PROP(arg3, UDMF_LINE_PARAM))
        {
            line.args[3] = ScanInt(s);
        }
        else if (PROP(arg4, UDMF_LINE_PARAM))
        {
            line.args[4] = ScanInt(s);
        }
        else if (BASE_PROP(sidefront))
        {
            line.sidefront = ScanInt(s);
        }
        else if (BASE_PROP(sideback))
        {
            line.sideback = ScanInt(s);
        }
        else if (BASE_PROP(blocking))
        {
            line.flags |= ScanFlag(s, ML_BLOCKING);
        }
        else if (BASE_PROP(blockmonsters))
        {
            line.flags |= ScanFlag(s, ML_BLOCKMONSTERS);
        }
        else if (BASE_PROP(twosided))
        {
            line.flags |= ScanFlag(s, ML_TWOSIDED);
        }
        else if (BASE_PROP(dontpegtop))
        {
            line.flags |= ScanFlag(s, ML_DONTPEGTOP);
        }
        else if (BASE_PROP(dontpegbottom))
        {
            line.flags |= ScanFlag(s, ML_DONTPEGBOTTOM);
        }
        else if (BASE_PROP(secret))
        {
            line.flags |= ScanFlag(s, ML_SECRET);
        }
        else if (BASE_PROP(blocksound))
        {
            line.flags |= ScanFlag(s, ML_SOUNDBLOCK);
        }
        else if (BASE_PROP(dontdraw))
        {
            line.flags |= ScanFlag(s, ML_DONTDRAW);
        }
        else if (BASE_PROP(mapped))
        {
            line.flags |= ScanFlag(s, ML_MAPPED);
        }
        else if (PROP(passuse, UDMF_LINE_PASSUSE))
        {
            line.flags |= ScanFlag(s, ML_PASSUSE);
        }
        else if (PROP(blocklandmonsters, UDMF_LINE_BLOCK))
        {
            line.flags |= ScanFlag(s, ML_BLOCKLANDMONSTERS);
        }
        else if (PROP(blockplayers, UDMF_LINE_BLOCK))
        {
            line.flags |= ScanFlag(s, ML_BLOCKPLAYERS);
        }
        else if (PROP(midtex3d, UDMF_LINE_3DMIDTEX))
        {
            line.flags |= ScanFlag(s, ML_3DMIDTEX);
        }
        else if (PROP(alpha, UDMF_LINE_ALPHA))
        {
            line.alpha = ScanDouble(s);
        }
        else if (PROP(tranmap, UDMF_LINE_TRANMAP))
        {
            ScanLumpName(s, line.tranmap);
        }
        else if (PROP(automapstyle, UDMF_LINE_STYLE))
        {
            line.amls = ScanInt(s);
        }
        else
        {
            SkipScan(s);
        }
    }
    array_push(map->udmf_linedefs, line);
}

//
// UDMF sidedef parsing
//

static void ParseSidedef(scanner_t *s, map_t *map)
{
    UDMF_Sidedef_t side = {0};
    M_CopyLumpName(side.texturetop, "-");
    M_CopyLumpName(side.texturemiddle, "-");
    M_CopyLumpName(side.texturebottom, "-");
    M_CopyLumpName(side.tint, "-");

    SC_MustGetToken(s, '{');
    while (!SC_CheckToken(s, '}'))
    {
        SC_MustGetToken(s, TK_Identifier);
        const char *prop = SC_GetString(s);
        M_StringToLower((char *)prop);
        if (BASE_PROP(offsetx))
        {
            side.offsetx = ScanInt(s);
        }
        else if (BASE_PROP(offsety))
        {
            side.offsety = ScanInt(s);
        }
        else if (BASE_PROP(sector))
        {
            side.sector_id = ScanInt(s);
        }
        else if (BASE_PROP(texturetop))
        {
            ScanLumpName(s, side.texturetop);
        }
        else if (BASE_PROP(texturemiddle))
        {
            ScanLumpName(s, side.texturemiddle);
        }
        else if (BASE_PROP(texturebottom))
        {
            ScanLumpName(s, side.texturebottom);
        }
        else if (PROP(light, UDMF_SIDE_LIGHT))
        {
            side.light = ScanInt(s);
        }
        else if (PROP(light_top, UDMF_SIDE_LIGHT))
        {
            side.light_top = ScanInt(s);
        }
        else if (PROP(light_mid, UDMF_SIDE_LIGHT))
        {
            side.light_mid = ScanInt(s);
        }
        else if (PROP(light_bottom, UDMF_SIDE_LIGHT))
        {
            side.light_bottom = ScanInt(s);
        }
        else if (PROP(lightabsolute, UDMF_SIDE_LIGHT))
        {
            side.flags |= ScanFlag(s, SF_ABS_LIGHT);
        }
        else if (PROP(lightabsolute_top, UDMF_SIDE_LIGHT))
        {
            side.flags |= ScanFlag(s, SF_ABS_LIGHT_TOP);
        }
        else if (PROP(lightabsolute_mid, UDMF_SIDE_LIGHT))
        {
            side.flags |= ScanFlag(s, SF_ABS_LIGHT_MID);
        }
        else if (PROP(lightabsolute_bottom, UDMF_SIDE_LIGHT))
        {
            side.flags |= ScanFlag(s, SF_ABS_LIGHT_BOTTOM);
        }
        else if (PROP(nofakecontrast, UDMF_SIDE_LIGHT))
        {
            side.flags |= ScanFlag(s, SF_NO_FAKE_CONTRAST);
        }
        else if (PROP(smoothlighting, UDMF_SIDE_LIGHT))
        {
            side.flags |= ScanFlag(s, SF_SMOOTH_CONTRAST);
        }
        else if (PROP(offsetx_top, UDMF_SIDE_OFFSET))
        {
            side.offsetx_top = ScanDouble(s);
        }
        else if (PROP(offsety_top, UDMF_SIDE_OFFSET))
        {
            side.offsety_top = ScanDouble(s);
        }
        else if (PROP(offsetx_mid, UDMF_SIDE_OFFSET))
        {
            side.offsetx_mid = ScanDouble(s);
        }
        else if (PROP(offsety_mid, UDMF_SIDE_OFFSET))
        {
            side.offsety_mid = ScanDouble(s);
        }
        else if (PROP(offsetx_bottom, UDMF_SIDE_OFFSET))
        {
            side.offsetx_bottom = ScanDouble(s);
        }
        else if (PROP(offsety_bottom, UDMF_SIDE_OFFSET))
        {
            side.offsety_bottom = ScanDouble(s);
        }
        else if (PROP(xscroll, UDMF_SIDE_SCROLL))
        {
            side.xscroll = ScanInt(s);
        }
        else if (PROP(yscroll, UDMF_SIDE_SCROLL))
        {
            side.yscroll = ScanInt(s);
        }
        else if (PROP(xscrolltop, UDMF_SIDE_SCROLL))
        {
            side.xscrolltop = ScanDouble(s);
        }
        else if (PROP(yscrolltop, UDMF_SIDE_SCROLL))
        {
            side.yscrolltop = ScanDouble(s);
        }
        else if (PROP(xscrollmid, UDMF_SIDE_SCROLL))
        {
            side.xscrollmid = ScanDouble(s);
        }
        else if (PROP(yscrollmid, UDMF_SIDE_SCROLL))
        {
            side.yscrollmid = ScanDouble(s);
        }
        else if (PROP(xscrollbottom, UDMF_SIDE_SCROLL))
        {
            side.xscrollbottom = ScanDouble(s);
        }
        else if (PROP(yscrollbottom, UDMF_SIDE_SCROLL))
        {
            side.yscrollbottom = ScanDouble(s);
        }
        else if (PROP(tint, UDMF_SIDE_TINT))
        {
            ScanLumpName(s, side.tint);
        }
        else
        {
            SkipScan(s);
        }
    }

    array_push(map->udmf_sidedefs, side);
}

//
// UDMF sector parsing
//

static void ParseSector(scanner_t *s, map_t *map)
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
        M_StringToLower((char *)prop);
        if (BASE_PROP(heightfloor))
        {
            sector.heightfloor = ScanInt(s);
        }
        else if (BASE_PROP(heightceiling))
        {
            sector.heightceiling = ScanInt(s);
        }
        else if (BASE_PROP(texturefloor))
        {
            ScanLumpName(s, sector.texturefloor);
        }
        else if (BASE_PROP(textureceiling))
        {
            ScanLumpName(s, sector.textureceiling);
        }
        else if (BASE_PROP(lightlevel))
        {
            sector.lightlevel = ScanInt(s);
        }
        else if (BASE_PROP(special))
        {
            sector.special = ScanInt(s);
        }
        else if (BASE_PROP(id))
        {
            sector.tag = ScanInt(s);
        }
        else if (PROP(rotationfloor, UDMF_SEC_ANGLE))
        {
            sector.rotationfloor = ScanDouble(s);
        }
        else if (PROP(rotationceiling, UDMF_SEC_ANGLE))
        {
            sector.rotationceiling = ScanDouble(s);
        }
        else if (PROP(xpanningfloor, UDMF_SEC_OFFSET))
        {
            sector.xpanningfloor = ScanDouble(s);
        }
        else if (PROP(ypanningfloor, UDMF_SEC_OFFSET))
        {
            sector.ypanningfloor = ScanDouble(s);
        }
        else if (PROP(xpanningceiling, UDMF_SEC_OFFSET))
        {
            sector.xpanningceiling = ScanDouble(s);
        }
        else if (PROP(ypanningceiling, UDMF_SEC_OFFSET))
        {
            sector.ypanningceiling = ScanDouble(s);
        }
        else if (PROP(scroll_floor_x, UDMF_SEC_EE_SCROLL))
        {
            sector.scroll_floor_x = ScanDouble(s);
        }
        else if (PROP(scroll_floor_, UDMF_SEC_EE_SCROLL))
        {
            sector.scroll_floor_y = ScanDouble(s);
        }
        else if (PROP(scroll_floor_type, UDMF_SEC_EE_SCROLL))
        {
            sector.scroll_floor_type = ScanSectorScroll(s);
        }
        else if (PROP(scroll_ceil_x, UDMF_SEC_EE_SCROLL))
        {
            sector.scroll_ceil_x = ScanDouble(s);
        }
        else if (PROP(scroll_ceil_y, UDMF_SEC_EE_SCROLL))
        {
            sector.scroll_ceil_y = ScanDouble(s);
        }
        else if (PROP(scroll_ceil_type, UDMF_SEC_EE_SCROLL))
        {
            sector.scroll_ceil_type = ScanSectorScroll(s);
        }
        else if (PROP(xscrollfloor, UDMF_SEC_SCROLL))
        {
            sector.xscrollfloor = ScanDouble(s);
        }
        else if (PROP(yscrollfloor, UDMF_SEC_SCROLL))
        {
            sector.yscrollfloor = ScanDouble(s);
        }
        else if (PROP(xscrollceiling, UDMF_SEC_SCROLL))
        {
            sector.xscrollceiling = ScanDouble(s);
        }
        else if (PROP(yscrollceiling, UDMF_SEC_SCROLL))
        {
            sector.yscrollceiling = ScanDouble(s);
        }
        else if (PROP(scrollfloormode, UDMF_SEC_SCROLL))
        {
            sector.scrollfloormode = ScanInt(s);
        }
        else if (PROP(scrollceilingmode, UDMF_SEC_SCROLL))
        {
            sector.scrollceilingmode = ScanInt(s);
        }
        else if (PROP(lightfloor, UDMF_SEC_LIGHT))
        {
            sector.lightfloor = ScanInt(s);
        }
        else if (PROP(lightceiling, UDMF_SEC_LIGHT))
        {
            sector.lightceiling = ScanInt(s);
        }
        else if (PROP(lightfloorabsolute, UDMF_SEC_LIGHT))
        {
            sector.flags |= ScanFlag(s, SECF_ABS_LIGHT_FLOOR);
        }
        else if (PROP(lightceilingabsolute, UDMF_SEC_LIGHT))
        {
            sector.flags |= ScanFlag(s, SECF_ABS_LIGHT_CEIL);
        }
        else if (PROP(colormap, UDMF_SEC_COLORMAP))
        {
            ScanLumpName(s, sector.colormap);
        }
        else if (PROP(tint, UDMF_SEC_TINT))
        {
            ScanLumpName(s, sector.tint);
        }
        else if (PROP(tintfloor, UDMF_SEC_TINT))
        {
            ScanLumpName(s, sector.tintfloor);
        }
        else if (PROP(tintceiling, UDMF_SEC_TINT))
        {
            ScanLumpName(s, sector.tintceiling);
        }
        else
        {
            SkipScan(s);
        }
    }

    array_push(map->udmf_sectors, sector);
}

//
// UDMF thing loading
//

static void ParseThing(scanner_t *s, map_t *map)
{
    UDMF_Thing_t thing = {0};
    thing.options |= MTF_NOTSINGLE | MTF_NOTCOOP | MTF_NOTDM;
    M_CopyLumpName(thing.tranmap, "-");
    thing.alpha = 1.0;
    thing.health = 1.0;

    SC_MustGetToken(s, '{');
    while (!SC_CheckToken(s, '}'))
    {
        SC_MustGetToken(s, TK_Identifier);
        const char *prop = SC_GetString(s);
        M_StringToLower((char *)prop);
        if (BASE_PROP(type))
        {
            thing.type = ScanInt(s);
        }
        else if (PROP(id, UDMF_THING_PARAM))
        {
            thing.tid = ScanInt(s);
        }
        else if (BASE_PROP(x))
        {
            thing.x = ScanDouble(s);
        }
        else if (BASE_PROP(y))
        {
            thing.y = ScanDouble(s);
        }
        else if (BASE_PROP(height))
        {
            thing.height = ScanDouble(s);
        }
        else if (BASE_PROP(angle))
        {
            thing.angle = ScanInt(s);
        }
        else if (BASE_PROP(skill1))
        {
            thing.options |= ScanFlag(s, MTF_SKILL1);
        }
        else if (BASE_PROP(skill2))
        {
            thing.options |= ScanFlag(s, MTF_SKILL2);
        }
        else if (BASE_PROP(skill3))
        {
            thing.options |= ScanFlag(s, MTF_SKILL3);
        }
        else if (BASE_PROP(skill4))
        {
            thing.options |= ScanFlag(s, MTF_SKILL4);
        }
        else if (BASE_PROP(skill5))
        {
            thing.options |= ScanFlag(s, MTF_SKILL5);
        }
        else if (BASE_PROP(ambush))
        {
            thing.options |= ScanFlag(s, MTF_AMBUSH);
        }
        else if (BASE_PROP(single))
        {
            thing.options &= ~ScanFlag(s, MTF_NOTSINGLE);
        }
        else if (BASE_PROP(dm))
        {
            thing.options &= ~ScanFlag(s, MTF_NOTDM);
        }
        else if (BASE_PROP(coop))
        {
            thing.options &= ~ScanFlag(s, MTF_NOTCOOP);
        }
        else if (PROP(friend, UDMF_THING_FRIEND))
        {
            thing.options |= ScanFlag(s, MTF_FRIEND);
        }
        else if (PROP(special, UDMF_THING_SPECIAL))
        {
            thing.special = ScanInt(s);
        }
        else if (PROP(arg0, UDMF_THING_SPECIAL | UDMF_THING_PARAM))
        {
            thing.args[0] = ScanInt(s);
        }
        else if (PROP(arg1, UDMF_THING_PARAM))
        {
            thing.args[1] = ScanInt(s);
        }
        else if (PROP(arg2, UDMF_THING_PARAM))
        {
            thing.args[2] = ScanInt(s);
        }
        else if (PROP(arg3, UDMF_THING_PARAM))
        {
            thing.args[3] = ScanInt(s);
        }
        else if (PROP(arg4, UDMF_THING_PARAM))
        {
            thing.args[4] = ScanInt(s);
        }
        else if (PROP(arg4, UDMF_THING_HEALTH))
        {
            thing.health = ScanDouble(s);
        }
        else if (PROP(alpha, UDMF_THING_ALPHA))
        {
            thing.alpha = ScanDouble(s);
        }
        else if (PROP(tranmap, UDMF_THING_TRANMAP))
        {
            ScanLumpName(s, thing.tranmap);
        }
        else if (PROP(tint, UDMF_THING_TINT))
        {
            ScanLumpName(s, thing.tint);
        }
        else
        {
            SkipScan(s);
        }
    }

    array_push(map->udmf_things, thing);
}

//
// UDMF textmap loading
//

void P_ParseTextMap(map_t *map)
{
    scanner_t *s = SC_Open("TEXTMAP", W_CacheLumpNum(map->textmap, PU_CACHE),
                           W_LumpLength(map->textmap));

    while (SC_TokensLeft(s))
    {
        SC_MustGetToken(s, TK_Identifier);
        const char *toplevel = SC_GetString(s);
        M_StringToLower((char *)toplevel);

        if (!strcasecmp(toplevel, "namespace"))
        {
            ParseNamespace(s);
        }
        else if (!strcasecmp(toplevel, "vertex"))
        {
            ParseVertex(s, map);
        }
        else if (!strcasecmp(toplevel, "linedef"))
        {
            ParseLinedef(s, map);
        }
        else if (!strcasecmp(toplevel, "sidedef"))
        {
            ParseSidedef(s, map);
        }
        else if (!strcasecmp(toplevel, "sector"))
        {
            ParseSector(s, map);
        }
        else if (!strcasecmp(toplevel, "thing"))
        {
            ParseThing(s, map);
        }
        else
        {
            SkipScan(s);
        }
    }

    if (array_size(map->udmf_vertexes) == 0)
    {
        SC_Error(s, "Not enough UDMF vertexes. Check your TEXTMAP.");
    }

    if (array_size(map->udmf_linedefs) == 0)
    {
        SC_Error(s, "Not enough UDMF linedefs. Check your TEXTMAP.");
    }

    if (array_size(map->udmf_sidedefs) == 0)
    {
        SC_Error(s, "Not enough UDMF sidedefs. Check your TEXTMAP.");
    }

    if (array_size(map->udmf_sectors) == 0)
    {
        SC_Error(s, "Not enough UDMF sectors. Check your TEXTMAP.");
    }

    if (array_size(map->udmf_things) == 0)
    {
        SC_Error(s, "Not enough UDMF things. Check your TEXTMAP.");
    }

    SC_Close(s);
}
