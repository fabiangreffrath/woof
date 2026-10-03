//
//  Copyright (C) 1999 by
//  id Software, Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman
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
//  all external data is defined here
//  most of the data is loaded into different structures at run time
//  some internal structures shared by many modules are here
//
//-----------------------------------------------------------------------------

#ifndef __DOOMDATA__
#define __DOOMDATA__

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
    // Level components
    int label;
    int vertexes;
    int linedefs;
    int sidedefs;
    int sectors;
    int things;
    int textmap;
    int nodes;
    int ssectors;
    int segs;
    int znodes;
    int blockmap;
    int reject;
    int behavior;
    int dialogue;
    int lightmap;
} map_t;

//
// Map level types.
// The following data structures define the persistent format
// used in the lumps of the WAD files.
//

// Lump order in a map WAD: each map needs a couple of lumps
// to provide a complete scene geometry description.
enum {
  ML_LABEL,             // A separator, name, ExMx or MAPxx
  ML_THINGS,            // Monsters, items..
  ML_LINEDEFS,          // LineDefs, from editing
  ML_SIDEDEFS,          // SideDefs, from editing
  ML_VERTEXES,          // Vertices, edited and BSP splits generated
  ML_SEGS,              // LineSegs, from LineDefs split by BSP
  ML_SSECTORS,          // SubSectors, list of LineSegs
  ML_NODES,             // BSP nodes
  ML_SECTORS,           // Sectors, from editing
  ML_REJECT,            // LUT, sector-sector visibility
  ML_BLOCKMAP,          // LUT, motion clipping, walls/grid element
  ML_BEHAVIOR,          // Hexen-format, ACS byte code.
  ML_SCRIPTS,           // Hexen-format, ZDoom extension, ACS source code.

  ML_TEXTMAP = ML_LABEL + 1, // UDMF map data
  ML_ZNODES,                 // ZDBSP-format BSP tree
  ML_DIALOGUE,               // USDF npc conversations
  ML_LIGHTMAP,               // Baked lighting, hardware-rendered
  ML_ENDMAP,                 // End-of-list marker

  ML_MAPLUMPCOUNT = ML_SCRIPTS + ML_ENDMAP,
};

// Support uncompiled maps by building with NanoBSP
// Same semantics as above
enum {
  MLX_LABEL,
  MLX_THINGS,
  MLX_LINEDEFS,
  MLX_SIDEDEFS,
  MLX_VERTEXES,
  MLX_SECTORS,
  MLX_BEHAVIOR,
};

// A single Vertex.
typedef struct {
  short x,y;
} mapvertex_t;

// A SideDef, defining the visual appearance of a wall,
// by setting textures and offsets.
typedef struct {
  short textureoffset;
  short rowoffset;
  char  toptexture[8];
  char  bottomtexture[8];
  char  midtexture[8];
  short sector;  // Front sector, towards viewer.
} mapsidedef_t;

// Not supported in binary format maps, but needed for UDMF

typedef enum sidedef_flags_e
{
  SF_NONE             = (0),
  SF_ABS_LIGHT        = (1u << 0),
  SF_ABS_LIGHT_TOP    = (1u << 1),
  SF_ABS_LIGHT_MID    = (1u << 2),
  SF_ABS_LIGHT_BOTTOM = (1u << 3),
  SF_NO_FAKE_CONTRAST = (1u << 4),
  SF_SMOOTH_CONTRAST  = (1u << 5),
  SF_CLIP_MIDTEX      = (1u << 6),
  SF_WRAP_MIDTEX      = (1u << 7),
} sidedef_flags_t;

// A LineDef, as used for editing, and as input to the BSP builder.

typedef struct PACKED_PREFIX
{
  // [FG] extended nodes
  unsigned short v1;
  unsigned short v2;
  unsigned short flags;
  short special;
  short tag;
  unsigned short sidenum[2];  // sidenum[1] will be -1 (NO_INDEX) if one sided
} PACKED_SUFFIX maplinedef_doom_t;

typedef struct PACKED_PREFIX
{
    // [FG] extended nodes
    unsigned short v1;
    unsigned short v2;
    unsigned short flags;
    byte special;
    byte args[5];
    unsigned short sidenum[2];
} PACKED_SUFFIX maplinedef_hexen_t;

//
// LineDef attributes.
//

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
  // Doom map format
  ML_BLOCKING          = (1u << 0),  // Solid, is an obstacle.
  ML_BLOCKMONSTERS     = (1u << 1),  // Blocks monsters only.
  ML_TWOSIDED          = (1u << 2),  // Backside will not be drawn if not two sided.
  ML_DONTPEGTOP        = (1u << 3),  // upper texture unpegged
  ML_DONTPEGBOTTOM     = (1u << 4),  // lower texture unpegged
  ML_SECRET            = (1u << 5),  // In AutoMap: don't map as two sided: IT'S A SECRET!
  ML_SOUNDBLOCK        = (1u << 6),  // Sound rendering: don't let sound cross two of these.
  ML_DONTDRAW          = (1u << 7),  // Don't draw on the automap at all.
  ML_MAPPED            = (1u << 8),  // Set if already seen, thus drawn in automap.
  ML_PASSUSE           = (1u << 9),  // jff 3/21/98 Set if line absorbs use by player
                                     // allow multiple push/switch triggers to be used on one push
  ML_3DMIDTEX          = (1u << 10), // SoM 9/02/02: 3D Middletexture flag!
  ML_RESERVED          = (1u << 11),
  ML_BLOCKLANDMONSTERS = (1u << 12), // mbf21
  ML_BLOCKPLAYERS      = (1u << 13), // mbf21 Blocks players
  ML_RESERVED2         = (1u << 14), // MBF2y
  ML_RESERVED3         = (1u << 15), // MBF2y
  // Hexen map format
  ML_REPEATSPECIAL       = (1u << 16), // special is repeatable
  ML_MONSTERSCANACTIVATE = (1u << 17), // Monsters and players can activate
  ML_BLOCKEVERYTHING     = (1u << 18), // Blocks everything
  // UDMF
  ML_FIRSTSIDEONLY    = (1u << 19),
  ML_CHECKSWITCHRANGE = (1u << 20),
} linedef_flags_t;

