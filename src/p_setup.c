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
//  Do all the WAD I/O, get map description,
//  set up initial state and misc. LUTs.
//
//-----------------------------------------------------------------------------

#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "d_think.h"
#include "doomdata.h"
#include "doomstat.h"
#include "g_game.h"
#include "g_compatibility.h"
#include "i_printf.h"
#include "i_system.h"
#include "info.h"
#include "m_arena.h"
#include "m_argv.h"
#include "m_bbox.h"
#include "m_misc.h"
#include "m_swap.h"
#include "nano_bsp.h"
#include "p_enemy.h"
#include "p_extnodes.h"
#include "p_keyframe.h"
#include "p_map.h"
#include "p_maputl.h"
#include "p_mobj.h"
#include "p_setup.h"
#include "p_spec.h"
#include "p_tick.h"
#include "r_data.h"
#include "r_defs.h"
#include "r_sky.h"
#include "r_main.h"
#include "r_state.h"
#include "r_things.h"
#include "s_musinfo.h" // [crispy] S_ParseMusInfo()
#include "s_sound.h"
#include "tables.h"
#include "w_wad.h"
#include "z_zone.h"

//
// MAP related Lookup tables.
// Store VERTEXES, LINEDEFS, SIDEDEFS, etc.
//

int      numvertexes;
vertex_t *vertexes;

int      numsegs;
seg_t    *segs;

int      numsectors;
sector_t *sectors;

int      numsubsectors;
subsector_t *subsectors;

int      numnodes;
node_t   *nodes;

int      numlines;
line_t   *lines;

int      numsides;
side_t   *sides;

// BLOCKMAP
// Created from axis aligned bounding box
// of the map, a rectangular array of
// blocks of size ...
// Used to speed up collision detection
// by spatial subdivision in 2D.
//
// Blockmap size.

int       bmapwidth, bmapheight;  // size in mapblocks

// killough 3/1/98: remove blockmap limit internally:
int32_t      *blockmap;           // was short -- killough

// offsets in blockmap are from here
int32_t      *blockmaplump;       // was short -- killough

fixed_t   bmaporgx, bmaporgy;     // origin of block map

mobj_t    **blocklinks;           // for thing chains
int       blocklinks_size;

boolean   skipblstart;  // MaxW: Skip initial blocklist short

//
// REJECT
// For fast sight rejection.
// Speeds up enemy AI by skipping detailed
//  LineOf Sight calculation.
// Without the special effect, this could
// be used as a PVS lookup as well.
//

byte *rejectmatrix;

// Maintain single and multi player starting spots.

// 1/11/98 killough: Remove limit on deathmatch starts
mapthing_t *deathmatchstarts;      // killough
size_t     num_deathmatchstarts;   // killough

mapthing_t *deathmatch_p;
mapthing_t playerstarts[MAXPLAYERS];

//
// P_LoadVertexes
//
// killough 5/3/98: reformatted, cleaned up

void P_LoadVertexes (int lump)
{
  byte *data;
  int i;

  // Determine number of lumps:
  //  total lump length / vertex record length.
  numvertexes = W_LumpLength(lump) / sizeof(mapvertex_t);

  // Allocate zone memory for buffer.
  vertexes = Z_Malloc(numvertexes*sizeof(vertex_t),PU_LEVEL,0);

  // Load data into cache.
  data = W_CacheLumpNum(lump, PU_STATIC);

  // Copy and convert vertex coordinates,
  // internal representation as fixed.
  for (i=0; i<numvertexes; i++)
    {
      vertexes[i].x = SHORT(((mapvertex_t *) data)[i].x)<<FRACBITS;
      vertexes[i].y = SHORT(((mapvertex_t *) data)[i].y)<<FRACBITS;

      // [FG] vertex coordinates used for rendering
      vertexes[i].r_x = vertexes[i].x;
      vertexes[i].r_y = vertexes[i].y;
    }

  // Free buffer memory.
  Z_Free (data);
}

// GetSectorAtNullAddress

sector_t* GetSectorAtNullAddress(void)
{
  static boolean null_sector_is_initialized = false;
  static sector_t null_sector;

  if (demo_compatibility && overflow[emu_missedbackside].enabled)
  {
    overflow[emu_missedbackside].triggered = true;

    if (!null_sector_is_initialized)
    {
      memset(&null_sector, 0, sizeof(null_sector));
      I_GetMemoryValue(0, &null_sector.floorheight, 4);
      I_GetMemoryValue(4, &null_sector.ceilingheight, 4);
      null_sector_is_initialized = true;
    }

    return &null_sector;
  }

  return 0;
}

//
// P_LoadSegs
//
// killough 5/3/98: reformatted, cleaned up

void P_LoadSegs (int lump)
{
  int  i;
  byte *data;

  numsegs = W_LumpLength(lump) / sizeof(mapseg_t);
  segs = Z_Malloc(numsegs*sizeof(seg_t),PU_LEVEL,0);
  memset(segs, 0, numsegs*sizeof(seg_t));
  data = W_CacheLumpNum(lump,PU_STATIC);

  for (i=0; i<numsegs; i++)
    {
      seg_t *li = segs+i;
      mapseg_t *ml = (mapseg_t *) data + i;

      int side, linedef;
      line_t *ldef;

      // [FG] extended nodes
      li->v1 = &vertexes[(unsigned short)SHORT(ml->v1)];
      li->v2 = &vertexes[(unsigned short)SHORT(ml->v2)];

      li->angle = (SHORT(ml->angle))<<16;
      li->offset = (SHORT(ml->offset))<<16;
      linedef = (unsigned short)SHORT(ml->linedef); // [FG] extended nodes
      ldef = &lines[linedef];
      li->linedef = ldef;
      side = SHORT(ml->side);

      // Andrey Budko: check for wrong indexes
      if ((unsigned)ldef->sidenum[side] >= (unsigned)numsides)
      {
        I_Error("linedef %d for seg %d references a non-existent sidedef %d",
                linedef, i, (unsigned)ldef->sidenum[side]);
      }

      li->sidedef = &sides[ldef->sidenum[side]];
      li->frontsector = sides[ldef->sidenum[side]].sector;
      // [FG] recalculate
      li->offset = P_GetOffset(li->v1, (ml->side ? ldef->v2 : ldef->v1));

      if (ldef->flags & ML_TWOSIDED)
      {
        int sidenum = ldef->sidenum[side ^ 1];

        if (sidenum == NO_INDEX)
        {
          // this is wrong
          li->backsector = GetSectorAtNullAddress();
        }
        else
        {
          li->backsector = sides[sidenum].sector;
        }
      }
      else
      {
        li->backsector = 0;
      }
    }

  Z_Free (data);
}

//
// P_LoadSubsectors
//
// killough 5/3/98: reformatted, cleaned up

void P_LoadSubsectors (int lump)
{
  byte *data;
  int  i;

  numsubsectors = W_LumpLength (lump) / sizeof(mapsubsector_t);
  subsectors = Z_Malloc(numsubsectors*sizeof(subsector_t),PU_LEVEL,0);
  data = W_CacheLumpNum(lump, PU_STATIC);

  memset(subsectors, 0, numsubsectors*sizeof(subsector_t));

  for (i=0; i<numsubsectors; i++)
    {
      // [FG] extended nodes
      subsectors[i].numlines  = (unsigned short)SHORT(((mapsubsector_t *) data)[i].numsegs );
      subsectors[i].firstline = (unsigned short)SHORT(((mapsubsector_t *) data)[i].firstseg);
    }

  Z_Free (data);
}

//
// P_LoadSectors
//
// killough 5/3/98: reformatted, cleaned up

