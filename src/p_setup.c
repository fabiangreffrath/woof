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

#include "doomdef.h"
#include "doomstat.h"
#include "doomtype.h"
#include "g_game.h"
#include "g_compatibility.h"
#include "i_printf.h"
#include "i_system.h"
#include "info.h"
#include "m_arena.h"
#include "m_fixed.h"
#include "m_misc.h"
#include "p_enemy.h"
#include "p_bsp.h"
#include "p_map.h"
#include "p_maputl.h"
#include "p_mobj.h"
#include "p_setup.h"
#include "p_spec.h"
#include "p_tick.h"
#include "p_udmf.h"
#include "r_data.h"
#include "r_defs.h"
#include "r_sky.h"
#include "r_main.h"
#include "r_state.h"
#include "r_things.h"
#include "r_tranmap.h"
#include "s_musinfo.h" // [crispy] S_ParseMusInfo()
#include "s_sound.h"
#include "tables.h"
#include "w_wad.h"
#include "z_zone.h"

statenum_t *seenstate_tab = NULL;

// Detect map Format currently being set up.
// Different map formats
static const char *const map_format_names[] = {
    [MAP_NONE] = "Invalid",
    [MAP_DOOM] = "Doom",
    [MAP_HEXEN] = "Hexen",
    [MAP_UDMF] = "UDMF",
};

static const char *const bsp_format_names[] = {
    [BSP_DOOMBSP] = "DoomBSP", [BSP_DEEPBSPV4] = "DeepBSPV4",
    [BSP_XNOD] = "XNOD",       [BSP_ZNOD] = "ZNOD",
    [BSP_XGLN] = "XGLN",       [BSP_ZGLN] = "ZGLN",
    [BSP_XGL2] = "XGL2",       [BSP_ZGL2] = "ZGL2",
    [BSP_XGL3] = "XGL3",       [BSP_ZGL3] = "ZGL3",
    [BSP_NANO] = "NanoBSP"};

// Appended to node_format_names, hence the plus sign
static const char *const bmap_format_names[] = {
    [BMAP_DoomBlockmap] = "",
    [BMAP_XBM1] = "+XBM1",
    [BMAP_BoomBuilder] = "+BoomBlockmap",
};

map_t map = {0};

//
// MAP related Lookup tables.
// Store VERTEXES, LINEDEFS, SIDEDEFS, etc.
//

uint32_t numvertexes;
vertex_t *vertexes;

uint32_t numsegs;
seg_t    *segs;

uint32_t numsectors;
sector_t *sectors;

uint32_t numsubsectors;
subsector_t *subsectors;

uint32_t numnodes;
node_t   *nodes;

uint32_t numlines;
line_t   *lines;

uint32_t numsides;
side_t   *sides;

uint32_t *sslines_indexes;
ssline_t *sslines;

arena_t *world_arena;

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

// GetSectorAtNullAddress

sector_t* P_GetSectorAtNullAddress(void)
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
  byte *hit = Z_Calloc(numvertexes, sizeof(*hit), PU_STATIC, 0); // Hitlist for vertices
  for (size_t i = 0; i < numsegs; i++)                   // Go through each seg
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

void P_SegLengths(void)
{
    for (int32_t i = 0; i < numsegs; i++)
    {
        seg_t *li = segs+i;

        int64_t dx = li->v2->r_x - li->v1->r_x;
        int64_t dy = li->v2->r_y - li->v1->r_y;
        sidedef_flags_t flags = (li->sidedef) // mini segs!!
                              ? li->sidedef->flags
                              : SF_NONE;

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

        if (flags & SF_NO_FAKE_CONTRAST)
        {
            li->fakecontrast = 0;
        }
        else
        {
            // vanilla
            if (!dy)
              li->fakecontrast = -1;
            else if (!dx)
              li->fakecontrast = +1;
        }
    }
}

//
// P_SetupLevel
//
// killough 5/3/98: reformatted, cleaned up

// fast-forward demo to the next map
boolean playback_nextlevel = false;

