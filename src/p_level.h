//
//  Copyright (C) 1999 by
//  id Software, Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman
//  Copyright (C) 2026 by
//  Guilherme Miranda
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
//  Description:
//    Level loading procedures.

#ifndef __P_LEVEL_H__
#define __P_LEVEL_H__

#include "doomtype.h"
#include "m_fixed.h"

typedef enum mapformat_e
{
    MAP_NONE,
    MAP_DOOM,
    MAP_HEXEN,
    MAP_UDMF,
} map_format_t;

typedef enum bspformat_e
{
    BSP_DOOMBSP,
    BSP_DEEPBSPV4,
    BSP_XNOD,
    BSP_ZNOD,
    BSP_XGLN,
    BSP_ZGLN,
    BSP_XGL2,
    BSP_ZGL2,
    BSP_XGL3,
    BSP_ZGL3,
    BSP_NANO,
} bsp_format_t;

typedef enum bmap_format_e
{
    BMAP_DoomBlockmap,
    BMAP_XBM1,
    BMAP_BoomBuilder,
} bmap_format_t;

struct UDMF_Vertex_s;
struct UDMF_Linedef_s;
struct UDMF_Sidedef_s;
struct UDMF_Sector_s;
struct UDMF_Thing_s;

typedef struct map_s
{
    // Format used by the lumps
    map_format_t map_format;
    bsp_format_t bsp_format;
    bmap_format_t bmap_format;

    // Is map using the parameterized line special system?
    boolean param;
    // Is the reject matrix compiled correctly?
    boolean reject_built;

    // Level lump indices
    int32_t label;
    int32_t vertexes;
    int32_t linedefs;
    int32_t sidedefs;
    int32_t sectors;
    int32_t things;
    int32_t textmap;
    // BSP
    int32_t nodes;
    int32_t ssectors;
    int32_t segs;
    int32_t znodes;
    // Other
    int32_t blockmap;
    int32_t reject;
    int32_t behavior;
    int32_t dialogue;
    int32_t lightmap;

    struct UDMF_Vertex_s *udmf_vertexes;
    struct UDMF_Linedef_s *udmf_linedefs;
    struct UDMF_Sidedef_s *udmf_sidedefs;
    struct UDMF_Sector_s *udmf_sectors;
    struct UDMF_Thing_s *udmf_things;
} map_t;

//
// Map level types.
// The following data structures define the persistent format
// used in the lumps of the WAD files.
//

// Lump order in a map WAD: each map needs a couple of lumps
// to provide a complete scene geometry description.
typedef enum maplump_e
{
    ML_LABEL,    // A separator, name, ExMx or MAPxx
    ML_THINGS,   // Monsters, items..
    ML_LINEDEFS, // LineDefs, from editing
    ML_SIDEDEFS, // SideDefs, from editing
    ML_VERTEXES, // Vertices, edited and BSP splits generated
    ML_SEGS,     // LineSegs, from LineDefs split by BSP
    ML_SSECTORS, // SubSectors, list of LineSegs
    ML_NODES,    // BSP nodes
    ML_SECTORS,  // Sectors, from editing
    ML_REJECT,   // LUT, sector-sector visibility
    ML_BLOCKMAP, // LUT, motion clipping, walls/grid element
    ML_BEHAVIOR, // Hexen-format, ACS byte code.
    ML_SCRIPTS,  // Hexen-format, ZDoom extension, ACS source code.

    ML_TEXTMAP = ML_LABEL + 1, // UDMF map data
    ML_ZNODES,                 // ZDBSP-format BSP tree
    ML_DIALOGUE,               // USDF npc conversations
    ML_LIGHTMAP,               // Baked lighting, hardware-rendered
    ML_ENDMAP,                 // End-of-list marker

    ML_MAPLUMPCOUNT = ML_SCRIPTS + ML_ENDMAP,
} maplump_t;

// Support uncompiled maps by building with NanoBSP
// Same semantics as above
typedef enum maplump_unbuilt_e
{
    MLX_LABEL,
    MLX_THINGS,
    MLX_LINEDEFS,
    MLX_SIDEDEFS,
    MLX_VERTEXES,
    MLX_SECTORS,
    MLX_BEHAVIOR,
} maplump_unbuilt_t;

//
// Internal representation
//
typedef struct mapthing_s
{
    fixed_t x;
    fixed_t y;
    fixed_t height;
    int32_t tid;
    int32_t special;
    int32_t args[5];
    int16_t angle;
    int16_t type;
    int32_t options;
    fixed_t health;
    int32_t tint;
    byte *tranmap;
} mapthing_t;

// Needed here for legacy save games
typedef struct PACKED_PREFIX
{
    int16_t x;
    int16_t y;
    int16_t angle;
    int16_t type;
    uint16_t options;
} PACKED_SUFFIX mapthing_doom_t;

