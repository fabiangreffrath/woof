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
#include "i_system.h"
#include "m_argv.h"
#include "m_array.h"
#include "m_bbox.h"
#include "m_fixed.h"
#include "m_misc.h"
#include "m_scanner.h"
#include "m_swap.h"
#include "p_extnodes.h"
#include "p_mobj.h"
#include "p_setup.h"
#include "r_data.h"
#include "r_main.h"
#include "r_state.h"
#include "w_wad.h"

//
// Universal Doom Map Format (UDMF) support
//

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

    // Compatibility
    UDMF_COMP_NO_ARG0 = (1 << 31),
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

    // Hexen
    int args[5];

    // Woof!
    char tranmap[9];
} UDMF_Linedef_t;

// Important note about line tag/id/arg0, in the Doom namespace:
// The base UDMF spec makes a distinction between the value used to identify a
// specific line (id), and the value used when an action is executed (arg0),
// as opposed to the binary Doom format, that used both as  the same (tag).

typedef struct
{
    // Base spec
    int sector_id;
    char texturetop[9];
    char texturemiddle[9];
    char texturebottom[9];
    int offsetx;
    int offsety;
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

static char *const UDMF_Lumps[] = {
    [UDMF_LABEL] = "-",           [UDMF_TEXTMAP] = "TEXTMAP",
    [UDMF_ZNODES] = "ZNODES",     [UDMF_BLOCKMAP] = "BLOCKMAP",
    [UDMF_REJECT] = "REJECT",     [UDMF_BEHAVIOR] = "BEHAVIOR",
    [UDMF_DIALOGUE] = "DIALOGUE", [UDMF_LIGHTMAP] = "LIGHTMAP",
    [UDMF_ENDMAP] = "ENDMAP",
};

static UDMF_Features_t udmf_features = UDMF_BASE;

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
    (!strcasecmp(prop, #keyword) && (udmf_features & flags))

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
    udmf_features = UDMF_BASE;

    if (!strcasecmp(name, "doom"))
    {
        udmf_features |= UDMF_DOOM | UDMF_BOOM | UDMF_MBF;
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
            line.args[0] = UDMF_ScanInt(s);
        }
        else if (BASE_PROP(arg1))
        {
            line.args[1] = UDMF_ScanInt(s);
        }
        else if (BASE_PROP(arg2))
        {
            line.args[2] = UDMF_ScanInt(s);
        }
        else if (BASE_PROP(arg3))
        {
            line.args[3] = UDMF_ScanInt(s);
        }
        else if (BASE_PROP(arg4))
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
        else if (PROP(tranmap, UDMF_MBF21))
        {
            UDMF_ScanLumpName(s, line.tranmap);
        }
        else if (PROP(passuse, UDMF_BOOM))
        {
            line.flags |= UDMF_ScanFlag(s, ML_PASSUSE);
        }
        else if (PROP(blocklandmonsters, UDMF_MBF21))
        {
            line.flags |= UDMF_ScanFlag(s, ML_BLOCKLANDMONSTERS);
        }
        else if (PROP(blockplayers, UDMF_MBF21))
        {
            line.flags |= UDMF_ScanFlag(s, ML_BLOCKPLAYERS);
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

    SC_MustGetToken(s, '{');
    while (!SC_CheckToken(s, '}'))
    {
        SC_MustGetToken(s, TK_Identifier);
        const char *prop = SC_GetString(s);
        if (BASE_PROP(type))
        {
            thing.type = UDMF_ScanInt(s);
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
        else if (PROP(dm, UDMF_BOOM))
        {
            thing.options &= ~UDMF_ScanFlag(s, MTF_NOTDM);
        }
        else if (PROP(coop, UDMF_BOOM))
        {
            thing.options &= ~UDMF_ScanFlag(s, MTF_NOTCOOP);
        }
        else if (PROP(friend, UDMF_MBF))
        {
            thing.options |= UDMF_ScanFlag(s, MTF_FRIEND);
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
        SC_Open("TEXTMAP", W_CacheLumpNum(lumpnum + UDMF_TEXTMAP, PU_LEVEL),
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
    vertexes = Z_Malloc(numvertexes * sizeof(vertex_t), PU_LEVEL, 0);
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
    sectors = Z_Malloc(numsectors * sizeof(sector_t), PU_LEVEL, 0);
    memset(sectors, 0, numsectors * sizeof(sector_t));

    for (int i = 0; i < numsectors; i++)
    {
        sectors[i].floorheight = IntToFixed(udmf_sectors[i].heightfloor);
        sectors[i].ceilingheight = IntToFixed(udmf_sectors[i].heightceiling);
        sectors[i].floorpic = R_FlatNumForName(udmf_sectors[i].texturefloor);
        sectors[i].ceilingpic =
            R_FlatNumForName(udmf_sectors[i].textureceiling);
        sectors[i].lightlevel = udmf_sectors[i].lightlevel;
        sectors[i].tag = udmf_sectors[i].tag;

        sectors[i].thinglist = NULL;
        sectors[i].touching_thinglist = NULL; // phares 3/14/98

        sectors[i].nextsec = -1; // jff 2/26/98 add fields to support locking
                                 // out
        sectors[i].prevsec = -1; // stair retriggering until build completes

        sectors[i].heightsec =
            -1; // sector used to get floor and ceiling height
        sectors[i].floorlightsec = -1;   // sector used to get floor lighting
        sectors[i].ceilinglightsec = -1; // sector used to get ceiling lighting:

        // [AM] Sector interpolation.  Even if we're
        //    not running uncapped, the renderer still
        //    uses this data.
        sectors[i].oldfloorheight = sectors[i].interpfloorheight =
            sectors[i].floorheight;
        sectors[i].oldceilingheight = sectors[i].interpceilingheight =
            sectors[i].ceilingheight;

        // [FG] inhibit sector interpolation during the 0th gametic
        sectors[i].oldceilgametic = sectors[i].oldfloorgametic = -1;
        sectors[i].old_ceil_offs_gametic = sectors[i].old_floor_offs_gametic =
            -1;
    }
}

static void UDMF_LoadSideDefs(void)
{
    numsides = array_size(udmf_sidedefs);
    sides = Z_Malloc(numsides * sizeof(side_t), PU_LEVEL, 0);
    memset(sides, 0, numsides * sizeof(side_t));

    for (int i = 0; i < numsides; i++)
    {
        sides[i].sector = &sectors[udmf_sidedefs[i].sector_id];
        sides[i].textureoffset = IntToFixed(udmf_sidedefs[i].offsetx);
        sides[i].rowoffset = IntToFixed(udmf_sidedefs[i].offsety);

        // [crispy] smooth texture scrolling
        sides[i].oldtextureoffset = sides[i].interptextureoffset =
            sides[i].textureoffset;
        sides[i].oldrowoffset = sides[i].interprowoffset = sides[i].rowoffset;
        sides[i].oldgametic = -1;
    }
}

static void UDMF_LoadLineDefs(void)
{
    numlines = array_size(udmf_linedefs);
    lines = Z_Malloc(numlines * sizeof(line_t), PU_LEVEL, 0);
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

        if (udmf_features & UDMF_COMP_NO_ARG0)
        {
            lines[i].args[0] = lines[i].id;
        }

        lines[i].tranlump = -1;
        // Line has TRANMAP lump
        if (!strcasecmp(udmf_linedefs[i].tranmap, "TRANMAP"))
        {
            // Is using default built-in TRANMAP lump
            lines[i].tranlump = 0;
        }
        else
        {
            lines[i].tranlump = W_CheckNumForName(udmf_linedefs[i].tranmap);
            if (lines[i].tranlump < 0
                || W_LumpLength(lines[i].tranlump) != 65536)
            {
                // Is using improper or non-existent custom TRANMAP lump
                lines[i].tranlump = 0;
            }
            else
            {
                // Is using proper custom TRANMAP lump
                W_CacheLumpNum(lines[i].tranlump, PU_CACHE);
                lines[i].tranlump++;
            }
        }

        vertex_t *v1 = lines[i].v1;
        vertex_t *v2 = lines[i].v2;
        lines[i].dx = v2->x - v1->x;
        lines[i].dy = v2->y - v1->y;
        lines[i].angle = R_PointToAngle2(v1->x, v1->y, v2->x, v2->y);

        lines[i].slopetype = !lines[i].dx   ? ST_VERTICAL
                             : !lines[i].dy ? ST_HORIZONTAL
                             : FixedDiv(lines[i].dy, lines[i].dx) > 0
                                 ? ST_POSITIVE
                                 : ST_NEGATIVE;

        if (v1->x < v2->x)
        {
            lines[i].bbox[BOXLEFT] = v1->x;
            lines[i].bbox[BOXRIGHT] = v2->x;
        }
        else
        {
            lines[i].bbox[BOXLEFT] = v2->x;
            lines[i].bbox[BOXRIGHT] = v1->x;
        }

        if (v1->y < v2->y)
        {
            lines[i].bbox[BOXBOTTOM] = v1->y;
            lines[i].bbox[BOXTOP] = v2->y;
        }
        else
        {
            lines[i].bbox[BOXBOTTOM] = v2->y;
            lines[i].bbox[BOXTOP] = v1->y;
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
            backside->special = lines[i].special;
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
                    int lump = sides[*lines[i].sidenum].special;
                    if (!lines[i].args[0])
                    {
                        // if tag==0,
                        // affect this linedef only
                        lines[i].tranlump = lump;
                    }
                    else
                    {
                        // if tag!=0,
                        // affect all matching linedefs
                        for (int j = 0; j < numlines; j++)
                        {
                            if (lines[j].id == lines[i].args[0])
                            {
                                lines[j].tranlump = lump;
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

        P_SpawnMapThing(&mt);
    }
}

boolean UDMF_LoadBlockMap(int blockmap_num)
{
    long count;
    boolean ret = true;

    //!
    // @category mod
    //
    // Forces a (re-)building of the BLOCKMAP lumps for loaded maps.
    //

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
    blocklinks = Z_Malloc(blocklinks_size, PU_LEVEL, 0);
    memset(blocklinks, 0, blocklinks_size);
    blockmap = blockmaplump + 4;

    return ret;
}

boolean UDMF_LoadReject(int reject_num, int totallines)
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
            W_ReadLump(reject_num, rejectmatrix);
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
    *pad_reject = UDMF_LoadReject(reject_num, P_GroupLines());
}