// check for different supported and unsupported formats
static void CheckMapFormat(int lumpnum, map_t *map)
{
    map->map_format = MAP_NONE;
    map->bsp_format = BSP_NANO;
    map->bmap_format = BMAP_BoomBuilder;
    map->param = false;

    map->label = lumpnum;
    map->vertexes = NO_INDEX;
    map->linedefs = NO_INDEX;
    map->sidedefs = NO_INDEX;
    map->sectors = NO_INDEX;
    map->things = NO_INDEX;
    map->textmap = NO_INDEX;
    map->nodes = NO_INDEX;
    map->ssectors = NO_INDEX;
    map->segs = NO_INDEX;
    map->znodes = NO_INDEX;
    map->blockmap = NO_INDEX;
    map->reject = NO_INDEX;
    map->behavior = NO_INDEX;
    map->dialogue = NO_INDEX;
    map->lightmap = NO_INDEX;

    // Fully built map
    if (W_LumpExistsWithName(lumpnum + ML_THINGS, "THINGS")
        && W_LumpExistsWithName(lumpnum + ML_LINEDEFS, "LINEDEFS")
        && W_LumpExistsWithName(lumpnum + ML_SIDEDEFS, "SIDEDEFS")
        && W_LumpExistsWithName(lumpnum + ML_VERTEXES, "VERTEXES")
        && W_LumpExistsWithName(lumpnum + ML_SEGS, "SEGS")
        && W_LumpExistsWithName(lumpnum + ML_SSECTORS, "SSECTORS")
        && W_LumpExistsWithName(lumpnum + ML_NODES, "NODES")
        && W_LumpExistsWithName(lumpnum + ML_SECTORS, "SECTORS")
        && W_LumpExistsWithName(lumpnum + ML_REJECT, "REJECT")
        && W_LumpExistsWithName(lumpnum + ML_BLOCKMAP, "BLOCKMAP"))
    {
        map->map_format = MAP_DOOM;
        map->things = lumpnum + ML_THINGS;
        map->linedefs = lumpnum + ML_LINEDEFS;
        map->sidedefs = lumpnum + ML_SIDEDEFS;
        map->vertexes = lumpnum + ML_VERTEXES;
        map->segs = lumpnum + ML_SEGS;
        map->ssectors = lumpnum + ML_SSECTORS;
        map->nodes = lumpnum + ML_NODES;
        map->sectors = lumpnum + ML_SECTORS;
        map->reject = lumpnum + ML_REJECT;
        map->blockmap = lumpnum + ML_BLOCKMAP;

        // [FG] check nodes format
        P_CheckBSPFormat_Binary(map);

        if (W_LumpExistsWithName(lumpnum + ML_BEHAVIOR, "BEHAVIOR"))
        {
            map->map_format = MAP_HEXEN;
            map->param = true;
            map->behavior = lumpnum + ML_BEHAVIOR;
        }
    }

    // Non built map
    if (map->map_format == MAP_NONE
        && W_LumpExistsWithName(lumpnum + MLX_THINGS, "THINGS")
        && W_LumpExistsWithName(lumpnum + MLX_LINEDEFS, "LINEDEFS")
        && W_LumpExistsWithName(lumpnum + MLX_SIDEDEFS, "SIDEDEFS")
        && W_LumpExistsWithName(lumpnum + MLX_VERTEXES, "VERTEXES")
        && W_LumpExistsWithName(lumpnum + MLX_SECTORS, "SECTORS"))
    {
        map->map_format = MAP_DOOM;
        map->things = lumpnum + MLX_THINGS;
        map->linedefs = lumpnum + MLX_LINEDEFS;
        map->sidedefs = lumpnum + MLX_SIDEDEFS;
        map->vertexes = lumpnum + MLX_VERTEXES;
        map->sectors = lumpnum + MLX_SECTORS;

        map->bsp_format = BSP_NANO;

        if (W_LumpExistsWithName(lumpnum + MLX_BEHAVIOR, "BEHAVIOR"))
        {
            map->map_format = MAP_HEXEN;
            map->param = true;
            map->behavior = lumpnum + MLX_BEHAVIOR;
        }
    }

    if (W_LumpExistsWithName(lumpnum + ML_TEXTMAP, "TEXTMAP"))
    {
        map->map_format = MAP_UDMF;
        map->textmap = lumpnum + ML_TEXTMAP;

        // skip label and TEXTMAP, test against all other lumps until ENDMAP
        for (int i = ML_TEXTMAP + 1; i < ML_MAPLUMPCOUNT; ++i)
        {
            int j = lumpnum + i;
            if (W_LumpExistsWithName(j, "ENDMAP"))
            {
                break;
            }
            else if (W_LumpExistsWithName(j, "ZNODES"))
            {
                map->znodes = j;
            }
            else if (W_LumpExistsWithName(j, "REJECT"))
            {
                map->reject = j;
            }
            else if (W_LumpExistsWithName(j, "BLOCKMAP"))
            {
                map->blockmap = j;
            }
            else if (W_LumpExistsWithName(j, "BEHAVIOR"))
            {
                map->behavior = j;
            }
            else if (W_LumpExistsWithName(j, "DIALOGUE"))
            {
                map->dialogue = j;
            }
            else if (W_LumpExistsWithName(j, "LIGHTMAP"))
            {
                map->lightmap = j;
            }
        }

        // [FG] check nodes format
        P_CheckBSPFormat_UDMF(map);
    }
}