void P_LoadSectors (int lump)
{
  byte *data;
  int  i;

  // [FG] SEGS, SSECTORS, NODES lumps missing?
  for (i = ML_SECTORS; !W_LumpExistsWithName(lump, "SECTORS") && i > ML_VERTEXES; i--)
  {
    lump--;
  }
  if (i == ML_VERTEXES)
  {
    I_Error("No SECTORS found for %s!", lumpinfo[lump - i].name);
  }

  numsectors = W_LumpLength (lump) / sizeof(mapsector_t);
  sectors = Z_Malloc (numsectors*sizeof(sector_t),PU_LEVEL,0);
  memset (sectors, 0, numsectors*sizeof(sector_t));
  data = W_CacheLumpNum (lump,PU_STATIC);

  for (i=0; i<numsectors; i++)
    {
      sector_t *ss = sectors + i;
      const mapsector_t *ms = (mapsector_t *) data + i;

      ss->floorheight = SHORT(ms->floorheight)<<FRACBITS;
      ss->ceilingheight = SHORT(ms->ceilingheight)<<FRACBITS;
      ss->floorpic = R_FlatNumForName(ms->floorpic);
      ss->ceilingpic = R_FlatNumForName(ms->ceilingpic);
      ss->lightlevel = SHORT(ms->lightlevel);
      ss->special = SHORT(ms->special);
      ss->oldspecial = SHORT(ms->special);
      ss->tag = SHORT(ms->tag);
      ss->thinglist = NULL;
      ss->touching_thinglist = NULL;            // phares 3/14/98
      // WiggleFix: [kb] for R_FixWiggle()
      ss->cachedheight = 0;

      ss->nextsec = -1; //jff 2/26/98 add fields to support locking out
      ss->prevsec = -1; // stair retriggering until build completes

      // killough 3/7/98:
      ss->floor_xoffs = 0;
      ss->floor_yoffs = 0;      // floor and ceiling flats offsets
      ss->old_floor_xoffs = ss->interp_floor_xoffs = 0;
      ss->old_floor_yoffs = ss->interp_floor_yoffs = 0;
      ss->floor_rotation = 0;
      ss->ceiling_xoffs = 0;
      ss->ceiling_yoffs = 0;
      ss->old_ceiling_xoffs = ss->interp_ceiling_xoffs = 0;
      ss->old_ceiling_yoffs = ss->interp_ceiling_yoffs = 0;
      ss->ceiling_rotation = 0;
      ss->heightsec = -1;       // sector used to get floor and ceiling height
      ss->floorlightsec = -1;   // sector used to get floor lighting
      // killough 3/7/98: end changes

      // killough 4/11/98 sector used to get ceiling lighting:
      ss->ceilinglightsec = -1;

      // ID24 per-sector colormap
      // killough 4/4/98: colormaps:
      ss->tint = ss->bottommap = ss->midmap = ss->topmap = 0;

      // killough 10/98: sky textures coming from sidedefs:
      ss->floorsky = ss->ceilingsky = 0;

      // [AM] Sector interpolation.  Even if we're
      //      not running uncapped, the renderer still
      //      uses this data.
      ss->oldfloorheight = ss->floorheight;
      ss->interpfloorheight = ss->floorheight;
      ss->oldceilingheight = ss->ceilingheight;
      ss->interpceilingheight = ss->ceilingheight;
      // [FG] inhibit sector interpolation during the 0th gametic
      ss->oldceilgametic = -1;
      ss->oldfloorgametic = -1;
      ss->old_ceil_offs_gametic = -1;
      ss->old_floor_offs_gametic = -1;
    }

  Z_Free (data);
}


//
// P_LoadNodes
//
// killough 5/3/98: reformatted, cleaned up

void P_LoadNodes (int lump)
{
  byte *data;
  int  i;

  numnodes = W_LumpLength (lump) / sizeof(mapnode_t);
  nodes = Z_Malloc (numnodes*sizeof(node_t),PU_LEVEL,0);
  data = W_CacheLumpNum (lump, PU_STATIC);

  for (i=0; i<numnodes; i++)
    {
      node_t *no = nodes + i;
      mapnode_t *mn = (mapnode_t *) data + i;
      int j;

      no->x = SHORT(mn->x)<<FRACBITS;
      no->y = SHORT(mn->y)<<FRACBITS;
      no->dx = SHORT(mn->dx)<<FRACBITS;
      no->dy = SHORT(mn->dy)<<FRACBITS;

      for (j=0 ; j<2 ; j++)
        {
          int k;
          no->children[j] = (unsigned short)SHORT(mn->children[j]); // [FG] extended nodes

          // [FG] extended nodes
          if (no->children[j] == (unsigned short)-1)
              no->children[j] = -1;
          else
          if (no->children[j] & NF_SUBSECTOR_VANILLA)
          {
              no->children[j] &= ~NF_SUBSECTOR_VANILLA;

              if (no->children[j] >= numsubsectors)
                  no->children[j] = 0;

              no->children[j] |= NF_SUBSECTOR;
          }

          for (k=0 ; k<4 ; k++)
            no->bbox[j][k] = SHORT(mn->bbox[j][k])<<FRACBITS;
        }
    }

  Z_Free (data);
}


//
// P_LoadThings
//
// killough 5/3/98: reformatted, cleaned up

void P_LoadThings (int lump)
{
  int  i, numthings = W_LumpLength (lump) / sizeof(mapthing_t);
  byte *data = W_CacheLumpNum (lump,PU_STATIC);

  for (i=0; i<numthings; i++)
    {
      mapthing_t *mt = (mapthing_t *) data + i;

      // Do not spawn cool, new monsters if !commercial
      if (gamemode != commercial)
        switch(mt->type)
          {
          case 68:  // Arachnotron
          case 64:  // Archvile
          case 88:  // Boss Brain
          case 89:  // Boss Shooter
          case 69:  // Hell Knight
          case 67:  // Mancubus
          case 71:  // Pain Elemental
          case 65:  // Former Human Commando
          case 66:  // Revenant
          case 84:  // Wolf SS
            continue;
          }

      // Do spawn all other stuff.
      mt->x = SHORT(mt->x);
      mt->y = SHORT(mt->y);
      mt->angle = SHORT(mt->angle);
      mt->type = SHORT(mt->type);
      mt->options = SHORT(mt->options);

      P_SpawnMapThing (mt);
    }

  Z_Free (data);
}

//
// P_LoadLineDefs
// Also counts secret lines for intermissions.
//        ^^^
// ??? killough ???
// Does this mean secrets used to be linedef-based, rather than sector-based?
//
// killough 4/4/98: split into two functions, to allow sidedef overloading
//
// killough 5/3/98: reformatted, cleaned up

void P_LoadLineDefs (int lump)
{
  byte *data;
  int  i;

  numlines = W_LumpLength (lump) / sizeof(maplinedef_t);
  lines = Z_Malloc (numlines*sizeof(line_t),PU_LEVEL,0);
  memset (lines, 0, numlines*sizeof(line_t));
  data = W_CacheLumpNum (lump,PU_STATIC);

  for (i=0; i<numlines; i++)
    {
      maplinedef_t *mld = (maplinedef_t *) data + i;
      line_t *ld = lines+i;
      vertex_t *v1, *v2;

      // [FG] extended nodes
      ld->flags = (unsigned short)SHORT(mld->flags);
      ld->special = SHORT(mld->special);
      ld->tag = SHORT(mld->tag);
      v1 = ld->v1 = &vertexes[(unsigned short)SHORT(mld->v1)];
      v2 = ld->v2 = &vertexes[(unsigned short)SHORT(mld->v2)];
      ld->dx = v2->x - v1->x;
      ld->dy = v2->y - v1->y;
      ld->angle = R_PointToAngle2(lines[i].v1->x, lines[i].v1->y,
                                  lines[i].v2->x, lines[i].v2->y);

      ld->tranlump = -1;   // killough 4/11/98: no translucency by default

      ld->slopetype = !ld->dx ? ST_VERTICAL : !ld->dy ? ST_HORIZONTAL :
        FixedDiv(ld->dy, ld->dx) > 0 ? ST_POSITIVE : ST_NEGATIVE;

      if (v1->x < v2->x)
        {
          ld->bbox[BOXLEFT] = v1->x;
          ld->bbox[BOXRIGHT] = v2->x;
        }
      else
        {
          ld->bbox[BOXLEFT] = v2->x;
          ld->bbox[BOXRIGHT] = v1->x;
        }

      if (v1->y < v2->y)
        {
          ld->bbox[BOXBOTTOM] = v1->y;
          ld->bbox[BOXTOP] = v2->y;
        }
      else
        {
          ld->bbox[BOXBOTTOM] = v2->y;
          ld->bbox[BOXTOP] = v1->y;
        }

      ld->sidenum[0] = SHORT(mld->sidenum[0]);
      ld->sidenum[1] = SHORT(mld->sidenum[1]);

      // killough 4/4/98: support special sidedef interpretation below
      if (ld->sidenum[0] != NO_INDEX && ld->special)
        sides[*ld->sidenum].special = ld->special;
    }
  Z_Free (data);
}