// SideDef attributes.
typedef enum sidedef_flags_e
{
    SF_NONE = (0),
    SF_ABS_LIGHT = (1u << 0),
    SF_ABS_LIGHT_TOP = (1u << 1),
    SF_ABS_LIGHT_MID = (1u << 2),
    SF_ABS_LIGHT_BOTTOM = (1u << 3),
    SF_NO_FAKE_CONTRAST = (1u << 4),
    SF_SMOOTH_CONTRAST = (1u << 5),
    SF_CLIP_MIDTEX = (1u << 6),
    SF_WRAP_MIDTEX = (1u << 7),
} sidedef_flags_t;

// LineDef attributes.

// Texture pegging:

// If a texture is pegged, the texture will have
// the end exposed to air held constant at the
// top or bottom of the texture (stairs or pulled
// down things) and will move with a height change
// of one of the neighbor sectors.
// Unpegged textures always have the first row of
// the texture at the top pixel of the line for both
// top and bottom textures (use next to windows).

// Reseved flag:

// haleyjd 05/02/06: Although it was believed until now that a reserved line
// flag was unnecessary, a problem with Ultimate DOOM E2M7 has disproven this
// theory. It has roughly 1000 linedefs with 0xFE00 masked into the flags, so
// making the next line flag reserved and using it to toggle off ALL extended
// flags will preserve compatibility for such maps. I have been told this map
// is one of the first ever created, so it may have something to do with that.

typedef enum linedef_flags_e
{
    // Solid, is an obstacle.
    ML_BLOCKING = (1u << 0),
    // Blocks monsters only.
    ML_BLOCKMONSTERS = (1u << 1),
    // Backside will not be drawn if not two sided.
    ML_TWOSIDED = (1u << 2),
    // upper texture unpegged
    ML_DONTPEGTOP = (1u << 3),
    // lower texture unpegged
    ML_DONTPEGBOTTOM = (1u << 4),
    // In AutoMap: don't map as two sided: IT'S A SECRET!
    ML_SECRET = (1u << 5),
    // Sound rendering: don't let sound cross two of these.
    ML_SOUNDBLOCK = (1u << 6),
    // Don't draw on the automap at all.
    ML_DONTDRAW = (1u << 7),
    // Set if already seen, thus drawn in automap.
    ML_MAPPED = (1u << 8),
    // jff 3/21/98 Set if line absorbs use by player
    // allow multiple push/switch triggers to be used on one push
    ML_PASSUSE = (1u << 9),
    // SoM 9/02/02: 3D Middletexture flag!
    ML_3DMIDTEX = (1u << 10),
    ML_RESERVED = (1u << 11),
    // mbf21
    ML_BLOCKLANDMONSTERS = (1u << 12),
    // mbf21
    ML_BLOCKPLAYERS = (1u << 13),

    // Masks
    ML_VANILLA = ML_BLOCKING | ML_BLOCKMONSTERS | ML_TWOSIDED | ML_DONTPEGTOP
        | ML_DONTPEGBOTTOM | ML_SECRET | ML_SOUNDBLOCK | ML_DONTDRAW
        | ML_MAPPED,
    ML_BOOM = ML_PASSUSE | ML_VANILLA,
    ML_MBF21 = ML_BLOCKPLAYERS | ML_BLOCKLANDMONSTERS | ML_RESERVED | ML_BOOM,
} linedef_flags_t;

// haleyjd 12/28/08: sector flags
typedef enum sector_flags_e
{
    SECF_SECRET = (1u << 0),
    SECF_FRICTION = (1u << 1),
    SECF_PUSH = (1u << 2),
    SECF_KILL_SOUND = (1u << 3),
    SECF_KILL_SOUND_MOVE = (1u << 4),
    SECF_KILL_PLAYER = (1u << 5),
    SECF_KILL_MONSTERS = (1u << 6),
    SECF_RESERVED1 = (1u << 7),
    SECF_RESERVED2 = (1u << 8),
    // UDMF
    SECF_ABS_LIGHT_FLOOR = (1u << 9),
    SECF_ABS_LIGHT_CEIL = (1u << 10),
} sector_flags_t;

#define NO_INDEX_SHORT ((unsigned short)-1)
#define FIX_NO_INDEX(x)      \
    if (x == NO_INDEX_SHORT) \
    {                        \
        x = NO_INDEX;        \
    }

extern map_t map;
void P_LoadThings_Doom(map_t *map);
void P_LoadThings_UDMF(map_t *map);

void P_LoadMap_Doom(map_t *map);
void P_LoadMap_UDMF(map_t *map);

#endif // __P_LEVEL_H__