typedef enum linedef_flags_hexen_e
{
  HML_REPEATSPECIAL       = 0x0200,
  HML_SPAC_MASK           = 0x1c00,
  ZML_MONSTERSCANACTIVATE = 0x2000,
  ZML_BLOCKPLAYERS        = 0x4000,
  ZML_BLOCKEVERYTHING     = 0x8000,
} linedef_flags_hexen_t;

// line activation
#define HML_SPAC_SHIFT 10
#define GET_SPAC_INDEX(flags) ((flags & HML_SPAC_MASK) >> HML_SPAC_SHIFT)

typedef enum spac_e
{
    SPAC_None       = (0),
    SPAC_Cross      = (1u << 0),
    SPAC_Use        = (1u << 1),
    SPAC_MCross     = (1u << 2),
    SPAC_Impact     = (1u << 3),
    SPAC_Push       = (1u << 4),
    SPAC_PCross     = (1u << 5),
    SPAC_UseThrough = (1u << 6),
    SPAC_AnyCross   = (1u << 7),
    SPAC_MUse       = (1u << 8),
    SPAC_MPush      = (1u << 9),
    SPAC_UseBack    = (1u << 10),
    SPAC_Damage     = (1u << 11),
    SPAC_Death      = (1u << 12),
    SPAC_Walking    = (1u << 13),
} spac_t;

// Sector definition, from editing.
typedef struct {
  short floorheight;
  short ceilingheight;
  char  floorpic[8];
  char  ceilingpic[8];
  short lightlevel;
  short special;
  short tag;
} mapsector_t;

// haleyjd 12/28/08: sector flags

typedef enum sector_flags_e
{
  SECF_SECRET          = (1u << 0),
  SECF_FRICTION        = (1u << 1),
  SECF_PUSH            = (1u << 2),
  SECF_KILL_SOUND      = (1u << 3),
  SECF_KILL_SOUND_MOVE = (1u << 4),
  SECF_KILL_PLAYER     = (1u << 5),
  SECF_KILL_MONSTERS   = (1u << 6),
  SECF_RESERVED1       = (1u << 7),
  SECF_RESERVED2       = (1u << 8),
  // UDMF
  SECF_ABS_LIGHT_FLOOR = (1u << 9),
  SECF_ABS_LIGHT_CEIL  = (1u << 10),
  SECF_DMG_TERRAIN_FX  = (1u << 11),
  SECF_HAZARD          = (1u << 12),
  SECF_HURT_MONSTERS   = (1u << 13),
  SECF_HARM_IN_AIR     = (1u << 14),
  SECF_NOATTACK        = (1u << 15),
  SECF_HIDDEN          = (1u << 16),
  // Internal only
  SECF_WAS_SECRET      = (1u << 17),
  SECF_END_GODMODE     = (1u << 18),
  SECF_END_LEVEL       = (1u << 19),
} sector_flags_t;

#define SECF_DAMAGE_FLAGS  (SECF_DMG_TERRAIN_FX|SECF_HAZARD|SECF_END_GODMODE|SECF_END_LEVEL)
#define SECF_TRANSFER_MASK (SECF_SECRET|SECF_FRICTION|SECF_PUSH|SECF_DAMAGE_FLAGS|SECF_WAS_SECRET)

// Indicate a leaf.
#define NF_SUBSECTOR    0x80000000
 // [FG] extended nodes
#define NO_INDEX_SHORT  ((unsigned short)-1)
#define NO_INDEX        ((unsigned int)-1)
#define FIX_NO_INDEX(x) if (x == NO_INDEX_SHORT) { x = NO_INDEX; }

// Thing definition, position, orientation and type,
// plus skill/visibility flags and attributes.
typedef struct PACKED_PREFIX mapthing_hexen_s
{
  short tid;
  short x;
  short y;
  short height;
  short angle;
  short type;
  short options;
  byte special;
  byte args[5];
} PACKED_SUFFIX mapthing_hexen_t;

typedef struct PACKED_PREFIX mapthing_doom_s
{
  short x;
  short y;
  short angle;
  short type;
  short options;
} PACKED_SUFFIX mapthing_doom_t;

typedef struct {
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

#endif // __DOOMDATA__

//----------------------------------------------------------------------------
//
// $Log: doomdata.h,v $
// Revision 1.4  1998/05/03  22:39:10  killough
// beautification
//
// Revision 1.3  1998/03/23  06:42:57  jim
// linedefs reference initial version
//
// Revision 1.2  1998/01/26  19:26:37  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:02:51  rand
// Lee's Jan 19 sources
//
//----------------------------------------------------------------------------