// killough 4/4/98: delay using sidedefs until they are loaded
// killough 5/3/98: reformatted, cleaned up

void P_LoadLineDefs2(int lump)
{
  int i = numlines;
  register line_t *ld = lines;
  for (;i--;ld++)
    {
      // killough 11/98: fix common wad errors (missing sidedefs):

      if (ld->sidenum[0] == NO_INDEX)
	ld->sidenum[0] = 0;  // Substitute dummy sidedef for missing right side

      if (ld->sidenum[1] == NO_INDEX)
      {
	if (!demo_compatibility || !overflow[emu_missedbackside].enabled)
	ld->flags &= ~ML_TWOSIDED;  // Clear 2s flag for missing left side
      }

      // haleyjd 05/02/06: Reserved line flag. If set, we must clear all
      // BOOM or later extended line flags. This is necessitated by E2M7.
      if (ld->flags & ML_RESERVED && comp[comp_reservedlineflag])
        ld->flags &= 0x1FF;

      ld->frontsector = ld->sidenum[0]!=NO_INDEX ? sides[ld->sidenum[0]].sector : 0;
      ld->backsector  = ld->sidenum[1]!=NO_INDEX ? sides[ld->sidenum[1]].sector : 0;
      switch (ld->special)
        {                       // killough 4/11/98: handle special types
          int lump, j;

        case 260:               // killough 4/11/98: translucent 2s textures
            lump = sides[*ld->sidenum].special; // translucency from sidedef
            if (!ld->tag)                       // if tag==0,
              ld->tranlump = lump;              // affect this linedef only
            else
              for (j=0;j<numlines;j++)          // if tag!=0,
                if (lines[j].tag == ld->tag)    // affect all matching linedefs
                  lines[j].tranlump = lump;
            break;
        }
    }
}

//
// P_LoadSideDefs
//
// killough 4/4/98: split into two functions

void P_LoadSideDefs (int lump)
{
  numsides = W_LumpLength(lump) / sizeof(mapsidedef_t);
  sides = Z_Malloc(numsides*sizeof(side_t),PU_LEVEL,0);
  memset(sides, 0, numsides*sizeof(side_t));
}

// killough 4/4/98: delay using texture names until
// after linedefs are loaded, to allow overloading.
// killough 5/3/98: reformatted, cleaned up

void P_LoadSideDefs2(int lump)
{
  byte *data = W_CacheLumpNum(lump,PU_STATIC);
  int  i;

  for (i=0; i<numsides; i++)
    {
      register mapsidedef_t *msd = (mapsidedef_t *) data + i;
      register side_t *sd = sides + i;
      register sector_t *sec;

      sd->textureoffset = SHORT(msd->textureoffset)<<FRACBITS;
      sd->rowoffset = SHORT(msd->rowoffset)<<FRACBITS;
      // [crispy] smooth texture scrolling
      sd->oldtextureoffset = sd->textureoffset;
      sd->oldrowoffset = sd->rowoffset;
      sd->interptextureoffset = sd->textureoffset;
      sd->interprowoffset = sd->rowoffset;
      sd->oldgametic = -1;

      // killough 4/4/98: allow sidedef texture names to be overloaded
      // killough 4/11/98: refined to allow colormaps to work as wall
      // textures if invalid as colormaps but valid as textures.

      sd->sector = sec = &sectors[SHORT(msd->sector)];
      switch (sd->special)
        {
        case 2057: case 2058: case 2059: case 2060: case 2061: case 2062:
        case 2063: case 2064: case 2065: case 2066: case 2067: case 2068:
        case 2087: case 2088: case 2089: case 2090: case 2091: case 2092:
        case 2093: case 2094: case 2095: case 2096: case 2097: case 2098:
        {
          // All of the W1, WR, S1, SR, G1, GR activations can be triggered from
          // the back sidedef (reading the front bottom texture) and triggered
          // from the front sidedef (reading the front upper texture).
          for (int j = 0; j < numlines; j++)
          {
            if (lines[j].sidenum[0] == i)
            {
              // Back triggered
              if ((lines[j].backmusic = W_CheckNumForName(msd->bottomtexture)) < 0)
              {
                lines[j].backmusic = 0;
                sd->bottomtexture = R_TextureNumForName(msd->bottomtexture);
              }
              else
              {
                sd->bottomtexture = 0;
              }

              // Front triggered
              if ((lines[j].frontmusic = W_CheckNumForName(msd->toptexture)) < 0)
              {
                lines[j].frontmusic = 0;
                sd->toptexture = R_TextureNumForName(msd->toptexture);
              }
              else
              {
                sd->toptexture = 0;
              }
            }
          }
          sd->midtexture = R_TextureNumForName(msd->midtexture);
          break;
        }

        case 2075:
        // Sector tinting
        {
          for (int j = 0; j < numlines; j++)
          {
            if (lines[j].sidenum[0] == i)
            {
              // Front triggered
              if ((lines[j].fronttint = R_ColormapNumForName(msd->toptexture)) < 0)
              {
                lines[j].fronttint = 0;
                sd->toptexture = R_TextureNumForName(msd->toptexture);
              }
              else
              {
                sd->toptexture = 0;
              }
            }
          }
          sd->midtexture = R_TextureNumForName(msd->midtexture);
          sd->bottomtexture = R_TextureNumForName(msd->bottomtexture);
          break;
        }

        case 2076: case 2077: case 2078: case 2079: case 2080: case 2081:
        // Sector tinting
        // All of the W1, WR, S1, SR, G1, GR activations can be triggered from
        // the back sidedef (reading the front bottom texture) and triggered
        // from the front sidedef (reading the front upper texture).
        {
          for (int j = 0; j < numlines; j++)
          {
            if (lines[j].sidenum[0] == i)
            {
              // Back triggered
              if ((lines[j].backtint = R_ColormapNumForName(msd->bottomtexture)) < 0)
              {
                lines[j].backtint = 0;
                sd->bottomtexture = R_TextureNumForName(msd->bottomtexture);
              }
              else
              {
                sd->bottomtexture = 0;
              }

              // Front triggered
              if ((lines[j].fronttint = R_ColormapNumForName(msd->toptexture)) < 0)
              {
                lines[j].fronttint = 0;
                sd->toptexture = R_TextureNumForName(msd->toptexture);
              }
              else
              {
                sd->toptexture = 0;
              }
            }
          }
          sd->midtexture = R_TextureNumForName(msd->midtexture);
          break;
        }

        case 242:                       // variable colormap via 242 linedef
          sd->bottomtexture =
            (sec->bottommap =   R_ColormapNumForName(msd->bottomtexture)) < 0 ?
            sec->bottommap = 0, R_TextureNumForName(msd->bottomtexture): 0 ;
          sd->midtexture =
            (sec->midmap =   R_ColormapNumForName(msd->midtexture)) < 0 ?
            sec->midmap = 0, R_TextureNumForName(msd->midtexture)  : 0 ;
          sd->toptexture =
            (sec->topmap =   R_ColormapNumForName(msd->toptexture)) < 0 ?
            sec->topmap = 0, R_TextureNumForName(msd->toptexture)  : 0 ;
          break;

        case 260: // killough 4/11/98: apply translucency to 2s normal texture
          sd->midtexture = strncasecmp("TRANMAP", msd->midtexture, 8) ?
            (sd->special = W_CheckNumForName(msd->midtexture)) < 0 ||
            W_LumpLength(sd->special) != 65536 ?
            sd->special=0, R_TextureNumForName(msd->midtexture) :
              (sd->special++, 0) : (sd->special=0);
          sd->toptexture = R_TextureNumForName(msd->toptexture);
          sd->bottomtexture = R_TextureNumForName(msd->bottomtexture);
          break;

        default:                        // normal cases
          sd->midtexture = R_TextureNumForName(msd->midtexture);
          sd->toptexture = R_TextureNumForName(msd->toptexture);
          sd->bottomtexture = R_TextureNumForName(msd->bottomtexture);
          break;
        }
    }
  Z_Free (data);
}