void P_SetupLevel(int episode, int map_num, skill_t skill, boolean from_savegame)
{
  char  lumpname[9];
  int   lumpnum;

  totalkills = totalitems = totalsecret = wminfo.maxfrags = 0;
  max_kill_requirement = 0;
  wminfo.partime = 180;
  for (int i = 0; i < MAXPLAYERS; i++)
  {
    players[i].killcount = players[i].secretcount = players[i].itemcount = 0;
    players[i].maxkilldiscount = 0;
  }

  // Initial height of PointOfView will be set by player think.
  players[consoleplayer].viewz = 1;

  // [FG] fast-forward demo to the desired map
  if (playback_warp == map_num || playback_nextlevel)
  {
    if (!playback_skiptics)
      G_EnableWarp(false);

    playback_warp = -1;
    playback_nextlevel = false;
  }

  // Make sure all sounds are stopped before Z_FreeTags.
  S_Reset();

  // do not start level music yet
  if (!from_savegame)
  {
    S_Start();
  }

  Z_FreeTag(PU_LEVEL);
  M_ArenaClear(world_arena);
  M_ArenaClear(thinkers_arena);
  M_ArenaClear(msecnodes_arena);

  Z_FreeTag(PU_CACHE);

  P_InitThinkers();
  // haleyjd 02/02/04 -- clear the TID hash table
  P_InitTIDHash();

  // if working with a devlopment map, reload it
  //    W_Reload ();     killough 1/31/98: W_Reload obsolete

  // find map name
  M_CopyLumpName(lumpname, MapName(episode, map_num));

  lumpnum = W_GetNumForName(lumpname);

  CheckMapFormat(lumpnum, &map);
  G_ApplyLevelCompatibility(&map);

  leveltime = 0;
  oldleveltime = 0;

  switch (map.map_format)
  {
    case MAP_DOOM:
      P_LoadMap_Doom(&map);
      break;
    case MAP_HEXEN:
      I_Error("Unsupported Hexen level format in %s", lumpname);
      break;
    case MAP_UDMF:
      P_LoadMap_UDMF(&map);
      break;
    case MAP_NONE:
      I_Error("Unknown level format in %s", lumpname);
      break;
  }

  // P_CrossSubsector optimization
  P_InitSubsectorLines();

  // XGL3/ZGL3 provide high-precision partition lines
  if (map.bsp_format >= BSP_XGL3)
  {
    R_PointOnSide = R_PointOnSide_Precise;
  }
  else
  {
    R_PointOnSide = R_PointOnSide_Classic;
  }

  if (map.bsp_format != BSP_NANO)
  {
    P_RemoveSlimeTrails();    // killough 10/98: remove slime trails from wad
  }

  // [crispy] fix long wall wobble
  P_SegLengths();

  // Note: you don't need to clear player queue slots --
  // a much simpler fix is in g_game.c -- killough 10/98

  bodyqueslot = 0;
  deathmatch_p = deathmatchstarts;
  P_MapStart();

  switch (map.map_format)
  {
    case MAP_DOOM:
      P_LoadThings_Doom(&map);
      break;
    case MAP_HEXEN:
      I_Error("Tried to spawn things on invalid map format");
      break;
    case MAP_UDMF:
      P_LoadThings_UDMF(&map);
      P_ClearMemory_UDMF(&map); // done with internal UDMF representation
      break;
    case MAP_NONE:
      I_Error("Tried to spawn things on invalid map format");
      break;
  }

  // if deathmatch, randomly spawn the active players
  if (deathmatch)
  {
    for (int i = 0; i < MAXPLAYERS; i++)
      if (playeringame[i])
        {
          players[i].mo = NULL;
          G_DeathMatchSpawnPlayer(i);
        }
  }
  else // if !deathmatch, check all necessary player starts actually exist
  {
    for (int i = 0; i < MAXPLAYERS; i++)
      if (playeringame[i] && !players[i].mo)
        I_Error("missing player %d start", i + 1);
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
  I_Printf(VB_DEMO, "P_SetupLevel: %.8s (%s), Skill %d, %s (%s%s%s), %s",
    lumpname, W_WadNameForLump(lumpnum),
    gameskill + 1,
    map_format_names[map.map_format],
    bsp_format_names[map.bsp_format],
    bmap_format_names[map.bmap_format],
    map.reject_built ? "+Reject" : "",
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
  world_arena = M_ArenaInit(SIZE_MB(128), SIZE_MB(4));
  thinkers_arena = M_ArenaInit(SIZE_MB(128), SIZE_MB(2));
  msecnodes_arena = M_ArenaInit(SIZE_MB(32), SIZE_MB(1));
  activeceilings_arena = M_ArenaInit(SIZE_MB(32), SIZE_MB(1));
  activeplats_arena = M_ArenaInit(SIZE_MB(32), SIZE_MB(1));
  #undef SIZE_MB

  seenstate_tab = calloc(num_states, sizeof(*seenstate_tab));
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
