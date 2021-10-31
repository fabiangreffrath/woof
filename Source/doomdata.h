// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// $Id: doomdata.h,v 1.4 1998/05/03 22:39:10 killough Exp $
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
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 
//  02111-1307, USA.
//
// DESCRIPTION:
//  all external data is defined here
//  most of the data is loaded into different structures at run time
//  some internal structures shared by many modules are here
//
//-----------------------------------------------------------------------------

#ifndef __DOOMDATA__
#define __DOOMDATA__

// The most basic types we use, portability.
#include "doomtype.h"

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
  ML_BLOCKMAP           // LUT, motion clipping, walls/grid element
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

// A LineDef, as used for editing, and as input to the BSP builder.

typedef struct {
  // [FG] extended nodes
  unsigned short v1;
  unsigned short v2;
  unsigned short flags;
  short special;
  short tag;
  unsigned short sidenum[2];  // sidenum[1] will be -1 (NO_INDEX) if one sided
} maplinedef_t;

//
// LineDef attributes.
//

// Solid, is an obstacle.
#define ML_BLOCKING             1

// Blocks monsters only.
#define ML_BLOCKMONSTERS        2

// Backside will not be drawn if not two sided.
#define ML_TWOSIDED             4

// If a texture is pegged, the texture will have
// the end exposed to air held constant at the
// top or bottom of the texture (stairs or pulled
// down things) and will move with a height change
// of one of the neighbor sectors.
// Unpegged textures always have the first row of
// the texture at the top pixel of the line for both
// top and bottom textures (use next to windows).

// upper texture unpegged
#define ML_DONTPEGTOP           8

// lower texture unpegged
#define ML_DONTPEGBOTTOM        16

// In AutoMap: don't map as two sided: IT'S A SECRET!
#define ML_SECRET               32

// Sound rendering: don't let sound cross two of these.
#define ML_SOUNDBLOCK           64

// Don't draw on the automap at all.
#define ML_DONTDRAW             128

// Set if already seen, thus drawn in automap.
#define ML_MAPPED               256

//jff 3/21/98 Set if line absorbs use by player
//allow multiple push/switch triggers to be used on one push
#define ML_PASSUSE      512

// Reserved by EE
// SoM 9/02/02: 3D Middletexture flag!
#define ML_3DMIDTEX             1024

// haleyjd 05/02/06: Although it was believed until now that a reserved line
// flag was unnecessary, a problem with Ultimate DOOM E2M7 has disproven this
// theory. It has roughly 1000 linedefs with 0xFE00 masked into the flags, so
// making the next line flag reserved and using it to toggle off ALL extended
// flags will preserve compatibility for such maps. I have been told this map
// is one of the first ever created, so it may have something to do with that.
#define ML_RESERVED             2048

// mbf21
#define ML_BLOCKLANDMONSTERS    4096
#define ML_BLOCKPLAYERS         8192

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

// SubSector, as generated by BSP.
typedef struct {
  // [FG] extended nodes
  unsigned short numsegs;
  unsigned short firstseg;    // Index of first one; segs are stored sequentially.
} mapsubsector_t;

// LineSeg, generated by splitting LineDefs
// using partition lines selected by BSP builder.
typedef struct {
  // [FG] extended nodes
  unsigned short v1;
  unsigned short v2;
  short angle;
  unsigned short linedef;
  short side;
  short offset;
} mapseg_t;

// BSP node structure.

// Indicate a leaf.
#define NF_SUBSECTOR    0x80000000
 // [FG] extended nodes
#define NO_INDEX        ((unsigned short)-1)

typedef struct {
  short x;  // Partition line from (x,y) to x+dx,y+dy)
  short y;
  short dx;
  short dy;
  // Bounding box for each child, clip against view frustum.
  short bbox[2][4];
  // If NF_SUBSECTOR its a subsector, else it's a node of another subtree.
  unsigned short children[2];
} mapnode_t;

// Thing definition, position, orientation and type,
// plus skill/visibility flags and attributes.
typedef struct {
  short x;
  short y;
  short angle;
  short type;
  short options;
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