#ifndef MBF_STRICT

// jff 10/6/98
// New code added to speed up calculation of internal blockmap
// Algorithm is order of nlines*(ncols+nrows) not nlines*ncols*nrows
//

#define blkshift 7               /* places to shift rel position for cell num */
#define blkmask ((1<<blkshift)-1)/* mask for rel position within cell */
#define blkmargin 0              /* size guardband around map used */
                                 // jff 10/8/98 use guardband>0
                                 // jff 10/12/98 0 ok with + 1 in rows,cols

typedef struct linelist_t        // type used to list lines in each block
{
  long num;
  struct linelist_t *next;
} linelist_t;

//
// Subroutine to add a line number to a block list
// It simply returns if the line is already in the block
//

static void AddBlockLine
(
  linelist_t **lists,
  int *count,
  int *done,
  int blockno,
  long lineno
)
{
  linelist_t *l;

  if (done[blockno])
    return;

  l = Z_Malloc(sizeof(linelist_t), PU_STATIC, 0);
  l->num = lineno;
  l->next = lists[blockno];
  lists[blockno] = l;
  count[blockno]++;
  done[blockno] = 1;
}

static void P_CreateBlockMap(void)
{
  int xorg,yorg;                 // blockmap origin (lower left)
  int nrows,ncols;               // blockmap dimensions
  linelist_t **blocklists=NULL;  // array of pointers to lists of lines
  int *blockcount=NULL;          // array of counters of line lists
  int *blockdone=NULL;           // array keeping track of blocks/line
  int NBlocks;                   // number of cells = nrows*ncols
  long linetotal=0;              // total length of all blocklists
  int i,j;
  int map_minx=INT_MAX;          // init for map limits search
  int map_miny=INT_MAX;
  int map_maxx=INT_MIN;
  int map_maxy=INT_MIN;

  // scan for map limits, which the blockmap must enclose

  // This fixes MBF's code, which has a bug where maxx/maxy
  // are wrong if the 0th node has the largest x or y
  if (numvertexes)
  {
    map_minx = map_maxx = vertexes[0].x;
    map_miny = map_maxy = vertexes[0].y;
  }

  for (i=0;i<numvertexes;i++)
  {
    fixed_t t;

    if ((t=vertexes[i].x) < map_minx)
      map_minx = t;
    else if (t > map_maxx)
      map_maxx = t;
    if ((t=vertexes[i].y) < map_miny)
      map_miny = t;
    else if (t > map_maxy)
      map_maxy = t;
  }
  map_minx >>= FRACBITS;    // work in map coords, not fixed_t
  map_maxx >>= FRACBITS;
  map_miny >>= FRACBITS;
  map_maxy >>= FRACBITS;

  // set up blockmap area to enclose level plus margin

  xorg = map_minx-blkmargin;
  yorg = map_miny-blkmargin;
  ncols = (map_maxx+blkmargin-xorg+1+blkmask)>>blkshift;  //jff 10/12/98
  nrows = (map_maxy+blkmargin-yorg+1+blkmask)>>blkshift;  //+1 needed for
  NBlocks = ncols*nrows;                                  //map exactly 1 cell

  // create the array of pointers on NBlocks to blocklists
  // also create an array of linelist counts on NBlocks
  // finally make an array in which we can mark blocks done per line

  // CPhipps - calloc's
  blocklists = Z_Calloc(NBlocks,sizeof(linelist_t *), PU_STATIC, 0);
  blockcount = Z_Calloc(NBlocks,sizeof(int), PU_STATIC, 0);
  blockdone = Z_Malloc(NBlocks*sizeof(int), PU_STATIC, 0);

  // initialize each blocklist, and enter the trailing -1 in all blocklists
  // note the linked list of lines grows backwards

  for (i=0;i<NBlocks;i++)
  {
    blocklists[i] = Z_Malloc(sizeof(linelist_t), PU_STATIC, 0);
    blocklists[i]->num = -1;
    blocklists[i]->next = NULL;
    blockcount[i]++;
  }

  // For each linedef in the wad, determine all blockmap blocks it touches,
  // and add the linedef number to the blocklists for those blocks

  for (i=0;i<numlines;i++)
  {
    int x1 = lines[i].v1->x>>FRACBITS;         // lines[i] map coords
    int y1 = lines[i].v1->y>>FRACBITS;
    int x2 = lines[i].v2->x>>FRACBITS;
    int y2 = lines[i].v2->y>>FRACBITS;
    int dx = x2-x1;
    int dy = y2-y1;
    int vert = !dx;                            // lines[i] slopetype
    int horiz = !dy;
    int spos = (dx^dy) > 0;
    int sneg = (dx^dy) < 0;
    int bx,by;                                 // block cell coords
    int minx = x1>x2? x2 : x1;                 // extremal lines[i] coords
    int maxx = x1>x2? x1 : x2;
    int miny = y1>y2? y2 : y1;
    int maxy = y1>y2? y1 : y2;

    // no blocks done for this linedef yet

    memset(blockdone,0,NBlocks*sizeof(int));

    // The line always belongs to the blocks containing its endpoints

    bx = (x1-xorg)>>blkshift;
    by = (y1-yorg)>>blkshift;
    AddBlockLine(blocklists,blockcount,blockdone,by*ncols+bx,i);
    bx = (x2-xorg)>>blkshift;
    by = (y2-yorg)>>blkshift;
    AddBlockLine(blocklists,blockcount,blockdone,by*ncols+bx,i);


    // For each column, see where the line along its left edge, which
    // it contains, intersects the Linedef i. Add i to each corresponding
    // blocklist.

    if (!vert)    // don't interesect vertical lines with columns
    {
      for (j=0;j<ncols;j++)
      {
        // intersection of Linedef with x=xorg+(j<<blkshift)
        // (y-y1)*dx = dy*(x-x1)
        // y = dy*(x-x1)+y1*dx;

        int x = xorg+(j<<blkshift);       // (x,y) is intersection
        int y = (dy*(x-x1))/dx+y1;
        int yb = (y-yorg)>>blkshift;      // block row number
        int yp = (y-yorg)&blkmask;        // y position within block

        if (yb<0 || yb>nrows-1)     // outside blockmap, continue
          continue;

        if (x<minx || x>maxx)       // line doesn't touch column
          continue;

        // The cell that contains the intersection point is always added

        AddBlockLine(blocklists,blockcount,blockdone,ncols*yb+j,i);

        // if the intersection is at a corner it depends on the slope
        // (and whether the line extends past the intersection) which
        // blocks are hit

        if (yp==0)        // intersection at a corner
        {
          if (sneg)       //   \ - blocks x,y-, x-,y
          {
            if (yb>0 && miny<y)
              AddBlockLine(blocklists,blockcount,blockdone,ncols*(yb-1)+j,i);
            if (j>0 && minx<x)
              AddBlockLine(blocklists,blockcount,blockdone,ncols*yb+j-1,i);
          }
          else if (spos)  //   / - block x-,y-
          {
            if (yb>0 && j>0 && minx<x)
              AddBlockLine(blocklists,blockcount,blockdone,ncols*(yb-1)+j-1,i);
          }
          else if (horiz) //   - - block x-,y
          {
            if (j>0 && minx<x)
              AddBlockLine(blocklists,blockcount,blockdone,ncols*yb+j-1,i);
          }
        }
        else if (j>0 && minx<x) // else not at corner: x-,y
          AddBlockLine(blocklists,blockcount,blockdone,ncols*yb+j-1,i);
      }
    }

    // For each row, see where the line along its bottom edge, which
    // it contains, intersects the Linedef i. Add i to all the corresponding
    // blocklists.

    if (!horiz)
    {
      for (j=0;j<nrows;j++)
      {
        // intersection of Linedef with y=yorg+(j<<blkshift)
        // (x,y) on Linedef i satisfies: (y-y1)*dx = dy*(x-x1)
        // x = dx*(y-y1)/dy+x1;

        int y = yorg+(j<<blkshift);       // (x,y) is intersection
        int x = (dx*(y-y1))/dy+x1;
        int xb = (x-xorg)>>blkshift;      // block column number
        int xp = (x-xorg)&blkmask;        // x position within block

        if (xb<0 || xb>ncols-1)   // outside blockmap, continue
          continue;

        if (y<miny || y>maxy)     // line doesn't touch row
          continue;

        // The cell that contains the intersection point is always added

        AddBlockLine(blocklists,blockcount,blockdone,ncols*j+xb,i);

        // if the intersection is at a corner it depends on the slope
        // (and whether the line extends past the intersection) which
        // blocks are hit

        if (xp==0)        // intersection at a corner
        {
          if (sneg)       //   \ - blocks x,y-, x-,y
          {
            if (j>0 && miny<y)
              AddBlockLine(blocklists,blockcount,blockdone,ncols*(j-1)+xb,i);
            if (xb>0 && minx<x)
              AddBlockLine(blocklists,blockcount,blockdone,ncols*j+xb-1,i);
          }
          else if (vert)  //   | - block x,y-
          {
            if (j>0 && miny<y)
              AddBlockLine(blocklists,blockcount,blockdone,ncols*(j-1)+xb,i);
          }
          else if (spos)  //   / - block x-,y-
          {
            if (xb>0 && j>0 && miny<y)
              AddBlockLine(blocklists,blockcount,blockdone,ncols*(j-1)+xb-1,i);
          }
        }
        else if (j>0 && miny<y) // else not on a corner: x,y-
          AddBlockLine(blocklists,blockcount,blockdone,ncols*(j-1)+xb,i);
      }
    }
  }

  // Add initial 0 to all blocklists
  // count the total number of lines (and 0's and -1's)

  memset(blockdone,0,NBlocks*sizeof(int));
  for (i=0,linetotal=0;i<NBlocks;i++)
  {
    AddBlockLine(blocklists,blockcount,blockdone,i,0);
    linetotal += blockcount[i];
  }

  // Create the blockmap lump

  //blockmaplump = malloc_IfSameLevel(blockmaplump, sizeof(*blockmaplump) * (4 + NBlocks + linetotal));
  blockmaplump = Z_Malloc(sizeof(*blockmaplump) * (4 + NBlocks + linetotal), PU_LEVEL, 0);

  // blockmap header

  blockmaplump[0] = bmaporgx = xorg << FRACBITS;
  blockmaplump[1] = bmaporgy = yorg << FRACBITS;
  blockmaplump[2] = bmapwidth  = ncols;
  blockmaplump[3] = bmapheight = nrows;

  // offsets to lists and block lists

  for (i=0;i<NBlocks;i++)
  {
    linelist_t *bl = blocklists[i];
    long offs = blockmaplump[4+i] =   // set offset to block's list
      (i? blockmaplump[4+i-1] : 4+NBlocks) + (i? blockcount[i-1] : 0);

    // add the lines in each block's list to the blockmaplump
    // delete each list node as we go

    while (bl)
    {
      linelist_t *tmp = bl->next;
      blockmaplump[offs++] = bl->num;
      Z_Free(bl);
      bl = tmp;
    }
  }

  // free all temporary storage

  Z_Free(blocklists);
  Z_Free(blockcount);
  Z_Free(blockdone);
}

#else // MBF_STRICT

//
// killough 10/98:
//
// Rewritten to use faster algorithm.
//
// New procedure uses Bresenham-like algorithm on the linedefs, adding the
// linedef to each block visited from the beginning to the end of the linedef.
//
// The algorithm's complexity is on the order of nlines*total_linedef_length.
//
// Please note: This section of code is not interchangable with TeamTNT's
// code which attempts to fix the same problem.

static void P_CreateBlockMap(void)
{
  register int i;
  fixed_t minx = INT_MAX, miny = INT_MAX, maxx = INT_MIN, maxy = INT_MIN;

  // First find limits of map

  for (i=0; i<numvertexes; i++)
    {
      if (vertexes[i].x >> FRACBITS < minx)
	minx = vertexes[i].x >> FRACBITS;
      else
	if (vertexes[i].x >> FRACBITS > maxx)
	  maxx = vertexes[i].x >> FRACBITS;
      if (vertexes[i].y >> FRACBITS < miny)
	miny = vertexes[i].y >> FRACBITS;
      else
	if (vertexes[i].y >> FRACBITS > maxy)
	  maxy = vertexes[i].y >> FRACBITS;
    }

  // Save blockmap parameters

  bmaporgx = minx << FRACBITS;
  bmaporgy = miny << FRACBITS;
  bmapwidth  = ((maxx-minx) >> MAPBTOFRAC) + 1;
  bmapheight = ((maxy-miny) >> MAPBTOFRAC) + 1;

  // Compute blockmap, which is stored as a 2d array of variable-sized lists.
  //
  // Pseudocode:
  //
  // For each linedef:
  //
  //   Map the starting and ending vertices to blocks.
  //
  //   Starting in the starting vertex's block, do:
  //
  //     Add linedef to current block's list, dynamically resizing it.
  //
  //     If current block is the same as the ending vertex's block, exit loop.
  //
  //     Move to an adjacent block by moving towards the ending block in 
  //     either the x or y direction, to the block which contains the linedef.

  {
    typedef struct { int n, nalloc, *list; } bmap_t;  // blocklist structure
    unsigned tot = bmapwidth * bmapheight;            // size of blockmap
    bmap_t *bmap = Z_Calloc(sizeof *bmap, tot, PU_STATIC, 0); // array of blocklists

    for (i=0; i < numlines; i++)
      {
	// starting coordinates
	int x = (lines[i].v1->x >> FRACBITS) - minx;
	int y = (lines[i].v1->y >> FRACBITS) - miny;
	
	// x-y deltas
	int adx = lines[i].dx >> FRACBITS, dx = adx < 0 ? -1 : 1;
	int ady = lines[i].dy >> FRACBITS, dy = ady < 0 ? -1 : 1; 

	// difference in preferring to move across y (>0) instead of x (<0)
	int diff = !adx ? 1 : !ady ? -1 :
	  (((x >> MAPBTOFRAC) << MAPBTOFRAC) + 
	   (dx > 0 ? MAPBLOCKUNITS-1 : 0) - x) * (ady = abs(ady)) * dx -
	  (((y >> MAPBTOFRAC) << MAPBTOFRAC) + 
	   (dy > 0 ? MAPBLOCKUNITS-1 : 0) - y) * (adx = abs(adx)) * dy;

	// starting block, and pointer to its blocklist structure
	int b = (y >> MAPBTOFRAC)*bmapwidth + (x >> MAPBTOFRAC);

	// ending block
	int bend = (((lines[i].v2->y >> FRACBITS) - miny) >> MAPBTOFRAC) *
	  bmapwidth + (((lines[i].v2->x >> FRACBITS) - minx) >> MAPBTOFRAC);

	// delta for pointer when moving across y
	dy *= bmapwidth;

	// deltas for diff inside the loop
	adx <<= MAPBTOFRAC;
	ady <<= MAPBTOFRAC;

	// Now we simply iterate block-by-block until we reach the end block.
	while ((unsigned) b < tot)    // failsafe -- should ALWAYS be true
	  {
	    // Increase size of allocated list if necessary
	    if (bmap[b].n >= bmap[b].nalloc)
	      bmap[b].list = realloc(bmap[b].list, 
				     (bmap[b].nalloc = bmap[b].nalloc ? 
				      bmap[b].nalloc*2 : 8)*sizeof*bmap->list);

	    // Add linedef to end of list
	    bmap[b].list[bmap[b].n++] = i;

	    // If we have reached the last block, exit
	    if (b == bend)
	      break;

	    // Move in either the x or y direction to the next block
	    if (diff < 0) 
	      diff += ady, b += dx;
	    else
	      diff -= adx, b += dy;
	  }
      }

    // Compute the total size of the blockmap.
    //
    // Compression of empty blocks is performed by reserving two offset words
    // at tot and tot+1.
    //
    // 4 words, unused if this routine is called, are reserved at the start.

    {
      int count = tot+6;  // we need at least 1 word per block, plus reserved's

      for (i = 0; i < tot; i++)
	if (bmap[i].n)
	  count += bmap[i].n + 2; // 1 header word + 1 trailer word + blocklist

      // Allocate blockmap lump with computed count
      blockmaplump = Z_Malloc(sizeof(*blockmaplump) * count, PU_LEVEL, 0);
    }									 

    // Now compress the blockmap.
    {
      int ndx = tot += 4;         // Advance index to start of linedef lists
      bmap_t *bp = bmap;          // Start of uncompressed blockmap

      blockmaplump[ndx++] = 0;    // Store an empty blockmap list at start
      blockmaplump[ndx++] = -1;   // (Used for compression)

      for (i = 4; i < tot; i++, bp++)
	if (bp->n)                                      // Non-empty blocklist
	  {
	    blockmaplump[blockmaplump[i] = ndx++] = 0;  // Store index & header
	    do
	      blockmaplump[ndx++] = bp->list[--bp->n];  // Copy linedef list
	    while (bp->n);
	    blockmaplump[ndx++] = -1;                   // Store trailer
	    Z_Free(bp->list);                           // Free linedef list
	  }
	else            // Empty blocklist: point to reserved empty blocklist
	  blockmaplump[i] = tot;

      Z_Free(bmap);    // Free uncompressed blockmap
    }
  }
}

#endif // MBF_STRICT

// Check if there is at least one block in BLOCKMAP
// which does not have 0 as the first item in the list

static void P_SetSkipBlockStart(void)
{
  int x, y;

  skipblstart = true;

  for(y = 0; y < bmapheight; y++)
    for(x = 0; x < bmapwidth; x++)
    {
      int32_t *list;
      int32_t *blockoffset;

      blockoffset = blockmaplump + y * bmapwidth + x + 4;

      list = blockmaplump + *blockoffset;

      if (*list != 0)
      {
        skipblstart = false;
        return;
      }
    }
}

//
// P_LoadBlockMap
//
// killough 3/1/98: substantially modified to work
// towards removing blockmap limit (a wad limitation)
//
// killough 3/30/98: Rewritten to remove blockmap limit
//

boolean P_LoadBlockMap (int lump)
{
  long count;
  boolean ret = true;

  //!
  // @category mod
  //
  // Forces a (re-)building of the BLOCKMAP lumps for loaded maps.
  //

  if (M_CheckParm("-blockmap") || (count = W_LumpLengthWithName(lump, "BLOCKMAP")/2) >= 0x10000 || count < 4) // [FG] always rebuild too short blockmaps
  {
    P_CreateBlockMap();
  }
  else
    {
      long i;
      short *wadblockmaplump = W_CacheLumpNum (lump, PU_LEVEL);
      blockmaplump = Z_Malloc(sizeof(*blockmaplump) * count, PU_LEVEL, 0);

      // killough 3/1/98: Expand wad blockmap into larger internal one,
      // by treating all offsets except -1 as unsigned and zero-extending
      // them. This potentially doubles the size of blockmaps allowed,
      // because Doom originally considered the offsets as always signed.

      blockmaplump[0] = SHORT(wadblockmaplump[0]);
      blockmaplump[1] = SHORT(wadblockmaplump[1]);
      blockmaplump[2] = (long)(SHORT(wadblockmaplump[2])) & FRACMASK;
      blockmaplump[3] = (long)(SHORT(wadblockmaplump[3])) & FRACMASK;

      for (i=4 ; i<count ; i++)
        {
          short t = SHORT(wadblockmaplump[i]);          // killough 3/1/98
          blockmaplump[i] = t == -1 ? -1l : (long) t & FRACMASK;
        }

      Z_Free(wadblockmaplump);

      bmaporgx = blockmaplump[0]<<FRACBITS;
      bmaporgy = blockmaplump[1]<<FRACBITS;
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

//
// P_GroupLines
// Builds sector line lists and subsector sector numbers.
// Finds block bounding boxes for sectors.
//
// killough 5/3/98: reformatted, cleaned up
// killough 8/24/98: rewrote to use faster algorithm

static void AddLineToSector(sector_t *s, line_t *l)
{
  M_AddToBox(s->blockbox, l->v1->x, l->v1->y);
  M_AddToBox(s->blockbox, l->v2->x, l->v2->y);
  *s->lines++ = l;
}

void P_DegenMobjThinker(mobj_t *mobj)
{
  // no-op
}

int P_GroupLines (void)
{
  int i, total;
  line_t **linebuffer;

  // look up sector number for each subsector
  for (i=0; i<numsubsectors; i++)
    subsectors[i].sector = segs[subsectors[i].firstline].sidedef->sector;

  // count number of lines in each sector
  for (i=0; i<numlines; i++)
    {
      lines[i].frontsector->linecount++;
      if (lines[i].backsector && lines[i].backsector != lines[i].frontsector)
	lines[i].backsector->linecount++;
    }

  // compute total number of lines and clear bounding boxes
  for (total=0, i=0; i<numsectors; i++)
    {
      total += sectors[i].linecount;
      M_ClearBox(sectors[i].blockbox);
    }

  // build line tables for each sector
  linebuffer = Z_Malloc(total * sizeof(*linebuffer), PU_LEVEL, 0);

  for (i=0; i<numsectors; i++)
    {
      sectors[i].lines = linebuffer;
      linebuffer += sectors[i].linecount;
    }
  
  for (i=0; i<numlines; i++)
    {
      AddLineToSector(lines[i].frontsector, &lines[i]);
      if (lines[i].backsector && lines[i].backsector != lines[i].frontsector)
	AddLineToSector(lines[i].backsector, &lines[i]);
    }

  for (i=0; i<numsectors; i++)
    {
      sector_t *sector = sectors+i;
      int block;

      // adjust pointers to point back to the beginning of each list
      sector->lines -= sector->linecount;

      // set the degenmobj_t to the middle of the bounding box
      sector->soundorg.x =
          sector->blockbox[BOXRIGHT] / 2 + sector->blockbox[BOXLEFT] / 2;
      sector->soundorg.y =
          sector->blockbox[BOXTOP] / 2 + sector->blockbox[BOXBOTTOM] / 2;

      sector->soundorg.thinker.function.p1 = P_DegenMobjThinker;

      // adjust bounding box to map blocks
      block = (sector->blockbox[BOXTOP]-bmaporgy+MAXRADIUS)>>MAPBLOCKSHIFT;
      block = block >= bmapheight ? bmapheight-1 : block;
      sector->blockbox[BOXTOP]=block;

      block = (sector->blockbox[BOXBOTTOM]-bmaporgy-MAXRADIUS)>>MAPBLOCKSHIFT;
      block = block < 0 ? 0 : block;
      sector->blockbox[BOXBOTTOM]=block;

      block = (sector->blockbox[BOXRIGHT]-bmaporgx+MAXRADIUS)>>MAPBLOCKSHIFT;
      block = block >= bmapwidth ? bmapwidth-1 : block;
      sector->blockbox[BOXRIGHT]=block;

      block = (sector->blockbox[BOXLEFT]-bmaporgx-MAXRADIUS)>>MAPBLOCKSHIFT;
      block = block < 0 ? 0 : block;
      sector->blockbox[BOXLEFT]=block;
    }
    return total;
}

//
// killough 10/98
//
// Remove slime trails.
//
// Slime trails are inherent to Doom's coordinate system -- i.e. there is
// nothing that a node builder can do to prevent slime trails ALL of the time,
// because it's a product of the integer coordinate system, and just because
// two lines pass through exact integer coordinates, doesn't necessarily mean
// that they will intersect at integer coordinates. Thus we must allow for
// fractional coordinates if we are to be able to split segs with node lines,
// as a node builder must do when creating a BSP tree.
//
// A wad file does not allow fractional coordinates, so node builders are out
// of luck except that they can try to limit the number of splits (they might
// also be able to detect the degree of roundoff error and try to avoid splits
// with a high degree of roundoff error). But we can use fractional coordinates
// here, inside the engine. It's like the difference between square inches and
// square miles, in terms of granularity.
//
// For each vertex of every seg, check to see whether it's also a vertex of
// the linedef associated with the seg (i.e, it's an endpoint). If it's not
// an endpoint, and it wasn't already moved, move the vertex towards the
// linedef by projecting it using the law of cosines. Formula:
//
//      2        2                         2        2
//    dx  x0 + dy  x1 + dx dy (y0 - y1)  dy  y0 + dx  y1 + dx dy (x0 - x1)
//   {---------------------------------, ---------------------------------}
//                  2     2                            2     2
//                dx  + dy                           dx  + dy
//
// (x0,y0) is the vertex being moved, and (x1,y1)-(x1+dx,y1+dy) is the
// reference linedef.
//
// Segs corresponding to orthogonal linedefs (exactly vertical or horizontal
// linedefs), which comprise at least half of all linedefs in most wads, don't
// need to be considered, because they almost never contribute to slime trails
// (because then any roundoff error is parallel to the linedef, which doesn't
// cause slime). Skipping simple orthogonal lines lets the code finish quicker.
//
// Please note: This section of code is not interchangable with TeamTNT's
// code which attempts to fix the same problem.
//
// Firelines (TM) is a Rezistered Trademark of MBF Productions
//

void P_RemoveSlimeTrails(void)                // killough 10/98
{
  byte *hit = Z_Calloc(1, numvertexes, PU_STATIC, 0);         // Hitlist for vertices
  int i;
  for (i=0; i<numsegs; i++)                   // Go through each seg
    {
      const line_t *l = segs[i].linedef;      // The parent linedef

      if (!segs[i].linedef)
        break; // Andrey Budko: probably 'continue;'?

      if (l->dx && l->dy)                     // We can ignore orthogonal lines
	{
	  vertex_t *v = segs[i].v1;
	  do
	    if (!hit[v - vertexes])           // If we haven't processed vertex
	      {
		hit[v - vertexes] = 1;        // Mark this vertex as processed
		if (v != l->v1 && v != l->v2) // Exclude endpoints of linedefs
		  { // Project the vertex back onto the parent linedef
		    int64_t dx2 = (l->dx >> FRACBITS) * (l->dx >> FRACBITS);
		    int64_t dy2 = (l->dy >> FRACBITS) * (l->dy >> FRACBITS);
		    int64_t dxy = (l->dx >> FRACBITS) * (l->dy >> FRACBITS);
		    int64_t s = dx2 + dy2;
		    int x0 = v->x, y0 = v->y, x1 = l->v1->x, y1 = l->v1->y;
		    // [FG] move vertex coordinates used for rendering
		    v->r_x = (fixed_t)((dx2 * x0 + dy2 * x1 + dxy * (y0 - y1)) / s);
		    v->r_y = (fixed_t)((dy2 * y0 + dx2 * y1 + dxy * (x0 - x1)) / s);

		    // [FG] override actual vertex coordinates except in compatibility mode
		    if (demo_version >= DV_MBF)
		    {
		      v->x = v->r_x;
		      v->y = v->r_y;
		    }

		    // [FG] wait a minute... moved more than 8 map units?
		    // maybe that's a Linguortal then, back to the original coordinates
		    if (abs(v->r_x - x0) > 8*FRACUNIT || abs(v->r_y - y0) > 8*FRACUNIT)
		    {
		      v->r_x = x0;
		      v->r_y = y0;
		    }
		  }
	      }  // Obfuscated C contest entry:   :)
	  while ((v != segs[i].v2) && (v = segs[i].v2));
	}
    }
  Z_Free(hit);
}

// [crispy] fix long wall wobble

static angle_t anglediff(angle_t a, angle_t b)
{
    if (b > a)
        return anglediff(b, a);

    if (a - b < ANG180)
        return a - b;
    else // [crispy] wrap around
        return b - a;
}

void P_SegLengths(boolean contrast_only)
{
    int i;
    const int rightangle = abs(finesine[(ANG60/2) >> ANGLETOFINESHIFT]);

    for (i = 0; i < numsegs; i++)
    {
        seg_t *li = segs+i;
        int64_t dx, dy;

        dx = li->v2->r_x - li->v1->r_x;
        dy = li->v2->r_y - li->v1->r_y;

        if (!contrast_only)
        {
            li->r_length = (uint32_t)(sqrt((double)dx*dx + (double)dy*dy)/2);

            // [crispy] re-calculate angle used for rendering
            viewx = li->v1->r_x;
            viewy = li->v1->r_y;
            li->r_angle = R_PointToAngleCrispy(li->v2->r_x, li->v2->r_y);
            // [crispy] more than just a little adjustment?
            // back to the original angle then
            if (anglediff(li->r_angle, li->angle) > ANG60/2)
            {
                li->r_angle = li->angle;
            }
        }

        // [crispy] smoother fake contrast
        if (!dy)
            li->fakecontrast = -LIGHTBRIGHT;
        else if (abs(finesine[li->r_angle >> ANGLETOFINESHIFT]) < rightangle)
            li->fakecontrast = -(LIGHTBRIGHT >> 1);
        else if (!dx)
            li->fakecontrast = LIGHTBRIGHT;
        else if (abs(finecosine[li->r_angle >> ANGLETOFINESHIFT]) < rightangle)
            li->fakecontrast = LIGHTBRIGHT >> 1;
        else
            li->fakecontrast = 0;
    }
}

// [FG] pad the REJECT table when the lump is too small

static boolean P_LoadReject(int lumpnum, int totallines)
{
    int minlength;
    int lumplen;
    boolean ret;

    // Calculate the size that the REJECT lump *should* be.

    minlength = (numsectors * numsectors + 7) / 8;

    // If the lump meets the minimum length, it can be loaded directly.
    // Otherwise, we need to allocate a buffer of the correct size
    // and pad it with appropriate data.

    lumplen = W_LumpLengthWithName(lumpnum, "REJECT");

    if (lumplen >= minlength)
    {
        rejectmatrix = W_CacheLumpNum(lumpnum, PU_LEVEL);
        ret = false;
    }
    else
    {
        unsigned int padvalue;

        rejectmatrix = Z_Malloc(minlength, PU_LEVEL, (void **) &rejectmatrix);
        W_ReadLump(lumpnum, rejectmatrix);

        if (M_CheckParm("-reject_pad_with_ff"))
        {
            padvalue = 0xff;
        }
        else
        {
            padvalue = 0x00;
        }

        memset(rejectmatrix + lumplen, padvalue, minlength - lumplen);

        if (demo_compatibility && overflow[emu_reject].enabled)
        {
            unsigned int i;
            unsigned int byte_num;
            byte *dest;

            unsigned int rejectpad[4] =
            {
                0,                               // Size
                0,                               // Part of z_zone block header
                50,                              // PU_LEVEL
                0x1d4a11                         // DOOM_CONST_ZONEID
            };

            overflow[emu_reject].triggered = true;

            rejectpad[0] = ((totallines * 4 + 3) & ~3) + 24;

            // Copy values from rejectpad into the destination array.

            dest = rejectmatrix + lumplen;

            for (i = 0; i < (minlength - lumplen) && i < sizeof(rejectpad); ++i)
            {
                byte_num = i % 4;
                *dest = (rejectpad[i / 4] >> (byte_num * 8)) & 0xff;
                ++dest;
            }
        }

        ret = true;
    }

    return ret;
}

//
// P_SetupLevel
//
// killough 5/3/98: reformatted, cleaned up

// fast-forward demo to the next map
boolean playback_nextlevel = false;

void P_SetupLevel(int episode, int map, int playermask, skill_t skill)
{
  int   i;
  char  lumpname[9];
  int   lumpnum;
  mapformat_t mapformat;
  boolean gen_blockmap, pad_reject;

  totalkills = totalitems = totalsecret = wminfo.maxfrags = 0;
  max_kill_requirement = 0;
  wminfo.partime = 180;
  for (i=0; i<MAXPLAYERS; i++)
  {
    players[i].killcount = players[i].secretcount = players[i].itemcount = 0;
    players[i].maxkilldiscount = 0;
  }

  // Initial height of PointOfView will be set by player think.
  players[consoleplayer].viewz = 1;

  // [FG] fast-forward demo to the desired map
  if (playback_warp == map || playback_nextlevel)
  {
    if (!playback_skiptics)
      G_EnableWarp(false);

    playback_warp = -1;
    playback_nextlevel = false;
  }

  // Make sure all sounds are stopped before Z_FreeTags.
  S_Start();

  Z_FreeTag(PU_LEVEL);
  M_ClearArena(thinkers_arena);
  M_ClearArena(msecnodes_arena);

  Z_FreeTag(PU_CACHE);

  P_InitThinkers();

  // if working with a devlopment map, reload it
  //    W_Reload ();     killough 1/31/98: W_Reload obsolete

  // find map name
  M_CopyLumpName(lumpname, MapName(episode, map));

  lumpnum = W_GetNumForName(lumpname);

  G_ApplyLevelCompatibility(lumpnum);

  leveltime = 0;
  oldleveltime = 0;

  // note: most of this ordering is important

  // killough 3/1/98: P_LoadBlockMap call moved down to below
  // killough 4/4/98: split load of sidedefs into two parts,
  // to allow texture names to be used in special linedefs

  // [FG] check nodes format
  mapformat = P_CheckMapFormat(lumpnum);

  P_LoadVertexes  (lumpnum+ML_VERTEXES);
  P_LoadSectors   (lumpnum+ML_SECTORS);
  P_LoadSideDefs  (lumpnum+ML_SIDEDEFS);             // killough 4/4/98
  P_LoadLineDefs  (lumpnum+ML_LINEDEFS);             //       |
  P_LoadSideDefs2 (lumpnum+ML_SIDEDEFS);             //       |
  P_LoadLineDefs2 (lumpnum+ML_LINEDEFS);             // killough 4/4/98
  gen_blockmap = P_LoadBlockMap  (lumpnum+ML_BLOCKMAP);             // killough 3/1/98
  // [FG] build nodes with NanoBSP
  if (mapformat >= MFMT_UNSUPPORTED)
  {
    BSP_BuildNodes();
  }
  // [FG] support maps with NODES in uncompressed XNOD/XGLN or compressed ZNOD/ZGLN formats, or DeePBSP format
  else if (mapformat == MFMT_XGLN || mapformat == MFMT_ZGLN)
  {
    P_LoadNodes_XNOD (lumpnum+ML_SSECTORS, mapformat == MFMT_ZGLN, true);
  }
  else if (mapformat == MFMT_XNOD || mapformat == MFMT_ZNOD)
  {
    P_LoadNodes_XNOD (lumpnum+ML_NODES, mapformat == MFMT_ZNOD, false);
  }
  else if (mapformat == MFMT_DEEP)
  {
    P_LoadSubsectors_DEEP (lumpnum+ML_SSECTORS);
    P_LoadNodes_DEEP (lumpnum+ML_NODES);
    P_LoadSegs_DEEP (lumpnum+ML_SEGS);
  }
  else
  {
  P_LoadSubsectors(lumpnum+ML_SSECTORS);
  P_LoadNodes     (lumpnum+ML_NODES);
  P_LoadSegs      (lumpnum+ML_SEGS);
  }

  // [FG] pad the REJECT table when the lump is too small
  pad_reject = P_LoadReject (lumpnum+ML_REJECT, P_GroupLines());

  if (mapformat != MFMT_UNSUPPORTED)
    P_RemoveSlimeTrails();    // killough 10/98: remove slime trails from wad

  // [crispy] fix long wall wobble
  P_SegLengths(false);

  // Note: you don't need to clear player queue slots --
  // a much simpler fix is in g_game.c -- killough 10/98

  bodyqueslot = 0;
  deathmatch_p = deathmatchstarts;
  P_MapStart();
  P_LoadThings(lumpnum+ML_THINGS);

  // if deathmatch, randomly spawn the active players
  if (deathmatch)
    for (i=0; i<MAXPLAYERS; i++)
      if (playeringame[i])
        {
          players[i].mo = NULL;
          G_DeathMatchSpawnPlayer(i);
        }

  // killough 3/26/98: Spawn icon landings:
  if (gamemode==commercial)
    P_SpawnBrainTargets();

  // [crispy] support MUSINFO lump (dynamic music changing)
  if (gamemode != shareware)
  {
    S_ParseMusInfo(lumpname);
  }

  // clear special respawning que
  iquehead = iquetail = 0;

  // SKYDEFS flatmapping needs to be loaded before 271/272 transfers
  R_InitSkyMap();

  // set up world state
  P_SpawnSpecials();
  P_MapEnd();

  // preload graphics
  if (precache)
    R_PrecacheLevel();

  // [FG] log level setup
  I_Printf(VB_DEMO, "P_SetupLevel: %.8s (%s), Skill %d, %s%s%s, %s",
    lumpname, W_WadNameForLump(lumpnum),
    gameskill + 1,
    mapformat >= MFMT_UNSUPPORTED ? "NanoBSP" :
    mapformat == MFMT_XNOD ? "XNOD" :
    mapformat == MFMT_ZNOD ? "ZNOD" :
    mapformat == MFMT_XGLN ? "XGLN" :
    mapformat == MFMT_ZGLN ? "ZGLN" :
    mapformat == MFMT_DEEP ? "DeepBSP" :
    "Doom",
    gen_blockmap ? "+Blockmap" : "",
    pad_reject ? "+Reject" : "",
    G_GetCurrentComplevelName());
}

//
// P_Init
//
void P_Init (void)
{
  P_InitSwitchList();
  P_InitPicAnims();
  R_InitSprites(sprnames);

  #define SIZE_MB(x) ((x) * 1024 * 1024)
  thinkers_arena = M_InitArena(SIZE_MB(256), SIZE_MB(2));
  msecnodes_arena = M_InitArena(SIZE_MB(32), SIZE_MB(1));
  activeceilings_arena = M_InitArena(SIZE_MB(32), SIZE_MB(1));
  activeplats_arena = M_InitArena(SIZE_MB(32), SIZE_MB(1));
  #undef SIZE_MB
}

//----------------------------------------------------------------------------
//
// $Log: p_setup.c,v $
// Revision 1.16  1998/05/07  00:56:49  killough
// Ignore translucency lumps that are not exactly 64K long
//
// Revision 1.15  1998/05/03  23:04:01  killough
// beautification
//
// Revision 1.14  1998/04/12  02:06:46  killough
// Improve 242 colomap handling, add translucent walls
//
// Revision 1.13  1998/04/06  04:47:05  killough
// Add support for overloading sidedefs for special uses
//
// Revision 1.12  1998/03/31  10:40:42  killough
// Remove blockmap limit
//
// Revision 1.11  1998/03/28  18:02:51  killough
// Fix boss spawner savegame crash bug
//
// Revision 1.10  1998/03/20  00:30:17  phares
// Changed friction to linedef control
//
// Revision 1.9  1998/03/16  12:35:36  killough
// Default floor light level is sector's
//
// Revision 1.8  1998/03/09  07:21:48  killough
// Remove use of FP for point/line queries and add new sector fields
//
// Revision 1.7  1998/03/02  11:46:10  killough
// Double blockmap limit, prepare for when it's unlimited
//
// Revision 1.6  1998/02/27  11:51:05  jim
// Fixes for stairs
//
// Revision 1.5  1998/02/17  22:58:35  jim
// Fixed bug of vanishinb secret sectors in automap
//
// Revision 1.4  1998/02/02  13:38:48  killough
// Comment out obsolete reload hack
//
// Revision 1.3  1998/01/26  19:24:22  phares
// First rev with no ^Ms
//
// Revision 1.2  1998/01/26  05:02:21  killough
// Generalize and simplify level name generation
//
// Revision 1.1.1.1  1998/01/19  14:03:00  rand
// Lee's Jan 19 sources
//
//----------------------------------------------------------------------------
