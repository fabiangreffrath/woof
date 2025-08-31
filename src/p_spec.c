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
//   -Loads and initializes texture and flat animation sequences
//   -Implements utility functions for all linedef/sector special handlers
//   -Dispatches walkover and gun line triggers
//   -Initializes and implements special sector types
//   -Implements donut linedef triggers
//   -Initializes and implements BOOM linedef triggers for
//     Scrollers/Conveyors
//     Friction
//     Wind/Current
//
//-----------------------------------------------------------------------------

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "d_deh.h"
#include "d_player.h"
#include "doomdata.h"
#include "doomstat.h"
#include "g_game.h"
#include "hu_obituary.h"
#include "i_system.h"
#include "info.h"
#include "m_argv.h"
#include "m_bbox.h" // phares 3/20/98
#include "m_misc.h"
#include "m_random.h"
#include "m_swap.h"
#include "p_inter.h"
#include "p_map.h"
#include "p_maputl.h"
#include "p_mobj.h"
#include "p_setup.h"
#include "p_spec.h"
#include "p_tick.h"
#include "r_data.h"
#include "r_defs.h"
#include "r_draw.h" // R_SetFuzzPosTic
#include "r_main.h"
#include "r_plane.h" // killough 10/98
#include "r_sky.h"   // R_GetSkyColor
#include "r_state.h"
#include "r_swirl.h"
#include "s_sound.h"
#include "s_musinfo.h"
#include "sounds.h"
#include "st_stuff.h"
#include "st_widgets.h"
#include "tables.h"
#include "w_wad.h"
#include "z_zone.h"

//
// Animating textures and planes
// There is another anim_t used in wi_stuff, unrelated.
//

typedef enum
{
  terrain_solid,
  terrain_water,
  terrain_slime,
  terrain_lava
} terrain_t;

typedef struct
{
  boolean     istexture;
  int         picnum;
  int         basepic;
  int         numpics;
  int         speed;
} anim_t;

#if defined(_MSC_VER)
#pragma pack(push, 1)
#endif

//
//      source animation definition
//
typedef PACKED_PREFIX struct
{
  // [FG] signed char!
  signed char istexture;            //jff 3/23/98 make char for comparison
  char endname[9];           //  if false, it is a flat
  char startname[9];
  int  speed;
} PACKED_SUFFIX animdef_t; //jff 3/23/98 pack to read from memory

#if defined(_MSC_VER)
#pragma pack(pop)
#endif

#define MAXANIMS 32                   // no longer a strict limit -- killough
static anim_t *lastanim, *anims;      // new structure w/o limits -- killough
static size_t maxanims;

// killough 3/7/98: Initialize generalized scrolling
static void P_SpawnScrollers(void);
static void P_SpawnFriction(void);    // phares 3/16/98
static void P_SpawnPushers(void);     // phares 3/20/98

//
// P_InitPicAnims
//
// Load the table of animation definitions, checking for existence of
// the start and end of each frame. If the start doesn't exist the sequence
// is skipped, if the last doesn't exist, BOOM exits.
//
// Wall/Flat animation sequences, defined by name of first and last frame,
// The full animation sequence is given using all lumps between the start
// and end entry, in the order found in the WAD file.
//
// This routine modified to read its data from a predefined lump or
// PWAD lump called ANIMATED rather than a static table in this module to
// allow wad designers to insert or modify animation sequences.
//
// Lump format is an array of byte packed animdef_t structures, terminated
// by a structure with istexture == -1. The lump can be generated from a
// text source file using SWANTBLS.EXE, distributed with the BOOM utils.
// The standard list of switches and animations is contained in the example
// source text file DEFSWANI.DAT also in the BOOM util distribution.
//
//
void P_InitPicAnims (void)
{
  int         i;
  animdef_t   *animdefs; //jff 3/23/98 pointer to animation lump
  //  Init animation

  //jff 3/23/98 read from predefined or wad lump instead of table
  animdefs = W_CacheLumpName("ANIMATED",PU_STATIC);

  lastanim = anims;
  for (i=0 ; animdefs[i].istexture != -1 ; i++)
    {
      // 1/11/98 killough -- removed limit by array-doubling
      if (!anims || lastanim >= anims + maxanims)
        {
          size_t newmax = maxanims ? maxanims*2 : MAXANIMS;
          anims = Z_Realloc(anims, newmax*sizeof(*anims), PU_STATIC, 0);   // killough
          lastanim = anims + maxanims;
          maxanims = newmax;
        }

      if (animdefs[i].istexture)
        {
          // different episode ?
          if (R_CheckTextureNumForName(animdefs[i].startname) == -1)
            continue;

          lastanim->picnum = R_TextureNumForName (animdefs[i].endname);
          lastanim->basepic = R_TextureNumForName (animdefs[i].startname);
        }
      else
        {
          if ((W_CheckNumForName)(animdefs[i].startname, ns_flats) == -1)  // killough 4/17/98
            continue;

          lastanim->picnum = R_FlatNumForName (animdefs[i].endname);
          lastanim->basepic = R_FlatNumForName (animdefs[i].startname);

          if (lastanim->picnum >= lastanim->basepic)
          {
            char *startname;
            terrain_t terrain;
            int j;

            startname = M_StringDuplicate(animdefs[i].startname);
            M_StringToUpper(startname);

            // [FG] play sound when hitting animated floor
            if (strstr(startname, "WATER") || strstr(startname, "BLOOD"))
              terrain = terrain_water;
            else if (strstr(startname, "NUKAGE") || strstr(startname, "SLIME"))
              terrain = terrain_slime;
            else if (strstr(startname, "LAVA"))
              terrain = terrain_lava;
            else
              terrain = terrain_solid;

            free(startname);

            for (j = lastanim->basepic; j <= lastanim->picnum; j++)
            {
              flatterrain[j] = terrain;
            }
          }
        }

      lastanim->istexture = animdefs[i].istexture;
      lastanim->numpics = lastanim->picnum - lastanim->basepic + 1;
      lastanim->speed = LONG(animdefs[i].speed); // killough 5/5/98: add LONG()

      // [crispy] add support for SMMU swirling flats
      if (lastanim->speed < 65536 && lastanim->numpics != 1)
      {
      if (lastanim->numpics < 2)
        I_Error ("bad cycle from %s to %s",
                 animdefs[i].startname,
                 animdefs[i].endname);
      }

      if (lastanim->speed == 0)
        I_Error ("%s to %s animation cannot have speed 0",
                 animdefs[i].startname,
                 animdefs[i].endname);

      lastanim++;
    }
  Z_ChangeTag (animdefs,PU_CACHE); //jff 3/23/98 allow table to be freed
}

// [FG] play sound when hitting animated floor
void P_HitFloor (mobj_t *mo, int oof)
{
  const short floorpic = mo->subsector->sector->floorpic;
  terrain_t terrain = flatterrain[floorpic];

  int hitsound[][2] = {
    {sfx_None,   sfx_oof},    // terrain_solid
    {sfx_splsml, sfx_splash}, // terrain_water
    {sfx_plosml, sfx_ploosh}, // terrain_slime
    {sfx_lavsml, sfx_lvsiz}   // terrain_lava
  };

  S_StartSoundHitFloor(mo, hitsound[terrain][oof]);
}

///////////////////////////////////////////////////////////////
//
// Linedef and Sector Special Implementation Utility Functions
//
///////////////////////////////////////////////////////////////

//
// getSide()
//
// Will return a side_t*
//  given the number of the current sector,
//  the line number, and the side (0/1) that you want.
//
// Note: if side=1 is specified, it must exist or results undefined
//

side_t *getSide(int currentSector, int line, int side)
{
  return &sides[sectors[currentSector].lines[line]->sidenum[side]];
}

//
// getSector()
//
// Will return a sector_t*
//  given the number of the current sector,
//  the line number and the side (0/1) that you want.
//
// Note: if side=1 is specified, it must exist or results undefined
//

sector_t *getSector(int currentSector, int line, int side)
{
  return sides[sectors[currentSector].lines[line]->sidenum[side]].sector;
}

//
// twoSided()
//
// Given the sector number and the line number,
//  it will tell you whether the line is two-sided or not.
//
// modified to return actual two-sidedness rather than presence
// of 2S flag unless compatibility optioned
//
// killough 11/98: reformatted

int twoSided(int sector, int line)
{
  //jff 1/26/98 return what is actually needed, whether the line
  //has two sidedefs, rather than whether the 2S flag is set

  return comp[comp_model] ? sectors[sector].lines[line]->flags & ML_TWOSIDED :
    sectors[sector].lines[line]->sidenum[1] != NO_INDEX;
}

//
// getNextSector()
//
// Return sector_t * of sector next to current across line.
//
// Note: returns NULL if not two-sided line, or both sides refer to sector
//
// killough 11/98: reformatted

sector_t *getNextSector(line_t *line, sector_t *sec)
{
  //jff 1/26/98 check unneeded since line->backsector already
  //returns NULL if the line is not two sided, and does so from
  //the actual two-sidedness of the line, rather than its 2S flag
  //
  //jff 5/3/98 don't retn sec unless compatibility
  // fixes an intra-sector line breaking functions
  // like floor->highest floor

  return comp[comp_model] && !(line->flags & ML_TWOSIDED) ? NULL :
    line->frontsector == sec ? comp[comp_model] || line->backsector != sec ?
    line->backsector : NULL : line->frontsector;
}

//
// P_FindLowestFloorSurrounding()
//
// Returns the fixed point value of the lowest floor height
// in the sector passed or its surrounding sectors.
//
// killough 11/98: reformatted

fixed_t P_FindLowestFloorSurrounding(sector_t* sec)
{
  fixed_t floor = sec->floorheight;
  const sector_t *other;
  int i;

  for (i = 0; i < sec->linecount; i++)
    if ((other = getNextSector(sec->lines[i], sec)) &&
        other->floorheight < floor)
      floor = other->floorheight;

  return floor;
}

//
// P_FindHighestFloorSurrounding()
//
// Passed a sector, returns the fixed point value of the largest
// floor height in the surrounding sectors, not including that passed
//
// NOTE: if no surrounding sector exists -32000*FRACUINT is returned
//       if compatibility then -500*FRACUNIT is the smallest return possible
//
// killough 11/98: reformatted

fixed_t P_FindHighestFloorSurrounding(sector_t *sec)
{
  fixed_t floor = -500*FRACUNIT;
  const sector_t *other;
  int i;

  //jff 1/26/98 Fix initial value for floor to not act differently
  //in sections of wad that are below -500 units

  if (!comp[comp_model])          //jff 3/12/98 avoid ovf
    floor = -32000*FRACUNIT;      // in height calculations

  for (i=0 ;i < sec->linecount ; i++)
    if ((other = getNextSector(sec->lines[i],sec)) &&
        other->floorheight > floor)
      floor = other->floorheight;

  return floor;
}

//
// P_FindNextHighestFloor()
//
// Passed a sector and a floor height, returns the fixed point value
// of the smallest floor height in a surrounding sector larger than
// the floor height passed. If no such height exists the floorheight
// passed is returned.
//
// Rewritten by Lee Killough to avoid fixed array and to be faster
//

fixed_t P_FindNextHighestFloor(sector_t *sec, int currentheight)
{
  sector_t *other;
  int i;

  for (i=0 ;i < sec->linecount ; i++)
    if ((other = getNextSector(sec->lines[i],sec)) &&
        other->floorheight > currentheight)
      {
        int height = other->floorheight;
        while (++i < sec->linecount)
          if ((other = getNextSector(sec->lines[i],sec)) &&
              other->floorheight < height &&
              other->floorheight > currentheight)
            height = other->floorheight;
        return height;
      }
  return currentheight;
}

//
// P_FindNextLowestFloor()
//
// Passed a sector and a floor height, returns the fixed point value
// of the largest floor height in a surrounding sector smaller than
// the floor height passed. If no such height exists the floorheight
// passed is returned.
//
// jff 02/03/98 Twiddled Lee's P_FindNextHighestFloor to make this

fixed_t P_FindNextLowestFloor(sector_t *sec, int currentheight)
{
  sector_t *other;
  int i;

  for (i=0 ;i < sec->linecount ; i++)
    if ((other = getNextSector(sec->lines[i],sec)) &&
        other->floorheight < currentheight)
      {
        int height = other->floorheight;
        while (++i < sec->linecount)
          if ((other = getNextSector(sec->lines[i],sec)) &&
              other->floorheight > height &&
              other->floorheight < currentheight)
            height = other->floorheight;
        return height;
      }
  return currentheight;
}

//
// P_FindNextLowestCeiling()
//
// Passed a sector and a ceiling height, returns the fixed point value
// of the largest ceiling height in a surrounding sector smaller than
// the ceiling height passed. If no such height exists the ceiling height
// passed is returned.
//
// jff 02/03/98 Twiddled Lee's P_FindNextHighestFloor to make this

fixed_t P_FindNextLowestCeiling(sector_t *sec, int currentheight)
{
  sector_t *other;
  int i;

  for (i=0 ;i < sec->linecount ; i++)
    if ((other = getNextSector(sec->lines[i],sec)) &&
        other->ceilingheight < currentheight)
      {
        int height = other->ceilingheight;
        while (++i < sec->linecount)
          if ((other = getNextSector(sec->lines[i],sec)) &&
              other->ceilingheight > height &&
              other->ceilingheight < currentheight)
            height = other->ceilingheight;
        return height;
      }
  return currentheight;
}

//
// P_FindNextHighestCeiling()
//
// Passed a sector and a ceiling height, returns the fixed point value
// of the smallest ceiling height in a surrounding sector larger than
// the ceiling height passed. If no such height exists the ceiling height
// passed is returned.
//
// jff 02/03/98 Twiddled Lee's P_FindNextHighestFloor to make this

fixed_t P_FindNextHighestCeiling(sector_t *sec, int currentheight)
{
  sector_t *other;
  int i;

  for (i=0 ;i < sec->linecount ; i++)
    if ((other = getNextSector(sec->lines[i],sec)) &&
        other->ceilingheight > currentheight)
      {
        int height = other->ceilingheight;
        while (++i < sec->linecount)
          if ((other = getNextSector(sec->lines[i],sec)) &&
              other->ceilingheight < height &&
              other->ceilingheight > currentheight)
            height = other->ceilingheight;
        return height;
      }
  return currentheight;
}

//
// P_FindLowestCeilingSurrounding()
//
// Passed a sector, returns the fixed point value of the smallest
// ceiling height in the surrounding sectors, not including that passed
//
// NOTE: if no surrounding sector exists 32000*FRACUINT is returned
//       but if compatibility then MAXINT is the return
//
// killough 11/98: reformatted

fixed_t P_FindLowestCeilingSurrounding(sector_t* sec)
{
  const sector_t *other;
  fixed_t height = INT_MAX;
  int i;

  if (!comp[comp_model])
    height = 32000*FRACUNIT; //jff 3/12/98 avoid ovf in

  // height calculations
  for (i=0; i < sec->linecount; i++)
    if ((other = getNextSector(sec->lines[i],sec)) &&
        other->ceilingheight < height)
      height = other->ceilingheight;

  return height;
}

//
// P_FindHighestCeilingSurrounding()
//
// Passed a sector, returns the fixed point value of the largest
// ceiling height in the surrounding sectors, not including that passed
//
// NOTE: if no surrounding sector exists -32000*FRACUINT is returned
//       but if compatibility then 0 is the smallest return possible
//
// killough 11/98: reformatted

fixed_t P_FindHighestCeilingSurrounding(sector_t* sec)
{
  const sector_t *other;
  fixed_t height = 0;
  int i;

  //jff 1/26/98 Fix initial value for floor to not act differently
  //in sections of wad that are below 0 units

  if (!comp[comp_model])
    height = -32000*FRACUNIT; //jff 3/12/98 avoid ovf in

  // height calculations
  for (i=0 ;i < sec->linecount ; i++)
    if ((other = getNextSector(sec->lines[i],sec)) &&
        other->ceilingheight > height)
      height = other->ceilingheight;

  return height;
}

//
// P_FindShortestTextureAround()
//
// Passed a sector number, returns the shortest lower texture on a
// linedef bounding the sector.
//
// Note: If no lower texture exists 32000*FRACUNIT is returned.
//       but if compatibility then MAXINT is returned
//
// jff 02/03/98 Add routine to find shortest lower texture
//
// killough 11/98: reformatted

fixed_t P_FindShortestTextureAround(int secnum)
{
  const sector_t *sec = &sectors[secnum];
  int i, minsize = INT_MAX;
#ifdef MBF_STRICT
  static const int mintex = 0;
#else
  static const int mintex = 1; //jff 8/14/98 texture 0 is a placeholder
#endif

  if (!comp[comp_model])
    minsize = 32000<<FRACBITS; //jff 3/13/98 prevent overflow in height calcs

  for (i = 0; i < sec->linecount; i++)
    if (twoSided(secnum, i))
      {
        const side_t *side;
        if ((side = getSide(secnum,i,0))->bottomtexture >= mintex &&
            textureheight[side->bottomtexture] < minsize)
          minsize = textureheight[side->bottomtexture];
        if ((side = getSide(secnum,i,1))->bottomtexture >= mintex &&
            textureheight[side->bottomtexture] < minsize)
          minsize = textureheight[side->bottomtexture];
      }

  return minsize;
}

//
// P_FindShortestUpperAround()
//
// Passed a sector number, returns the shortest upper texture on a
// linedef bounding the sector.
//
// Note: If no upper texture exists 32000*FRACUNIT is returned.
//       but if compatibility then MAXINT is returned
//
// jff 03/20/98 Add routine to find shortest upper texture
//
// killough 11/98: reformatted

fixed_t P_FindShortestUpperAround(int secnum)
{
  const sector_t *sec = &sectors[secnum];
  int i, minsize = INT_MAX;
#ifdef MBF_STRICT
  static const int mintex = 0;
#else
  static const int mintex = 1; //jff 8/14/98 texture 0 is a placeholder
#endif

  if (!comp[comp_model])
    minsize = 32000<<FRACBITS; //jff 3/13/98 prevent overflow

  // in height calcs
  for (i = 0; i < sec->linecount; i++)
    if (twoSided(secnum, i))
      {
        const side_t *side;
        if ((side = getSide(secnum,i,0))->toptexture >= mintex)
          if (textureheight[side->toptexture] < minsize)
            minsize = textureheight[side->toptexture];
        if ((side = getSide(secnum,i,1))->toptexture >= mintex)
          if (textureheight[side->toptexture] < minsize)
            minsize = textureheight[side->toptexture];
      }

  return minsize;
}

//
// P_FindModelFloorSector()
//
// Passed a floor height and a sector number, return a pointer to a
// a sector with that floor height across the lowest numbered two sided
// line surrounding the sector.
//
// Note: If no sector at that height bounds the sector passed, return NULL
//
// jff 02/03/98 Add routine to find numeric model floor
//  around a sector specified by sector number
// jff 3/14/98 change first parameter to plain height to allow call
//  from routine not using floormove_t
//
// killough 11/98: reformatted

sector_t *P_FindModelFloorSector(fixed_t floordestheight, int secnum)
{
  sector_t *sec = &sectors[secnum]; //jff 3/2/98 woops! better do this

  //jff 5/23/98 don't disturb sec->linecount while searching
  // but allow early exit in old demos

  int i, linecount = sec->linecount;

  for (i = 0; i < (demo_compatibility && sec->linecount < linecount ?
                   sec->linecount : linecount); i++)
    if (twoSided(secnum, i) &&
        (sec = getSector(secnum, i,
                         getSide(secnum,i,0)->sector-sectors == secnum))->
        floorheight == floordestheight)
      return sec;

  return NULL;
}

//
// P_FindModelCeilingSector()
//
// Passed a ceiling height and a sector number, return a pointer to a
// a sector with that ceiling height across the lowest numbered two sided
// line surrounding the sector.
//
// Note: If no sector at that height bounds the sector passed, return NULL
//
// jff 02/03/98 Add routine to find numeric model ceiling
//  around a sector specified by sector number
//  used only from generalized ceiling types
// jff 3/14/98 change first parameter to plain height to allow call
//  from routine not using ceiling_t
//
// killough 11/98: reformatted

sector_t *P_FindModelCeilingSector(fixed_t ceildestheight, int secnum)
{
  sector_t *sec = &sectors[secnum]; //jff 3/2/98 woops! better do this

  //jff 5/23/98 don't disturb sec->linecount while searching
  // but allow early exit in old demos
  int i, linecount = sec->linecount;

  for (i = 0; i < (demo_compatibility && sec->linecount<linecount?
                   sec->linecount : linecount); i++)
    if (twoSided(secnum, i) &&
        (sec = getSector(secnum, i,
                         getSide(secnum,i,0)->sector-sectors == secnum))->
        ceilingheight == ceildestheight)
      return sec;

  return NULL;
}

//
// RETURN NEXT SECTOR # THAT LINE TAG REFERS TO
//

// Find the next sector with the same tag as a linedef.
// Rewritten by Lee Killough to use chained hashing to improve speed

int P_FindSectorFromLineTag(const line_t *line, int start)
{
  start = start >= 0 ? sectors[start].nexttag :
    sectors[(unsigned) line->tag % (unsigned) numsectors].firsttag;
  while (start >= 0 && sectors[start].tag != line->tag)
    start = sectors[start].nexttag;
  return start;
}

// killough 4/16/98: Same thing, only for linedefs

int P_FindLineFromLineTag(const line_t *line, int start)
{
  start = start >= 0 ? lines[start].nexttag :
    lines[(unsigned) line->tag % (unsigned) numlines].firsttag;
  while (start >= 0 && lines[start].tag != line->tag)
    start = lines[start].nexttag;
  return start;
}

// Hash the sector tags across the sectors and linedefs.
static void P_InitTagLists(void)
{
  register int i;

  for (i=numsectors; --i>=0; )        // Initially make all slots empty.
    sectors[i].firsttag = -1;

  for (i=numsectors; --i>=0; )        // Proceed from last to first sector
    {                                 // so that lower sectors appear first
      int j = (unsigned) sectors[i].tag % (unsigned) numsectors; // Hash func
      sectors[i].nexttag = sectors[j].firsttag;   // Prepend sector to chain
      sectors[j].firsttag = i;
    }

  // killough 4/17/98: same thing, only for linedefs

  for (i=numlines; --i>=0; )        // Initially make all slots empty.
    lines[i].firsttag = -1;

  for (i=numlines; --i>=0; )        // Proceed from last to first linedef
    {                               // so that lower linedefs appear first
      int j = (unsigned) lines[i].tag % (unsigned) numlines; // Hash func
      lines[i].nexttag = lines[j].firsttag;   // Prepend linedef to chain
      lines[j].firsttag = i;
    }
}

//
// P_FindMinSurroundingLight()
//
// Passed a sector and a light level, returns the smallest light level
// in a surrounding sector less than that passed. If no smaller light
// level exists, the light level passed is returned.
//
// killough 11/98: reformatted

int P_FindMinSurroundingLight(sector_t *sector, int min)
{
  const sector_t *check;
  int i;

  for (i=0; i < sector->linecount; i++)
    if ((check = getNextSector(sector->lines[i], sector)) &&
        check->lightlevel < min)
      min = check->lightlevel;

  return min;
}

//
// P_CanUnlockGenDoor()
//
// Passed a generalized locked door linedef and a player, returns whether
// the player has the keys necessary to unlock that door.
//
// Note: The linedef passed MUST be a generalized locked door type
//       or results are undefined.
//
// jff 02/05/98 routine added to test for unlockability of
//  generalized locked doors
//
// killough 11/98: reformatted

boolean P_CanUnlockGenDoor(line_t *line, player_t *player)
{
  // does this line special distinguish between skulls and keys?
  int skulliscard = (line->special & LockedNKeys)>>LockedNKeysShift;

  // determine for each case of lock type if player's keys are adequate
  switch((line->special & LockedKey)>>LockedKeyShift)
    {
    case AnyKey:
      if (!player->cards[it_redcard] &&
          !player->cards[it_redskull] &&
          !player->cards[it_bluecard] &&
          !player->cards[it_blueskull] &&
          !player->cards[it_yellowcard] &&
          !player->cards[it_yellowskull])
        {
          doomprintf(player, MESSAGES_NONE, "%s", s_PD_ANY); // Ty 03/27/98 - externalized
          S_StartSound(player->mo,sfx_oof);             // killough 3/20/98
          return false;
        }
      break;
    case RCard:
      if (!player->cards[it_redcard] &&
          (!skulliscard || !player->cards[it_redskull]))
        {
          doomprintf(player, MESSAGES_NONE, "%s", skulliscard? s_PD_REDK : s_PD_REDC); // Ty 03/27/98 - externalized
          S_StartSound(player->mo,sfx_oof);             // killough 3/20/98
          return false;
        }
      break;
    case BCard:
      if (!player->cards[it_bluecard] &&
          (!skulliscard || !player->cards[it_blueskull]))
        {
          doomprintf(player, MESSAGES_NONE, "%s", skulliscard? s_PD_BLUEK : s_PD_BLUEC); // Ty 03/27/98 - externalized
          S_StartSound(player->mo,sfx_oof);             // killough 3/20/98
          return false;
        }
      break;
    case YCard:
      if (!player->cards[it_yellowcard] &&
          (!skulliscard || !player->cards[it_yellowskull]))
        {
          doomprintf(player, MESSAGES_NONE, "%s", skulliscard? s_PD_YELLOWK : s_PD_YELLOWC); // Ty 03/27/98 - externalized
          S_StartSound(player->mo,sfx_oof);             // killough 3/20/98
          return false;
        }
      break;
    case RSkull:
      if (!player->cards[it_redskull] &&
          (!skulliscard || !player->cards[it_redcard]))
        {
          doomprintf(player, MESSAGES_NONE, "%s", skulliscard? s_PD_REDK : s_PD_REDS); // Ty 03/27/98 - externalized
          S_StartSound(player->mo,sfx_oof);             // killough 3/20/98
          return false;
        }
      break;
    case BSkull:
      if (!player->cards[it_blueskull] &&
          (!skulliscard || !player->cards[it_bluecard]))
        {
          doomprintf(player, MESSAGES_NONE, "%s", skulliscard? s_PD_BLUEK : s_PD_BLUES); // Ty 03/27/98 - externalized
          S_StartSound(player->mo,sfx_oof);             // killough 3/20/98
          return false;
        }
      break;
    case YSkull:
      if (!player->cards[it_yellowskull] &&
          (!skulliscard || !player->cards[it_yellowcard]))
        {
          doomprintf(player, MESSAGES_NONE, "%s", skulliscard? s_PD_YELLOWK : s_PD_YELLOWS); // Ty 03/27/98 - externalized
          S_StartSound(player->mo,sfx_oof);             // killough 3/20/98
          return false;
        }
      break;
    case AllKeys:
      if (!skulliscard &&
          (!player->cards[it_redcard] ||
           !player->cards[it_redskull] ||
           !player->cards[it_bluecard] ||
           !player->cards[it_blueskull] ||
           !player->cards[it_yellowcard] ||
           !player->cards[it_yellowskull]))
        {
          doomprintf(player, MESSAGES_NONE, "%s", s_PD_ALL6); // Ty 03/27/98 - externalized
          S_StartSound(player->mo,sfx_oof);             // killough 3/20/98
          return false;
        }
      if (skulliscard &&
          (!(player->cards[it_redcard] | player->cards[it_redskull]) ||
           !(player->cards[it_bluecard] | player->cards[it_blueskull]) ||
           // [FG] 3-key door works with only 2 keys
           // http://prboom.sourceforge.net/mbf-bugs.html
           !(player->cards[it_yellowcard] | (demo_version == DV_MBF ? !player->cards[it_yellowskull] : player->cards[it_yellowskull]))))
        {
          doomprintf(player, MESSAGES_NONE, "%s", s_PD_ALL3); // Ty 03/27/98 - externalized
          S_StartSound(player->mo,sfx_oof);             // killough 3/20/98
          return false;
        }
      break;
    }
  return true;
}

//
// P_SectorActive()
//
// Passed a linedef special class (floor, ceiling, lighting) and a sector
// returns whether the sector is already busy with a linedef special of the
// same class. If old demo compatibility true, all linedef special classes
// are the same.
//
// jff 2/23/98 added to prevent old demos from
//  succeeding in starting multiple specials on one sector
//
// killough 11/98: reformatted

int P_SectorActive(special_e t,sector_t *sec)
{
  return demo_compatibility ?  // return whether any thinker is active
    sec->floordata || sec->ceilingdata || sec->lightingdata :
    t == floor_special ? !!sec->floordata :        // return whether
    t == ceiling_special ? !!sec->ceilingdata :    // thinker of same
    t == lighting_special ? !!sec->lightingdata :  // type is active
    1; // don't know which special, must be active, shouldn't be here
}

//
// P_CheckTag()
//
// Passed a line, returns true if the tag is non-zero or the line special
// allows no tag without harm. If compatibility, all linedef specials are
// allowed to have zero tag.
//
// Note: Only line specials activated by walkover, pushing, or shooting are
//       checked by this routine.
//
// jff 2/27/98 Added to check for zero tag allowed for regular special types

int P_CheckTag(line_t *line)
{
  // killough 11/98: compatibility option:

  if (comp[comp_zerotags] || line->tag)
    return 1;

  switch (line->special)
    {
    case 1:   // Manual door specials
    case 26:
    case 27:
    case 28:
    case 31:
    case 32:
    case 33:
    case 34:
    case 117:
    case 118:
    case 139:  // Lighting specials
    case 170:
    case 79:
    case 35:
    case 138:
    case 171:
    case 81:
    case 13:
    case 192:
    case 169:
    case 80:
    case 12:
    case 194:
    case 173:
    case 157:
    case 104:
    case 193:
    case 172:
    case 156:
    case 17:
    case 195:  // Thing teleporters
    case 174:
    case 97:
    case 39:
    case 126:
    case 125:
    case 210:
    case 209:
    case 208:
    case 207:
    case 11:  // Exits
    case 52:
    case 197:
    case 51:
    case 124:
    case 198:
    case 48:  // Scrolling walls
    case 85:
    case 2069: // Inventory-reset Exits
    case 2070:
    case 2071:
    case 2072:
    case 2073:
    case 2074:
    case 2082: // Two-sided scrolling walls
    case 2083:
    case 2057: // Music changers
    case 2058:
    case 2059:
    case 2060:
    case 2061:
    case 2062:
    case 2063:
    case 2064:
    case 2065:
    case 2066:
    case 2067:
    case 2068:
    case 2087:
    case 2088:
    case 2089:
    case 2090:
    case 2091:
    case 2092:
    case 2093:
    case 2094:
    case 2095:
    case 2096:
    case 2097:
    case 2098:
      return 1;
    }

  return 0;
}

boolean P_IsDeathExit(sector_t *sector)
{
  if (sector->special < 32)
  {
    return (sector->special == 11);
  }
  else if (mbf21 && sector->special & DEATH_MASK)
  {
    const int i = (sector->special & DAMAGE_MASK) >> DAMAGE_SHIFT;

    return (i == 2 || i == 3);
  }

  return false;
}

//
// P_IsSecret()
//
// Passed a sector, returns if the sector secret type is still active, i.e.
// secret type is set and the secret has not yet been obtained.
//
// jff 3/14/98 added to simplify checks for whether sector is secret
//  in automap and other places
//

boolean P_IsSecret(sector_t *sec)
{
  return sec->special == 9 || sec->special & SECRET_MASK;
}

//
// P_WasSecret()
//
// Passed a sector, returns if the sector secret type is was active, i.e.
// secret type was set and the secret has been obtained already.
//
// jff 3/14/98 added to simplify checks for whether sector is secret
//  in automap and other places
//

boolean P_WasSecret(sector_t *sec)
{
  return sec->oldspecial == 9 || sec->oldspecial & SECRET_MASK;
}

//
// EV_ChangeMusic() -- ID24 Music Changers
//
// Generic solution for changing the currently playing music during play time.
// There are four type of music changing behavior, all of them available in all
// six major activation triggers (W1, WR, S1, SR, G1, GR) totalling 24 lines.
// All specials can be triggered from either side of the line being activated.
// Of the four categories, there are two conditions:
//
//  1. If the given music lump will loop or not
//  2. If it will reset to the map's default when no music lump is defined
//
// Giving the four resulting categories:
// * Change music and make it loop only if a track is defined.
// * Change music and make it play only once and stop all music after.
// * Change music and make it loop, reset to looping default if no track
//    defined.
// * Change music and make it play only once, reset to looping default if no
//    track defined.
//

void EV_ChangeMusic(line_t *line, int side)
{
  boolean once = false;
  boolean loops = false;
  boolean resets = false;

  int music = side ? line->backmusic : line->frontmusic;

  switch (line->special)
  {
    case 2057: case 2059: case 2061: case 2063:
    case 2065: case 2067: case 2087: case 2089:
    case 2091: case 2093: case 2095: case 2097:
      once = true;
      break;
  }

  switch (line->special)
  {
    case 2057: case 2058: case 2059: case 2060:
    case 2061: case 2062: case 2087: case 2088:
    case 2089: case 2090: case 2091: case 2092:
      loops = true;
      break;
  }

  switch (line->special)
  {
    case 2087: case 2088: case 2089: case 2090:
    case 2091: case 2092: case 2093: case 2094:
    case 2095: case 2096: case 2097: case 2098:
      resets = true;
      break;
  }

  if (music)
    S_ChangeMusInfoMusic(music, loops);
  else if (resets)
    S_ChangeMusInfoMusic(musinfo.items[0], true); // Always loops when defaulting

  if (once)
    line->special = 0;
}

//////////////////////////////////////////////////////////////////////////
//
// Events
//
// Events are operations triggered by using, crossing,
// or shooting special lines, or by timed thinkers.
//
/////////////////////////////////////////////////////////////////////////

//
// P_CrossSpecialLine - Walkover Trigger Dispatcher
//
// Called every time a thing origin is about
//  to cross a line with a non 0 special, whether a walkover type or not.
//
// jff 02/12/98 all W1 lines were fixed to check the result from the EV_
//  function before clearing the special. This avoids losing the function
//  of the line, should the sector already be active when the line is
//  crossed. Change is qualified by demo_compatibility.
//
// killough 11/98: change linenum parameter to a line_t pointer

void P_CrossSpecialLine(line_t *line, int side, mobj_t *thing, boolean bossaction)
{
  int ok;

  //  Things that should never trigger lines
  if (!thing->player && !bossaction)
    switch(thing->type)    // Things that should NOT trigger specials...
      {
      case MT_ROCKET:
      case MT_PLASMA:
      case MT_BFG:
      case MT_TROOPSHOT:
      case MT_HEADSHOT:
      case MT_BRUISERSHOT:
      case MT_PLASMA1:    // killough 8/28/98: exclude beta fireballs
      case MT_PLASMA2:
        return;
      default:
        break;
      }

  //jff 02/04/98 add check here for generalized lindef types
  if (!demo_compatibility) // generalized types not recognized if old demo
    {
      // pointer to line function is NULL by default, set non-null if
      // line special is walkover generalized linedef type
      int (*linefunc)(line_t *)=NULL;

      // check each range of generalized linedefs
      if ((unsigned)line->special >= GenFloorBase)
        {
          if (!thing->player && !bossaction)
            if ((line->special & FloorChange) || !(line->special & FloorModel))
              return;     // FloorModel is "Allow Monsters" if FloorChange is 0
          if (!line->tag) //jff 2/27/98 all walk generalized types require tag
            return;
          linefunc = EV_DoGenFloor;
        }
      else
        if ((unsigned)line->special >= GenCeilingBase)
          {
            if (!thing->player && !bossaction)
              if ((line->special & CeilingChange) || !(line->special & CeilingModel))
                return;     // CeilingModel is "Allow Monsters" if CeilingChange is 0
            if (!line->tag) //jff 2/27/98 all walk generalized types require tag
              return;
            linefunc = EV_DoGenCeiling;
          }
        else
          if ((unsigned)line->special >= GenDoorBase)
            {
              if (!thing->player && !bossaction)
                {
                  if (!(line->special & DoorMonster))
                    return;                    // monsters disallowed from this door
                  if (line->flags & ML_SECRET) // they can't open secret doors either
                    return;
                }
              if (!line->tag) //3/2/98 move outside the monster check
                return;
              linefunc = EV_DoGenDoor;
            }
          else
            if ((unsigned)line->special >= GenLockedBase)
              {
                if (!thing->player || bossaction)
                  return;                     // monsters disallowed from unlocking doors
                if (((line->special&TriggerType)==WalkOnce) || ((line->special&TriggerType)==WalkMany))
                  { //jff 4/1/98 check for being a walk type before reporting door type
                    if (!P_CanUnlockGenDoor(line,thing->player))
                      return;
                  }
                else
                  return;
                linefunc = EV_DoGenLockedDoor;
              }
            else
              if ((unsigned)line->special >= GenLiftBase)
                {
                  if (!thing->player && !bossaction)
                    if (!(line->special & LiftMonster))
                      return; // monsters disallowed
                  if (!line->tag) //jff 2/27/98 all walk generalized types require tag
                    return;
                  linefunc = EV_DoGenLift;
                }
              else
                if ((unsigned)line->special >= GenStairsBase)
                  {
                    if (!thing->player && !bossaction)
                      if (!(line->special & StairMonster))
                        return; // monsters disallowed
                    if (!line->tag) //jff 2/27/98 all walk generalized types require tag
                      return;
                    linefunc = EV_DoGenStairs;
                  }
              else
                if (mbf21 && (unsigned)line->special >= GenCrusherBase)
                  {
                    // haleyjd 06/09/09: This was completely forgotten in BOOM, disabling
                    // all generalized walk-over crusher types!
                    if (!thing->player && !bossaction)
                      if (!(line->special & StairMonster))
                        return; // monsters disallowed
                    if (!line->tag) //jff 2/27/98 all walk generalized types require tag
                      return;
                    linefunc = EV_DoGenCrusher;
                  }

      if (linefunc) // if it was a valid generalized type
        switch((line->special & TriggerType) >> TriggerTypeShift)
          {
          case WalkOnce:
            if (linefunc(line))
              line->special = 0;    // clear special if a walk once type
            return;
          case WalkMany:
            linefunc(line);
            return;
          default:                  // if not a walk type, do nothing here
            return;
          }
    }

  if (!thing->player || bossaction)
    {
      ok = bossaction;
      switch(line->special)
        {
        case 39:      // teleport trigger
        case 97:      // teleport retrigger
        case 125:     // teleport monsteronly trigger
        case 126:     // teleport monsteronly retrigger
          //jff 3/5/98 add ability of monsters etc. to use teleporters
        case 208:     //silent thing teleporters
        case 207:
        case 243:     //silent line-line teleporter
        case 244:     //jff 3/6/98 make fit within DCK's 256 linedef types
        case 262:     //jff 4/14/98 add monster only
        case 263:     //jff 4/14/98 silent thing,line,line rev types
        case 264:     //jff 4/14/98 plus player/monster silent line
        case 265:     //            reversed types
        case 266:
        case 267:
        case 268:
        case 269:
          if (bossaction) return;
        case 4:       // raise door
        case 10:      // plat down-wait-up-stay trigger
        case 88:      // plat down-wait-up-stay retrigger
          ok = 1;
          break;
        }
      if (!ok)
        return;
    }

  if (!P_CheckTag(line))  //jff 2/27/98 disallow zero tag on some types
    return;

  // Dispatch on the line special value to the line's action routine
  // If a once only function, and successful, clear the line special

  switch (line->special)
    {
      // Regular walk once triggers

    case 2:
      // Open Door
      if (EV_DoDoor(line,doorOpen) || demo_compatibility)
        line->special = 0;
      break;

    case 3:
      // Close Door
      if (EV_DoDoor(line,doorClose) || demo_compatibility)
        line->special = 0;
      break;

    case 4:
      // Raise Door
      if (EV_DoDoor(line,doorNormal) || demo_compatibility)
        line->special = 0;
      break;

    case 5:
      // Raise Floor
      if (EV_DoFloor(line,raiseFloor) || demo_compatibility)
        line->special = 0;
      break;

    case 6:
      // Fast Ceiling Crush & Raise
      if (EV_DoCeiling(line,fastCrushAndRaise) || demo_compatibility)
        line->special = 0;
      break;

    case 8:
      // Build Stairs
      if (EV_BuildStairs(line,build8) || demo_compatibility)
        line->special = 0;
      break;

    case 10:
      // PlatDownWaitUp
      if (EV_DoPlat(line,downWaitUpStay,0) || demo_compatibility)
        line->special = 0;
      break;

    case 12:
      // Light Turn On - brightest near
      if (EV_LightTurnOn(line,0) || demo_compatibility)
        line->special = 0;
      break;

    case 13:
      // Light Turn On 255
      if (EV_LightTurnOn(line,255) || demo_compatibility)
        line->special = 0;
      break;

    case 16:
      // Close Door 30
      if (EV_DoDoor(line,close30ThenOpen) || demo_compatibility)
        line->special = 0;
      break;

    case 17:
      // Start Light Strobing
      if (EV_StartLightStrobing(line) || demo_compatibility)
        line->special = 0;
      break;

    case 19:
      // Lower Floor
      if (EV_DoFloor(line,lowerFloor) || demo_compatibility)
        line->special = 0;
      break;

    case 22:
      // Raise floor to nearest height and change texture
      if (EV_DoPlat(line,raiseToNearestAndChange,0) || demo_compatibility)
        line->special = 0;
      break;

    case 25:
      // Ceiling Crush and Raise
      if (EV_DoCeiling(line,crushAndRaise) || demo_compatibility)
        line->special = 0;
      break;

    case 30:
      // Raise floor to shortest texture height
      //  on either side of lines.
      if (EV_DoFloor(line,raiseToTexture) || demo_compatibility)
        line->special = 0;
      break;

    case 35:
      // Lights Very Dark
      if (EV_LightTurnOn(line,35) || demo_compatibility)
        line->special = 0;
      break;

    case 36:
      // Lower Floor (TURBO)
      if (EV_DoFloor(line,turboLower) || demo_compatibility)
        line->special = 0;
      break;

    case 37:
      // LowerAndChange
      if (EV_DoFloor(line,lowerAndChange) || demo_compatibility)
        line->special = 0;
      break;

    case 38:
      // Lower Floor To Lowest
      if (EV_DoFloor(line, lowerFloorToLowest) || demo_compatibility)
        line->special = 0;
      break;

    case 39:
      // TELEPORT! //jff 02/09/98 fix using up with wrong side crossing
      if (EV_Teleport(line, side, thing) || demo_compatibility)
        line->special = 0;
      break;

    case 40:
      // RaiseCeilingLowerFloor
      if (demo_compatibility)
        {
          EV_DoCeiling( line, raiseToHighest );
          EV_DoFloor( line, lowerFloorToLowest ); //jff 02/12/98 doesn't work
          line->special = 0;
        }
      else
        if (EV_DoCeiling(line, raiseToHighest))
          line->special = 0;
      break;

    case 44:
      // Ceiling Crush
      if (EV_DoCeiling(line, lowerAndCrush) || demo_compatibility)
        line->special = 0;
      break;

    // W1 - Exit to the next map and reset inventory.
    case 2069:
      if (demo_version < DV_ID24)
        break;
      reset_inventory = true;
      // fallthrough

    case 52:
      // EXIT!

      // killough 10/98: prevent zombies from exiting levels
      if (bossaction || (!(thing->player && thing->player->health <= 0 && !comp[comp_zombie])))
        G_ExitLevel ();
      break;

    case 53:
      // Perpetual Platform Raise
      if (EV_DoPlat(line,perpetualRaise,0) || demo_compatibility)
        line->special = 0;
      break;

    case 54:
      // Platform Stop
      if (EV_StopPlat(line) || demo_compatibility)
        line->special = 0;
      break;

    case 56:
      // Raise Floor Crush
      if (EV_DoFloor(line,raiseFloorCrush) || demo_compatibility)
        line->special = 0;
      break;

    case 57:
      // Ceiling Crush Stop
      if (EV_CeilingCrushStop(line) || demo_compatibility)
        line->special = 0;
      break;

    case 58:
      // Raise Floor 24
      if (EV_DoFloor(line,raiseFloor24) || demo_compatibility)
        line->special = 0;
      break;

    case 59:
      // Raise Floor 24 And Change
      if (EV_DoFloor(line,raiseFloor24AndChange) || demo_compatibility)
        line->special = 0;
      break;

    case 100:
      // Build Stairs Turbo 16
      if (EV_BuildStairs(line,turbo16) || demo_compatibility)
        line->special = 0;
      break;

    case 104:
      // Turn lights off in sector(tag)
      if (EV_TurnTagLightsOff(line) || demo_compatibility)
        line->special = 0;
      break;

    case 108:
      // Blazing Door Raise (faster than TURBO!)
      if (EV_DoDoor(line,blazeRaise) || demo_compatibility)
        line->special = 0;
      break;

    case 109:
      // Blazing Door Open (faster than TURBO!)
      if (EV_DoDoor (line,blazeOpen) || demo_compatibility)
        line->special = 0;
      break;

    case 110:
      // Blazing Door Close (faster than TURBO!)
      if (EV_DoDoor (line,blazeClose) || demo_compatibility)
        line->special = 0;
      break;

    case 119:
      // Raise floor to nearest surr. floor
      if (EV_DoFloor(line,raiseFloorToNearest) || demo_compatibility)
        line->special = 0;
      break;

    case 121:
      // Blazing PlatDownWaitUpStay
      if (EV_DoPlat(line,blazeDWUS,0) || demo_compatibility)
        line->special = 0;
      break;

    // W1 - Exit to the secret map and reset inventory.
    case 2072:
      if (demo_version < DV_ID24)
        break;
      reset_inventory = true;
      // fallthrough

    case 124:
      // Secret EXIT

      // killough 10/98: prevent zombies from exiting levels
      if (bossaction || (!(thing->player && thing->player->health <= 0 && !comp[comp_zombie])))
        G_SecretExitLevel ();
      break;

    case 125:
      // TELEPORT MonsterONLY
      if (!thing->player &&
          (EV_Teleport(line, side, thing) || demo_compatibility))
        line->special = 0;
      break;

    case 130:
      // Raise Floor Turbo
      if (EV_DoFloor(line,raiseFloorTurbo) || demo_compatibility)
        line->special = 0;
      break;

    case 141:
      // Silent Ceiling Crush & Raise
      if (EV_DoCeiling(line,silentCrushAndRaise) || demo_compatibility)
        line->special = 0;
      break;

      // Regular walk many retriggerable

    case 72:
      // Ceiling Crush
      EV_DoCeiling( line, lowerAndCrush );
      break;

    case 73:
      // Ceiling Crush and Raise
      EV_DoCeiling(line,crushAndRaise);
      break;

    case 74:
      // Ceiling Crush Stop
      EV_CeilingCrushStop(line);
      break;

    case 75:
      // Close Door
      EV_DoDoor(line,doorClose);
      break;

    case 76:
      // Close Door 30
      EV_DoDoor(line,close30ThenOpen);
      break;

    case 77:
      // Fast Ceiling Crush & Raise
      EV_DoCeiling(line,fastCrushAndRaise);
      break;

    case 79:
      // Lights Very Dark
      EV_LightTurnOn(line,35);
      break;

    case 80:
      // Light Turn On - brightest near
      EV_LightTurnOn(line,0);
      break;

    case 81:
      // Light Turn On 255
      EV_LightTurnOn(line,255);
      break;

    case 82:
      // Lower Floor To Lowest
      EV_DoFloor( line, lowerFloorToLowest );
      break;

    case 83:
      // Lower Floor
      EV_DoFloor(line,lowerFloor);
      break;

    case 84:
      // LowerAndChange
      EV_DoFloor(line,lowerAndChange);
      break;

    case 86:
      // Open Door
      EV_DoDoor(line,doorOpen);
      break;

    case 87:
      // Perpetual Platform Raise
      EV_DoPlat(line,perpetualRaise,0);
      break;

    case 88:
      // PlatDownWaitUp
      EV_DoPlat(line,downWaitUpStay,0);
      break;

    case 89:
      // Platform Stop
      EV_StopPlat(line);
      break;

    case 90:
      // Raise Door
      EV_DoDoor(line,doorNormal);
      break;

    case 91:
      // Raise Floor
      EV_DoFloor(line,raiseFloor);
      break;

    case 92:
      // Raise Floor 24
      EV_DoFloor(line,raiseFloor24);
      break;

    case 93:
      // Raise Floor 24 And Change
      EV_DoFloor(line,raiseFloor24AndChange);
      break;

    case 94:
      // Raise Floor Crush
      EV_DoFloor(line,raiseFloorCrush);
      break;

    case 95:
      // Raise floor to nearest height
      // and change texture.
      EV_DoPlat(line,raiseToNearestAndChange,0);
      break;

    case 96:
      // Raise floor to shortest texture height
      // on either side of lines.
      EV_DoFloor(line,raiseToTexture);
      break;

    case 97:
      // TELEPORT!
      EV_Teleport( line, side, thing );
      break;

    case 98:
      // Lower Floor (TURBO)
      EV_DoFloor(line,turboLower);
      break;

    case 105:
      // Blazing Door Raise (faster than TURBO!)
      EV_DoDoor (line,blazeRaise);
      break;

    case 106:
      // Blazing Door Open (faster than TURBO!)
      EV_DoDoor (line,blazeOpen);
      break;

    case 107:
      // Blazing Door Close (faster than TURBO!)
      EV_DoDoor (line,blazeClose);
      break;

    case 120:
      // Blazing PlatDownWaitUpStay.
      EV_DoPlat(line,blazeDWUS,0);
      break;

    case 126:
      // TELEPORT MonsterONLY.
      if (!thing->player)
        EV_Teleport( line, side, thing );
      break;

    case 128:
      // Raise To Nearest Floor
      EV_DoFloor(line,raiseFloorToNearest);
      break;

    case 129:
      // Raise Floor Turbo
      EV_DoFloor(line,raiseFloorTurbo);
      break;

    // ID24 Music Changers
    case 2057: case 2063: case 2087: case 2093:
    case 2058: case 2064: case 2088: case 2094:
      EV_ChangeMusic(line, side);
      break;


    case 2076:
      line->special = 0;
      // fallthrough

    case 2077:
    {
      int colormap_index = side ? line->backtint : line->fronttint;
      for (int s = -1; (s = P_FindSectorFromLineTag(line, s)) >= 0;)
      {
        sectors[s].tint = colormap_index;
      }
      break;
    }

      // Extended walk triggers

      // jff 1/29/98 added new linedef types to fill all functions out so that
      // all have varieties SR, S1, WR, W1

      // killough 1/31/98: "factor out" compatibility test, by
      // adding inner switch qualified by compatibility flag.
      // relax test to demo_compatibility

      // killough 2/16/98: Fix problems with W1 types being cleared too early

    default:
      if (!demo_compatibility)
        switch (line->special)
          {
            // Extended walk once triggers

          case 142:
            // Raise Floor 512
            // 142 W1  EV_DoFloor(raiseFloor512)
            if (EV_DoFloor(line,raiseFloor512))
              line->special = 0;
            break;

          case 143:
            // Raise Floor 24 and change
            // 143 W1  EV_DoPlat(raiseAndChange,24)
            if (EV_DoPlat(line,raiseAndChange,24))
              line->special = 0;
            break;

          case 144:
            // Raise Floor 32 and change
            // 144 W1  EV_DoPlat(raiseAndChange,32)
            if (EV_DoPlat(line,raiseAndChange,32))
              line->special = 0;
            break;

          case 145:
            // Lower Ceiling to Floor
            // 145 W1  EV_DoCeiling(lowerToFloor)
            if (EV_DoCeiling( line, lowerToFloor ))
              line->special = 0;
            break;

          case 146:
            // Lower Pillar, Raise Donut
            // 146 W1  EV_DoDonut()
            if (EV_DoDonut(line))
              line->special = 0;
            break;

          case 199:
            // Lower ceiling to lowest surrounding ceiling
            // 199 W1 EV_DoCeiling(lowerToLowest)
            if (EV_DoCeiling(line,lowerToLowest))
              line->special = 0;
            break;

          case 200:
            // Lower ceiling to highest surrounding floor
            // 200 W1 EV_DoCeiling(lowerToMaxFloor)
            if (EV_DoCeiling(line,lowerToMaxFloor))
              line->special = 0;
            break;

          case 207:
            // killough 2/16/98: W1 silent teleporter (normal kind)
            if (EV_SilentTeleport(line, side, thing))
              line->special = 0;
            break;

            //jff 3/16/98 renumber 215->153
          case 153: //jff 3/15/98 create texture change no motion type
            // Texture/Type Change Only (Trig)
            // 153 W1 Change Texture/Type Only
            if (EV_DoChange(line,trigChangeOnly))
              line->special = 0;
            break;

          case 239: //jff 3/15/98 create texture change no motion type
            // Texture/Type Change Only (Numeric)
            // 239 W1 Change Texture/Type Only
            if (EV_DoChange(line,numChangeOnly))
              line->special = 0;
            break;

          case 219:
            // Lower floor to next lower neighbor
            // 219 W1 Lower Floor Next Lower Neighbor
            if (EV_DoFloor(line,lowerFloorToNearest))
              line->special = 0;
            break;

          case 227:
            // Raise elevator next floor
            // 227 W1 Raise Elevator next floor
            if (EV_DoElevator(line,elevateUp))
              line->special = 0;
            break;

          case 231:
            // Lower elevator next floor
            // 231 W1 Lower Elevator next floor
            if (EV_DoElevator(line,elevateDown))
              line->special = 0;
            break;

          case 235:
            // Elevator to current floor
            // 235 W1 Elevator to current floor
            if (EV_DoElevator(line,elevateCurrent))
              line->special = 0;
            break;

          case 243: //jff 3/6/98 make fit within DCK's 256 linedef types
            // killough 2/16/98: W1 silent teleporter (linedef-linedef kind)
            if (EV_SilentLineTeleport(line, side, thing, false))
              line->special = 0;
            break;

          case 262: //jff 4/14/98 add silent line-line reversed
            if (EV_SilentLineTeleport(line, side, thing, true))
              line->special = 0;
            break;

          case 264: //jff 4/14/98 add monster-only silent line-line reversed
            if (!thing->player &&
                EV_SilentLineTeleport(line, side, thing, true))
              line->special = 0;
            break;

          case 266: //jff 4/14/98 add monster-only silent line-line
            if (!thing->player &&
                EV_SilentLineTeleport(line, side, thing, false))
              line->special = 0;
            break;

          case 268: //jff 4/14/98 add monster-only silent
            if (!thing->player && EV_SilentTeleport(line, side, thing))
              line->special = 0;
            break;

            //jff 1/29/98 end of added W1 linedef types

            // Extended walk many retriggerable

            //jff 1/29/98 added new linedef types to fill all functions
            //out so that all have varieties SR, S1, WR, W1

          case 147:
            // Raise Floor 512
            // 147 WR  EV_DoFloor(raiseFloor512)
            EV_DoFloor(line,raiseFloor512);
            break;

          case 148:
            // Raise Floor 24 and Change
            // 148 WR  EV_DoPlat(raiseAndChange,24)
            EV_DoPlat(line,raiseAndChange,24);
            break;

          case 149:
            // Raise Floor 32 and Change
            // 149 WR  EV_DoPlat(raiseAndChange,32)
            EV_DoPlat(line,raiseAndChange,32);
            break;

          case 150:
            // Start slow silent crusher
            // 150 WR  EV_DoCeiling(silentCrushAndRaise)
            EV_DoCeiling(line,silentCrushAndRaise);
            break;

          case 151:
            // RaiseCeilingLowerFloor
            // 151 WR  EV_DoCeiling(raiseToHighest),
            //         EV_DoFloor(lowerFloortoLowest)
            EV_DoCeiling( line, raiseToHighest );
            EV_DoFloor( line, lowerFloorToLowest );
            break;

          case 152:
            // Lower Ceiling to Floor
            // 152 WR  EV_DoCeiling(lowerToFloor)
            EV_DoCeiling( line, lowerToFloor );
            break;

            //jff 3/16/98 renumber 153->256
          case 256:
            // Build stairs, step 8
            // 256 WR EV_BuildStairs(build8)
            EV_BuildStairs(line,build8);
            break;

            //jff 3/16/98 renumber 154->257
          case 257:
            // Build stairs, step 16
            // 257 WR EV_BuildStairs(turbo16)
            EV_BuildStairs(line,turbo16);
            break;

          case 155:
            // Lower Pillar, Raise Donut
            // 155 WR  EV_DoDonut()
            EV_DoDonut(line);
            break;

          case 156:
            // Start lights strobing
            // 156 WR Lights EV_StartLightStrobing()
            EV_StartLightStrobing(line);
            break;

          case 157:
            // Lights to dimmest near
            // 157 WR Lights EV_TurnTagLightsOff()
            EV_TurnTagLightsOff(line);
            break;

          case 201:
            // Lower ceiling to lowest surrounding ceiling
            // 201 WR EV_DoCeiling(lowerToLowest)
            EV_DoCeiling(line,lowerToLowest);
            break;

          case 202:
            // Lower ceiling to highest surrounding floor
            // 202 WR EV_DoCeiling(lowerToMaxFloor)
            EV_DoCeiling(line,lowerToMaxFloor);
            break;

          case 208:
            // killough 2/16/98: WR silent teleporter (normal kind)
            EV_SilentTeleport(line, side, thing);
            break;

          case 212: //jff 3/14/98 create instant toggle floor type
            // Toggle floor between C and F instantly
            // 212 WR Instant Toggle Floor
            EV_DoPlat(line,toggleUpDn,0);
            break;

            //jff 3/16/98 renumber 216->154
          case 154: //jff 3/15/98 create texture change no motion type
            // Texture/Type Change Only (Trigger)
            // 154 WR Change Texture/Type Only
            EV_DoChange(line,trigChangeOnly);
            break;

          case 240: //jff 3/15/98 create texture change no motion type
            // Texture/Type Change Only (Numeric)
            // 240 WR Change Texture/Type Only
            EV_DoChange(line,numChangeOnly);
            break;

          case 220:
            // Lower floor to next lower neighbor
            // 220 WR Lower Floor Next Lower Neighbor
            EV_DoFloor(line,lowerFloorToNearest);
            break;

          case 228:
            // Raise elevator next floor
            // 228 WR Raise Elevator next floor
            EV_DoElevator(line,elevateUp);
            break;

          case 232:
            // Lower elevator next floor
            // 232 WR Lower Elevator next floor
            EV_DoElevator(line,elevateDown);
            break;

          case 236:
            // Elevator to current floor
            // 236 WR Elevator to current floor
            EV_DoElevator(line,elevateCurrent);
            break;

          case 244: //jff 3/6/98 make fit within DCK's 256 linedef types
            // killough 2/16/98: WR silent teleporter (linedef-linedef kind)
            EV_SilentLineTeleport(line, side, thing, false);
            break;

          case 263: //jff 4/14/98 add silent line-line reversed
            EV_SilentLineTeleport(line, side, thing, true);
            break;

          case 265: //jff 4/14/98 add monster-only silent line-line reversed
            if (!thing->player)
              EV_SilentLineTeleport(line, side, thing, true);
            break;

          case 267: //jff 4/14/98 add monster-only silent line-line
            if (!thing->player)
              EV_SilentLineTeleport(line, side, thing, false);
            break;

          case 269: //jff 4/14/98 add monster-only silent
            if (!thing->player)
              EV_SilentTeleport(line, side, thing);
            break;
            //jff 1/29/98 end of added WR linedef types

          }
      break;
    }
}

//
// P_ShootSpecialLine - Gun trigger special dispatcher
//
// Called when a thing shoots a special line with bullet, shell, saw, or fist.
//
// jff 02/12/98 all G1 lines were fixed to check the result from the EV_
// function before clearing the special. This avoids losing the function
// of the line, should the sector already be in motion when the line is
// impacted. Change is qualified by demo_compatibility.
//
void P_ShootSpecialLine(mobj_t *thing, line_t *line, int side)
{
  //jff 02/04/98 add check here for generalized linedef
  if (!demo_compatibility)
    {
      // pointer to line function is NULL by default, set non-null if
      // line special is gun triggered generalized linedef type
      int (*linefunc)(line_t *line)=NULL;

      // check each range of generalized linedefs
      if ((unsigned)line->special >= GenFloorBase)
        {
          if (!thing->player)
            if ((line->special & FloorChange) || !(line->special & FloorModel))
              return;   // FloorModel is "Allow Monsters" if FloorChange is 0
          if (!line->tag) //jff 2/27/98 all gun generalized types require tag
            return;

          linefunc = EV_DoGenFloor;
        }
      else
        if ((unsigned)line->special >= GenCeilingBase)
          {
            if (!thing->player)
              if ((line->special & CeilingChange) || !(line->special & CeilingModel))
                return;   // CeilingModel is "Allow Monsters" if CeilingChange is 0
            if (!line->tag) //jff 2/27/98 all gun generalized types require tag
              return;
            linefunc = EV_DoGenCeiling;
          }
        else
          if ((unsigned)line->special >= GenDoorBase)
            {
              if (!thing->player)
                {
                  if (!(line->special & DoorMonster))
                    return;   // monsters disallowed from this door
                  if (line->flags & ML_SECRET) // they can't open secret doors either
                    return;
                }
              if (!line->tag) //jff 3/2/98 all gun generalized types require tag
                return;
              linefunc = EV_DoGenDoor;
            }
          else
            if ((unsigned)line->special >= GenLockedBase)
              {
                if (!thing->player)
                  return;   // monsters disallowed from unlocking doors
                if (((line->special&TriggerType)==GunOnce) || ((line->special&TriggerType)==GunMany))
                  { //jff 4/1/98 check for being a gun type before reporting door type
                    if (!P_CanUnlockGenDoor(line,thing->player))
                      return;
                  }
                else
                  return;
                if (!line->tag) //jff 2/27/98 all gun generalized types require tag
                  return;

                linefunc = EV_DoGenLockedDoor;
              }
            else
              if ((unsigned)line->special >= GenLiftBase)
                {
                  if (!thing->player)
                    if (!(line->special & LiftMonster))
                      return; // monsters disallowed
                  linefunc = EV_DoGenLift;
                }
              else
                if ((unsigned)line->special >= GenStairsBase)
                  {
                    if (!thing->player)
                      if (!(line->special & StairMonster))
                        return; // monsters disallowed
                    if (!line->tag) //jff 2/27/98 all gun generalized types require tag
                      return;
                    linefunc = EV_DoGenStairs;
                  }
                else
                  if ((unsigned)line->special >= GenCrusherBase)
                    {
                      if (!thing->player)
                        if (!(line->special & StairMonster))
                          return; // monsters disallowed
                      if (!line->tag) //jff 2/27/98 all gun generalized types require tag
                        return;
                      linefunc = EV_DoGenCrusher;
                    }

      if (linefunc)
        switch((line->special & TriggerType) >> TriggerTypeShift)
          {
          case GunOnce:
            if (linefunc(line))
              P_ChangeSwitchTexture(line,0);
            return;
          case GunMany:
            if (linefunc(line))
              P_ChangeSwitchTexture(line,1);
            return;
          default:  // if not a gun type, do nothing here
            return;
          }
    }

  // Impacts that other things can activate.
  if (!thing->player)
    {
      int ok = 0;
      switch(line->special)
        {
        case 46:
          // 46 GR Open door on impact weapon is monster activatable
          ok = 1;
          break;
        }
      if (!ok)
        return;
    }

  if (!P_CheckTag(line))  //jff 2/27/98 disallow zero tag on some types
    return;

  switch(line->special)
    {
    case 24:
      // 24 G1 raise floor to highest adjacent
      if (EV_DoFloor(line,raiseFloor) || demo_compatibility)
        P_ChangeSwitchTexture(line,0);
      break;

    case 46:
      // 46 GR open door, stay open
      EV_DoDoor(line,doorOpen);
      P_ChangeSwitchTexture(line,1);
      break;

    case 47:
      // 47 G1 raise floor to nearest and change texture and type
      if (EV_DoPlat(line,raiseToNearestAndChange,0) || demo_compatibility)
        P_ChangeSwitchTexture(line,0);
      break;

    // ID24 Music Changers
    case 2061: case 2067: case 2091: case 2097:
    case 2062: case 2068: case 2092: case 2098:
      EV_ChangeMusic(line, side);
      break;

    case 2080:
      line->special = 0;
      // fallthrough

    case 2081:
    {
      int colormap_index = side ? line->backtint : line->fronttint;
      for (int s = -1; (s = P_FindSectorFromLineTag(line, s)) >= 0;)
      {
        sectors[s].tint = colormap_index;
      }
      break;
    }

      //jff 1/30/98 added new gun linedefs here
      // killough 1/31/98: added demo_compatibility check, added inner switch

    default:
      if (!demo_compatibility)
        switch (line->special)
          {
          // G1 - Exit to the next map and reset inventory.
          case 2071:
            if (demo_version < DV_ID24)
              break;
            reset_inventory = true;
            // fallthrough

          case 197:
            // Exit to next level

            // killough 10/98: prevent zombies from exiting levels
            if(thing->player && thing->player->health<=0 && !comp[comp_zombie])
              break;
            P_ChangeSwitchTexture(line,0);
            G_ExitLevel();
            break;

          // G1 - Exit to the secret map and reset inventory.
          case 2074:
            if (demo_version < DV_ID24)
              break;
            reset_inventory = true;
            // fallthrough

          case 198:
            // Exit to secret level

            // killough 10/98: prevent zombies from exiting levels
            if(thing->player && thing->player->health<=0 && !comp[comp_zombie])
              break;
            P_ChangeSwitchTexture(line,0);
            G_SecretExitLevel();
            break;
            //jff end addition of new gun linedefs
          }
      break;
    }
}

int disable_nuke;  // killough 12/98: nukage disabling cheat

//
// P_PlayerInSpecialSector()
//
// Called every tick frame
//  that the player origin is in a special sector
//
// Changed to ignore sector types the engine does not recognize
//

static void P_SecretRevealed(player_t *player)
{
  if (hud_secret_message && player == &players[consoleplayer])
  {
    if (hud_secret_message == SECRETMESSAGE_COUNT)
    {
      static char str_count[32];

      M_snprintf(str_count, sizeof(str_count), "Secret %d of %d revealed!",
                 player->secretcount, totalsecret);

      player->secretmessage = str_count;
    }
    else
    {
      player->secretmessage = s_HUSTR_SECRETFOUND;
    }

    S_StartSound(NULL, sfx_secret);
  }
}

void P_PlayerInSpecialSector (player_t *player)
{
  sector_t *sector = player->mo->subsector->sector;

  // Falling, not all the way down yet?
  // Sector specials don't apply in mid-air
  if (player->mo->z != sector->floorheight)
    return;

  // Has hit ground.
  //jff add if to handle old vs generalized types
  if (sector->special<32) // regular sector specials
    {
      if (sector->special==9)     // killough 12/98
	{
          // Tally player in secret sector, clear secret special
          player->secretcount++;
          sector->special = 0;

          P_SecretRevealed(player);
	}
      else
	if (!disable_nuke)  // killough 12/98: nukage disabling cheat
	{
	  method_t mod = MOD_None;

	  switch (flatterrain[sector->floorpic])
	  {
	      case terrain_lava:
	          mod = MOD_Lava;
	          break;
	      case terrain_slime:
	          mod = MOD_Slime;
	          break;
	      default:
	          break;
	  }

	  switch (sector->special)
	    {
	    case 5:
	      // 5/10 unit damage per 31 ticks
	      if (!player->powers[pw_ironfeet])
		if (!(leveltime&0x1f))
		  P_DamageMobjBy (player->mo, NULL, NULL, 10, mod);
	      break;

	    case 7:
	      // 2/5 unit damage per 31 ticks
	      if (!player->powers[pw_ironfeet])
		if (!(leveltime&0x1f))
		  P_DamageMobjBy (player->mo, NULL, NULL, 5, mod);
	      break;

	    case 16:
	      // 10/20 unit damage per 31 ticks
	    case 4:
	      // 10/20 unit damage plus blinking light (light already spawned)
	      if (!player->powers[pw_ironfeet]
		  || (P_Random(pr_slimehurt)<5) ) // even with suit, take damage
		{
		  if (!(leveltime&0x1f))
		    P_DamageMobjBy (player->mo, NULL, NULL, 20, mod);
		}
	      break;

	    case 11:
	      // Exit on health < 11, take 10/20 damage per 31 ticks
	      if (comp[comp_god])   // killough 2/21/98: add compatibility switch
		player->cheats &= ~CF_GODMODE; // on godmode cheat clearing
	      // does not affect invulnerability
	      if (!(leveltime&0x1f))
		P_DamageMobjBy (player->mo, NULL, NULL, 20, mod);

	      if (player->health <= 10)
		G_ExitLevel();
	      break;

	    default:
	      //jff 1/24/98 Don't exit as DOOM2 did, just ignore
	      break;
	    }
	}
    }
  else //jff 3/14/98 handle extended sector types for secrets and damage
    {
      if (mbf21 && sector->special & DEATH_MASK)
      {
        int i;

        switch ((sector->special & DAMAGE_MASK) >> DAMAGE_SHIFT)
        {
          case 0:
            if (!player->powers[pw_invulnerability] && !player->powers[pw_ironfeet])
              P_DamageMobj(player->mo, NULL, NULL, 10000);
            break;
          case 1:
            P_DamageMobj(player->mo, NULL, NULL, 10000);
            break;
          case 2:
            for (i = 0; i < MAXPLAYERS; i++)
              if (playeringame[i])
                P_DamageMobj(players[i].mo, NULL, NULL, 10000);
            G_ExitLevel();
            break;
          case 3:
            for (i = 0; i < MAXPLAYERS; i++)
              if (playeringame[i])
                P_DamageMobj(players[i].mo, NULL, NULL, 10000);
            G_SecretExitLevel();
            break;
        }
      }
      else if (!disable_nuke)  // killough 12/98: nukage disabling cheat
	switch ((sector->special&DAMAGE_MASK)>>DAMAGE_SHIFT)
	  {
	  case 0: // no damage
	    break;
	  case 1: // 2/5 damage per 31 ticks
	    if (!player->powers[pw_ironfeet])
	      if (!(leveltime&0x1f))
		P_DamageMobj (player->mo, NULL, NULL, 5);
	    break;
	  case 2: // 5/10 damage per 31 ticks
	    if (!player->powers[pw_ironfeet])
	      if (!(leveltime&0x1f))
		P_DamageMobj (player->mo, NULL, NULL, 10);
	    break;
	  case 3: // 10/20 damage per 31 ticks
	    if (!player->powers[pw_ironfeet]
		|| (P_Random(pr_slimehurt)<5))  // take damage even with suit
	      {
		if (!(leveltime&0x1f))
		  P_DamageMobj (player->mo, NULL, NULL, 20);
	      }
	    break;
	  }

      if (sector->special&SECRET_MASK)
        {
          player->secretcount++;
          sector->special &= ~SECRET_MASK;
          if (sector->special<32) // if all extended bits clear,
            sector->special=0;    // sector is not special anymore
          P_SecretRevealed(player);
        }

      // phares 3/19/98:
      //
      // If FRICTION_MASK or PUSH_MASK is set, we don't care at this
      // point, since the code to deal with those situations is
      // handled by Thinkers.
    }
}

//
// P_UpdateSpecials()
//
// Check level timer, frag counter,
// animate flats, scroll walls,
// change button textures
//
// Reads and modifies globals:
//  levelTimer, levelTimeCount,
//  levelFragLimit, levelFragLimitCount
//

boolean         levelTimer;
int             levelTimeCount;
boolean         levelFragLimit;      // Ty 03/18/98 Added -frags support
int             levelFragLimitCount; // Ty 03/18/98 Added -frags support

void P_UpdateSpecials (void)
{
  anim_t*     anim;
  int         pic;
  int         i;

  // Downcount level timer, exit level if elapsed
  if (levelTimer == true)
  {
    levelTimeCount--;
    if (!levelTimeCount)
      G_ExitLevel();
  }

  // Check frag counters, if frag limit reached, exit level // Ty 03/18/98
  //  Seems like the total frags should be kept in a simple
  //  array somewhere, but until they are...
  if (levelFragLimit == true)  // we used -frags so compare count
    {
      int k,m,fragcount,exitflag=false;
      for (k=0;k<MAXPLAYERS;k++)
        {
          if (!playeringame[k]) continue;
          fragcount = 0;
          for (m=0;m<MAXPLAYERS;m++)
            {
              if (!playeringame[m]) continue;
              fragcount += (m!=k)?  players[k].frags[m] : -players[k].frags[m];
            }
          if (fragcount >= levelFragLimitCount) exitflag = true;
          if (exitflag == true) break; // skip out of the loop--we're done
        }
      if (exitflag == true)
        G_ExitLevel();
    }

  // Animate flats and textures globally
  for (anim = anims ; anim < lastanim ; anim++)
    for (i = 0 ; i < anim->numpics ; i++)
      {
        pic = anim->basepic + ( (leveltime/anim->speed + i)%anim->numpics );
        if (anim->istexture)
          texturetranslation[anim->basepic + i] = pic;
        else
        {
          flattranslation[anim->basepic + i] = pic;
          // [crispy] add support for SMMU swirling flats
          if (anim->speed > 65535 || anim->numpics == 1 || r_swirl)
            flattranslation[anim->basepic + i] = -1;
        }
      }

  // Check buttons (retriggerable switches) and change texture on timeout
  for (i = 0; i < MAXBUTTONS; i++)
    if (buttonlist[i].btimer)
      {
        buttonlist[i].btimer--;
        if (!buttonlist[i].btimer)
          {
            switch(buttonlist[i].where)
              {
              case top:
                sides[buttonlist[i].line->sidenum[0]].toptexture =
                  buttonlist[i].btexture;
                break;

              case middle:
                sides[buttonlist[i].line->sidenum[0]].midtexture =
                  buttonlist[i].btexture;
                break;

              case bottom:
                sides[buttonlist[i].line->sidenum[0]].bottomtexture =
                  buttonlist[i].btexture;
                break;
              }
            S_StartSound((mobj_t *)&buttonlist[i].soundorg,sfx_swtchn);
            memset(&buttonlist[i],0,sizeof(button_t));
          }
      }

  // [crispy] draw fuzz effect independent of rendering frame rate
  R_SetFuzzPosTic();

  R_UpdateSkies();
}

//////////////////////////////////////////////////////////////////////
//
// Sector and Line special thinker spawning at level startup
//
//////////////////////////////////////////////////////////////////////

//
// EV_RotateOffsetFlat
//
// As of the ID24, this action only contains the static specials that are
// triggered at spawn time:
// * Offset target floor texture by line direction
// * Offset target ceiling texture by line direction
// * Offset target floor and ceiling texture by line direction
// * Rotate target floor texture by line angle
// * Rotate target ceiling texture by line angle
// * Rotate target floor and ceiling texture by line angle
// * Offset then rotate target floor texture by line direction and angle
// * Offset then rotate target ceiling texture by line direction and angle
// * Offset then rotate target floor and ceiling texture by line direction and angle
//

void EV_RotateOffsetFlat(line_t *line, sector_t *sector)
{
  boolean offset_floor   = false;
  boolean offset_ceiling = false;
  boolean rotate_floor   = false;
  boolean rotate_ceiling = false;

  int s = -1;

  switch (line->special)
  {
    case 2048:
      offset_floor   = true;
      break;
    case 2049:
      offset_ceiling = true;
      break;
    case 2050:
      offset_floor   = true;
      offset_ceiling = true;
      break;
    case 2051:
      rotate_floor   = true;
      break;
    case 2052:
      rotate_ceiling = true;
      break;
    case 2053:
      rotate_floor   = true;
      rotate_ceiling = true;
      break;
    case 2054:
      offset_floor   = true;
      rotate_floor   = true;
      break;
    case 2055:
      offset_ceiling = true;
      rotate_ceiling = true;
      break;
    case 2056:
      offset_floor   = true;
      offset_ceiling = true;
      rotate_floor   = true;
      rotate_ceiling = true;
      break;
  }

  for (s = -1; (s = P_FindSectorFromLineTag(line, s)) >= 0;)
  {
    if (offset_floor)
    {
      sectors[s].floor_xoffs -= line->dx;
      sectors[s].floor_yoffs += line->dy;
    }

    if (offset_ceiling)
    {
      sectors[s].ceiling_xoffs -= line->dx;
      sectors[s].ceiling_yoffs += line->dy;
    }

    if (rotate_floor)
    {
      sectors[s].floor_rotation -= line->angle;
    }

    if (rotate_ceiling)
    {
      sectors[s].ceiling_rotation -= line->angle;
    }
  }
}

//
// P_SpawnSpecials
// After the map has been loaded,
//  scan for specials that spawn thinkers
//

// Parses command line parameters.
void P_SpawnSpecials (void)
{
  sector_t*   sector;
  int         i;

  // See if -timer needs to be used.
  levelTimer = false;

  if (timelimit > 0 && deathmatch)
  {
    levelTimer = true;
    levelTimeCount = timelimit * 60 * TICRATE;
  }
  else
  {
    levelTimer = false;
  }

  // See if -frags has been used
  levelFragLimit = false;

  //!
  // @category net
  // @arg <n>
  //
  // The -frags option allows you to deathmatch until one player achieves <n>
  // frags, at which time the level ends and scores are displayed. If <n> is
  // not specified the match is to 10 frags.
  //

  i = M_CheckParm("-frags");  // Ty 03/18/98 Added -frags support
  if (i && deathmatch)
    {
      int frags;
      frags = M_ParmArgToInt(i);
      if (frags <= 0) frags = 10;  // default 10 if no count provided
      levelFragLimit = true;
      levelFragLimitCount = frags;
    }


  //  Init special sectors.
  sector = sectors;
  for (i=0 ; i<numsectors ; i++, sector++)
    {
      if (!sector->special)
        continue;

      if (sector->special&SECRET_MASK) //jff 3/15/98 count extended
        totalsecret++;                 // secret sectors too

      switch (sector->special&31)
        {
        case 1:
          // random off
          P_SpawnLightFlash (sector);
          break;

        case 2:
          // strobe fast
          P_SpawnStrobeFlash(sector,FASTDARK,0);
          break;

        case 3:
          // strobe slow
          P_SpawnStrobeFlash(sector,SLOWDARK,0);
          break;

        case 4:
          // strobe fast/death slime
          P_SpawnStrobeFlash(sector,FASTDARK,0);
          sector->special |= 3<<DAMAGE_SHIFT; //jff 3/14/98 put damage bits in
          break;

        case 8:
          // glowing light
          P_SpawnGlowingLight(sector);
          break;
        case 9:
          // secret sector
          if (sector->special<32) //jff 3/14/98 bits don't count unless not
            totalsecret++;        // a generalized sector type
          break;

        case 10:
          // door close in 30 seconds
          P_SpawnDoorCloseIn30 (sector);
          break;

        case 12:
          // sync strobe slow
          P_SpawnStrobeFlash (sector, SLOWDARK, 1);
          break;

        case 13:
          // sync strobe fast
          P_SpawnStrobeFlash (sector, FASTDARK, 1);
          break;

        case 14:
          // door raise in 5 minutes
          P_SpawnDoorRaiseIn5Mins (sector, i);
          break;

        case 17:
          // fire flickering
          P_SpawnFireFlicker(sector);
          break;
        }
    }

  P_RemoveAllActiveCeilings();  // jff 2/22/98 use killough's scheme

  P_RemoveAllActivePlats();     // killough

  for (i = 0;i < MAXBUTTONS;i++)
    memset(&buttonlist[i],0,sizeof(button_t));

  // P_InitTagLists() must be called before P_FindSectorFromLineTag()
  // or P_FindLineFromLineTag() can be called.

  P_InitTagLists();   // killough 1/30/98: Create xref tables for tags

  P_SpawnScrollers(); // killough 3/7/98: Add generalized scrollers

  if (!demo_compatibility)
  {
  P_SpawnFriction();  // phares 3/12/98: New friction model using linedefs

  P_SpawnPushers();   // phares 3/20/98: New pusher model using linedefs
  }

  for (i=0; i<numlines; i++)
    switch (lines[i].special)
      {
        int s, sec;

        // killough 3/7/98:
        // support for drawn heights coming from different sector
      case 242:
        sec = sides[*lines[i].sidenum].sector-sectors;
        for (s = -1; (s = P_FindSectorFromLineTag(lines+i,s)) >= 0;)
          sectors[s].heightsec = sec;
        break;

        // killough 3/16/98: Add support for setting
        // floor lighting independently (e.g. lava)
      case 213:
        sec = sides[*lines[i].sidenum].sector-sectors;
        for (s = -1; (s = P_FindSectorFromLineTag(lines+i,s)) >= 0;)
          sectors[s].floorlightsec = sec;
        break;

        // killough 4/11/98: Add support for setting
        // ceiling lighting independently
      case 261:
        sec = sides[*lines[i].sidenum].sector-sectors;
        for (s = -1; (s = P_FindSectorFromLineTag(lines+i,s)) >= 0;)
          sectors[s].ceilinglightsec = sec;
        break;

        // killough 10/98:
        //
        // Support for sky textures being transferred from sidedefs.
        // Allows scrolling and other effects (but if scrolling is
        // used, then the same sector tag needs to be used for the
        // sky sector, the sky-transfer linedef, and the scroll-effect
        // linedef). Still requires user to use F_SKY1 for the floor
        // or ceiling texture, to distinguish floor and ceiling sky.

      case 271:   // Regular sky
      case 272:   // Same, only flipped
      {
        // Pre-calculate sky color
        skyindex_t skyindex = R_AddLevelskyFromLine(&sides[lines[i].sidenum[0]]);
        for (s = -1; (s = P_FindSectorFromLineTag(lines+i,s)) >= 0;)
          sectors[s].floorsky = sectors[s].ceilingsky = skyindex | PL_SKYFLAT;
        break;
      }

      case 2048: case 2049: case 2050:
      case 2051: case 2052: case 2053:
      case 2054: case 2055: case 2056:
        EV_RotateOffsetFlat(&lines[i], sectors);
        break;

      case 2075:
        for (int s = -1; (s = P_FindSectorFromLineTag(&lines[i], s)) >= 0;)
        {
          sectors[s].tint = lines[i].fronttint;
        }
        break;
      }
}

// killough 2/28/98:
//
// This function, with the help of r_plane.c and r_bsp.c, supports generalized
// scrolling floors and walls, with optional mobj-carrying properties, e.g.
// conveyor belts, rivers, etc. A linedef with a special type affects all
// tagged sectors the same way, by creating scrolling and/or object-carrying
// properties. Multiple linedefs may be used on the same sector and are
// cumulative, although the special case of scrolling a floor and carrying
// things on it, requires only one linedef. The linedef's direction determines
// the scrolling direction, and the linedef's length determines the scrolling
// speed. This was designed so that an edge around the sector could be used to
// control the direction of the sector's scrolling, which is usually what is
// desired.
//
// Process the active scrollers.
//
// This is the main scrolling code
// killough 3/7/98

static void T_Scroll(scroll_t *s)
{
  fixed_t dx = s->dx, dy = s->dy;

  if (s->control != -1)
    {   // compute scroll amounts based on a sector's height changes
      fixed_t height = sectors[s->control].floorheight +
        sectors[s->control].ceilingheight;
      fixed_t delta = height - s->last_height;
      s->last_height = height;
      dx = FixedMul(dx, delta);
      dy = FixedMul(dy, delta);
    }

  // killough 3/14/98: Add acceleration
  if (s->accel)
    {
      s->vdx = dx += s->vdx;
      s->vdy = dy += s->vdy;
    }

  if (!(dx | dy))                   // no-op if both (x,y) offsets 0
    return;

  switch (s->type)
    {
      side_t *side;
      sector_t *sec;
      fixed_t height, waterheight;  // killough 4/4/98: add waterheight
      msecnode_t *node;
      mobj_t *thing;

    case sc_side:                   // killough 3/7/98: Scroll wall texture
        side = sides + s->affectee;
        if (side->oldgametic != gametic)
        {
          side->oldtextureoffset = side->textureoffset;
          side->oldrowoffset = side->rowoffset;
          side->oldgametic = gametic;
        }
        side->textureoffset += dx;
        side->rowoffset += dy;
        break;

    case sc_floor:                  // killough 3/7/98: Scroll floor texture
        sec = sectors + s->affectee;
        if (sec->old_floor_offs_gametic != gametic)
        {
          sec->old_floor_xoffs = sec->floor_xoffs;
          sec->old_floor_yoffs = sec->floor_yoffs;
          sec->old_floor_offs_gametic = gametic;
        }
        sec->floor_xoffs += dx;
        sec->floor_yoffs += dy;
        break;

    case sc_ceiling:               // killough 3/7/98: Scroll ceiling texture
        sec = sectors + s->affectee;
        if (sec->old_ceil_offs_gametic != gametic)
        {
          sec->old_ceiling_xoffs = sec->ceiling_xoffs;
          sec->old_ceiling_yoffs = sec->ceiling_yoffs;
          sec->old_ceil_offs_gametic = gametic;
        }
        sec->ceiling_xoffs += dx;
        sec->ceiling_yoffs += dy;
        break;

    case sc_carry:

      // killough 3/7/98: Carry things on floor
      // killough 3/20/98: use new sector list which reflects true members
      // killough 3/27/98: fix carrier bug
      // killough 4/4/98: Underwater, carry things even w/o gravity

      sec = sectors + s->affectee;
      height = sec->floorheight;
      waterheight = sec->heightsec != -1 &&
        sectors[sec->heightsec].floorheight > height ?
        sectors[sec->heightsec].floorheight : INT_MIN;

      // Move objects only if on floor or underwater,
      // non-floating, and clipped.

      for (node = sec->touching_thinglist; node; node = node->m_snext)
        if (!((thing = node->m_thing)->flags & MF_NOCLIP) &&
            (!(thing->flags & MF_NOGRAVITY || thing->z > height) ||
             thing->z < waterheight))
          {
	  thing->momx += dx, thing->momy += dy;
	  thing->intflags |= MIF_SCROLLING;
          }
      break;

    case sc_carry_ceiling:       // to be added later
      break;
    }
}

void T_ScrollAdapter(mobj_t *mobj)
{
    T_Scroll((scroll_t *)mobj);
}

//
// Add_Scroller()
//
// Add a generalized scroller to the thinker list.
//
// type: the enumerated type of scrolling: floor, ceiling, floor carrier,
//   wall, floor carrier & scroller
//
// (dx,dy): the direction and speed of the scrolling or its acceleration
//
// control: the sector whose heights control this scroller's effect
//   remotely, or -1 if no control sector
//
// affectee: the index of the affected object (sector or sidedef)
//
// accel: non-zero if this is an accelerative effect
//

static void Add_Scroller(int type, fixed_t dx, fixed_t dy,
                         int control, int affectee, int accel)
{
  scroll_t *s = arena_alloc(thinkers_arena, 1, scroll_t);
  s->thinker.function.p1 = T_ScrollAdapter;
  s->type = type;
  s->dx = dx;
  s->dy = dy;
  s->accel = accel;
  s->vdx = s->vdy = 0;
  if ((s->control = control) != -1)
    s->last_height =
      sectors[control].floorheight + sectors[control].ceilingheight;
  s->affectee = affectee;
  P_AddThinker(&s->thinker);
}

// Adds wall scroller. Scroll amount is rotated with respect to wall's
// linedef first, so that scrolling towards the wall in a perpendicular
// direction is translated into vertical motion, while scrolling along
// the wall in a parallel direction is translated into horizontal motion.
//
// killough 5/25/98: cleaned up arithmetic to avoid drift due to roundoff
//
// killough 10/98:
// fix scrolling aliasing problems, caused by long linedefs causing overflowing

static void Add_WallScroller(int64_t dx, int64_t dy, const line_t *l,
                             int control, int accel)
{
  fixed_t x = abs(l->dx), y = abs(l->dy), d;
  if (y > x)
    d = x, x = y, y = d;
  d = FixedDiv(x, finesine[(tantoangle[FixedDiv(y,x) >> DBITS] + ANG90)
                          >> ANGLETOFINESHIFT]);

  x = (fixed_t)((dy * -l->dy - dx * l->dx) / d);  // killough 10/98:
  y = (fixed_t)((dy * l->dx - dx * l->dy) / d);   // Use long long arithmetic
  Add_Scroller(sc_side, x, y, control, *l->sidenum, accel);
}

// Amount (dx,dy) vector linedef is shifted right to get scroll amount
#define SCROLL_SHIFT 5

// Factor to scale scrolling effect into mobj-carrying properties = 3/32.
// (This is so scrolling floors and objects on them can move at same speed.)
#define CARRYFACTOR ((fixed_t)(FRACUNIT*.09375))

// Initialize the scrollers
static void P_SpawnScrollers(void)
{
  int i;
  line_t *l = lines;

  for (i=0;i<numlines;i++,l++)
    {
      fixed_t dx = l->dx >> SCROLL_SHIFT;  // direction and speed of scrolling
      fixed_t dy = l->dy >> SCROLL_SHIFT;
      int control = -1, accel = 0;         // no control sector or acceleration
      int special = l->special;

      if (demo_compatibility)
      {
        // Allow all scrollers that do not break demo compatibility.
        // The following are the original Boom scrollers that move Things
        // across the floor, in their accelerative / displacement / normal
        // variants.
        // All other scrollers from Boom through ID24 retain compatibility.
        switch (special)
        {
          case 216:
          case 217:
          case 247:
          case 248:
          case 252:
          case 253:
            continue;
        }
      }

      // killough 3/7/98: Types 245-249 are same as 250-254 except that the
      // first side's sector's heights cause scrolling when they change, and
      // this linedef controls the direction and speed of the scrolling. The
      // most complicated linedef since donuts, but powerful :)
      //
      // killough 3/15/98: Add acceleration. Types 214-218 are the same but
      // are accelerative.

      if (special >= 245 && special <= 249)         // displacement scrollers
        {
          special += 250-245;
          control = sides[*l->sidenum].sector - sectors;
        }
      else
        if (special >= 214 && special <= 218)       // accelerative scrollers
          {
            accel = 1;
            special += 250-214;
            control = sides[*l->sidenum].sector - sectors;
          }

      switch (special)
        {
          register int s;

        case 250:   // scroll effect ceiling
          for (s=-1; (s = P_FindSectorFromLineTag(l,s)) >= 0;)
            Add_Scroller(sc_ceiling, -dx, dy, control, s, accel);
          break;

        case 251:   // scroll effect floor
        case 253:   // scroll and carry objects on floor
          for (s=-1; (s = P_FindSectorFromLineTag(l,s)) >= 0;)
            Add_Scroller(sc_floor, -dx, dy, control, s, accel);
          if (special != 253)
            break;

        case 252: // carry objects on floor
          dx = FixedMul(dx,CARRYFACTOR);
          dy = FixedMul(dy,CARRYFACTOR);
          for (s=-1; (s = P_FindSectorFromLineTag(l,s)) >= 0;)
            Add_Scroller(sc_carry, dx, dy, control, s, accel);
          break;

          // killough 3/1/98: scroll wall according to linedef
          // (same direction and speed as scrolling floors)
        case 254:
          for (s=-1; (s = P_FindLineFromLineTag(l,s)) >= 0;)
            if (s != i)
              Add_WallScroller(dx, dy, lines+s, control, accel);
          break;

        case 255:    // killough 3/2/98: scroll according to sidedef offsets
          s = lines[i].sidenum[0];
          Add_Scroller(sc_side, -sides[s].textureoffset,
                       sides[s].rowoffset, -1, s, accel);
          break;

        // special 255 with tag control

        // Always - Scroll both front and back sidedef's textures and
        // accelerate the scroll value by the target sector's movement
        // divided by 8.
        case 2086:
        case 1026:
          accel = 1;
          // fallthrough

        // Always - Scroll both front and back sidedef's textures
        // according to the target sector's movement divided by 8.
        case 2085:
        case 1025:
          control = sides[*l->sidenum].sector - sectors;
          // fallthrough

        // Always - Scroll both front and back sidedef's textures
        // according to the target sector's scroll values divided by 8
        case 2084:
        case 1024:
          if (l->tag == 0)
            I_Error("Line %d is missing a tag!", i);

          s = lines[i].sidenum[0];
          dx = -sides[s].textureoffset / 8;
          dy = sides[s].rowoffset / 8;
          for (s = -1; (s = P_FindLineFromLineTag(l, s)) >= 0;)
            if (s != i)
            {
              Add_Scroller(sc_side, dx, dy, control, lines[s].sidenum[0], accel);

              if (special >= 2084 && special <= 2086 && lines[s].sidenum[1] != NO_INDEX)
                Add_Scroller(sc_side, -dx, dy, control, lines[s].sidenum[1], accel);
            }
          break;

        // Always - Scroll both front and back sidedef's textures
        // according to the line's left direction.
        case 2082:
          if (lines[i].sidenum[1] != NO_INDEX)
            Add_Scroller(sc_side, -FRACUNIT, 0, -1, lines[i].sidenum[1], accel);
          // fallthrough

        case 48:                  // scroll first side
          Add_Scroller(sc_side,  FRACUNIT, 0, -1, lines[i].sidenum[0], accel);
          break;

        // Always - Scroll both front and back sidedef's textures
        // according to the line's right direction.
        case 2083:
          if (lines[i].sidenum[1] != NO_INDEX)
            Add_Scroller(sc_side, FRACUNIT, 0, -1, lines[i].sidenum[1], accel);
          // fallthrough

        case 85:                  // jff 1/30/98 2-way scroll
          Add_Scroller(sc_side, -FRACUNIT, 0, -1, lines[i].sidenum[0], accel);
          break;
        }
    }
}

// Restored Boom's friction code

/////////////////////////////
//
// Add a friction thinker to the thinker list
//
// Add_Friction adds a new friction thinker to the list of active thinkers.
//

static void Add_Friction(int friction, int movefactor, int affectee)
{
    friction_t *f = arena_alloc(thinkers_arena, 1, friction_t);

    f->thinker.function.p1 = T_FrictionAdapter;
    f->friction = friction;
    f->movefactor = movefactor;
    f->affectee = affectee;
    P_AddThinker(&f->thinker);
}

/////////////////////////////
//
// This is where abnormal friction is applied to objects in the sectors.
// A friction thinker has been spawned for each sector where less or
// more friction should be applied. The amount applied is proportional to
// the length of the controlling linedef.

void T_Friction(friction_t *f)
{
    sector_t *sec;
    mobj_t   *thing;
    msecnode_t* node;

    if (compatibility || !variable_friction)
        return;

    sec = sectors + f->affectee;

    // Be sure the special sector type is still turned on. If so, proceed.
    // Else, bail out; the sector type has been changed on us.

    if (!(sec->special & FRICTION_MASK))
        return;

    // Assign the friction value to players on the floor, non-floating,
    // and clipped. Normally the object's friction value is kept at
    // ORIG_FRICTION and this thinker changes it for icy or muddy floors.

    // In Phase II, you can apply friction to Things other than players.

    // When the object is straddling sectors with the same
    // floorheight that have different frictions, use the lowest
    // friction value (muddy has precedence over icy).

    node = sec->touching_thinglist; // things touching this sector
    while (node)
    {
        thing = node->m_thing;
        if (thing->player &&
            !(thing->flags & (MF_NOGRAVITY | MF_NOCLIP)) &&
            thing->z <= sec->floorheight)
            {
                if ((thing->friction == ORIG_FRICTION) ||     // normal friction?
                    (f->friction < thing->friction))
                    {
                      thing->friction   = f->friction;
                      thing->movefactor = f->movefactor;
                    }
            }
        node = node->m_snext;
    }
}

void T_FrictionAdapter(mobj_t *mobj)
{
    T_Friction((friction_t *)mobj);
}

// killough 3/7/98 -- end generalized scroll effects

////////////////////////////////////////////////////////////////////////////
//
// FRICTION EFFECTS
//
// phares 3/12/98: Start of friction effects
//
// As the player moves, friction is applied by decreasing the x and y
// momentum values on each tic. By varying the percentage of decrease,
// we can simulate muddy or icy conditions. In mud, the player slows
// down faster. In ice, the player slows down more slowly.
//
// The amount of friction change is controlled by the length of a linedef
// with type 223. A length < 100 gives you mud. A length > 100 gives you ice.
//
// Also, each sector where these effects are to take place is given a
// new special type _______. Changing the type value at runtime allows
// these effects to be turned on or off.
//
// Sector boundaries present problems. The player should experience these
// friction changes only when his feet are touching the sector floor. At
// sector boundaries where floor height changes, the player can find
// himself still 'in' one sector, but with his feet at the floor level
// of the next sector (steps up or down). To handle this, Thinkers are used
// in icy/muddy sectors. These thinkers examine each object that is touching
// their sectors, looking for players whose feet are at the same level as
// their floors. Players satisfying this condition are given new friction
// values that are applied by the player movement code later.
//
// killough 8/28/98:
//
// Completely redid code, which did not need thinkers, and which put a heavy
// drag on CPU. Friction is now a property of sectors, NOT objects inside
// them. All objects, not just players, are affected by it, if they touch
// the sector's floor. Code simpler and faster, only calling on friction
// calculations when an object needs friction considered, instead of doing
// friction calculations on every sector during every tic.
//
// Although this -might- ruin Boom demo sync involving friction, it's the only
// way, short of code explosion, to fix the original design bug. Fixing the
// design bug in Boom's original friction code, while maintaining demo sync
// under every conceivable circumstance, would double or triple code size, and
// would require maintenance of buggy legacy code which is only useful for old
// demos. Doom demos, which are more important IMO, are not affected by this
// change.
//
/////////////////////////////
//
// Initialize the sectors where friction is increased or decreased

static void P_SpawnFriction(void)
{
  int i;
  line_t *l = lines;

  // killough 8/28/98: initialize all sectors to normal friction first
  for (i = 0; i < numsectors; i++)
    {
      sectors[i].friction = ORIG_FRICTION;
      sectors[i].movefactor = ORIG_FRICTION_FACTOR;
    }

  for (i = 0 ; i < numlines ; i++,l++)
    if (l->special == 223)
      {
        int length = P_AproxDistance(l->dx,l->dy)>>FRACBITS;
        int friction = (0x1EB8*length)/0x80 + 0xD000;
        int movefactor, s;

        // The following check might seem odd. At the time of movement,
        // the move distance is multiplied by 'friction/0x10000', so a
        // higher friction value actually means 'less friction'.

        if (friction > ORIG_FRICTION)       // ice
          movefactor = ((0x10092 - friction)*(0x70))/0x158;
        else
          movefactor = ((friction - 0xDB34)*(0xA))/0x80;

        if (demo_version >= DV_MBF)
          { // killough 8/28/98: prevent odd situations
            if (friction > FRACUNIT)
              friction = FRACUNIT;
            if (friction < 0)
              friction = 0;
            if (movefactor < 32)
              movefactor = 32;
          }

        for (s = -1; (s = P_FindSectorFromLineTag(l,s)) >= 0 ; )
          {
            // killough 8/28/98:
            //
            // Instead of spawning thinkers, which are slow and expensive,
            // modify the sector's own friction values. Friction should be
            // a property of sectors, not objects which reside inside them.
            // Original code scanned every object in every friction sector
            // on every tic, adjusting its friction, putting unnecessary
            // drag on CPU. New code adjusts friction of sector only once
            // at level startup, and then uses this friction value.

            // Boom's friction code for demo compatibility
            if (!demo_compatibility && demo_version < DV_MBF)
              Add_Friction(friction,movefactor,s);

            sectors[s].friction = friction;
            sectors[s].movefactor = movefactor;
          }
      }
}

//
// phares 3/12/98: End of friction effects
//
////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////
//
// PUSH/PULL EFFECT
//
// phares 3/20/98: Start of push/pull effects
//
// This is where push/pull effects are applied to objects in the sectors.
//
// There are four kinds of push effects
//
// 1) Pushing Away
//
//    Pushes you away from a point source defined by the location of an
//    MT_PUSH Thing. The force decreases linearly with distance from the
//    source. This force crosses sector boundaries and is felt w/in a circle
//    whose center is at the MT_PUSH. The force is felt only if the point
//    MT_PUSH can see the target object.
//
// 2) Pulling toward
//
//    Same as Pushing Away except you're pulled toward an MT_PULL point
//    source. This force crosses sector boundaries and is felt w/in a circle
//    whose center is at the MT_PULL. The force is felt only if the point
//    MT_PULL can see the target object.
//
// 3) Wind
//
//    Pushes you in a constant direction. Full force above ground, half
//    force on the ground, nothing if you're below it (water).
//
// 4) Current
//
//    Pushes you in a constant direction. No force above ground, full
//    force if on the ground or below it (water).
//
// The magnitude of the force is controlled by the length of a controlling
// linedef. The force vector for types 3 & 4 is determined by the angle
// of the linedef, and is constant.
//
// For each sector where these effects occur, the sector special type has
// to have the PUSH_MASK bit set. If this bit is turned off by a switch
// at run-time, the effect will not occur. The controlling sector for
// types 1 & 2 is the sector containing the MT_PUSH/MT_PULL Thing.

#define PUSH_FACTOR 7

/////////////////////////////
//
// Add a push thinker to the thinker list

static void Add_Pusher(int type, int x_mag, int y_mag,
                       mobj_t *source, int affectee)
{
  pusher_t *p = arena_alloc(thinkers_arena, 1, pusher_t);

  p->thinker.function.p1 = T_PusherAdapter;
  p->source = source;
  p->type = type;
  p->x_mag = x_mag>>FRACBITS;
  p->y_mag = y_mag>>FRACBITS;
  p->magnitude = P_AproxDistance(p->x_mag,p->y_mag);
  if (source) // point source exist?
    {
      p->radius = (p->magnitude)<<(FRACBITS+1); // where force goes to zero
      p->x = p->source->x;
      p->y = p->source->y;
    }
  p->affectee = affectee;
  P_AddThinker(&p->thinker);
}

/////////////////////////////
//
// PIT_PushThing determines the angle and magnitude of the effect.
// The object's x and y momentum values are changed.
//
// tmpusher belongs to the point source (MT_PUSH/MT_PULL).
//
// killough 10/98: allow to affect things besides players

pusher_t* tmpusher; // pusher structure for blockmap searches

boolean PIT_PushThing(mobj_t* thing)
{
  if (demo_version < DV_MBF  ?     // killough 10/98: made more general
      thing->player && !(thing->flags & (MF_NOCLIP | MF_NOGRAVITY)) :
      (sentient(thing) || thing->flags & MF_SHOOTABLE) &&
      !(thing->flags & MF_NOCLIP))
    {
      angle_t pushangle;
      fixed_t speed;
      fixed_t sx = tmpusher->x;
      fixed_t sy = tmpusher->y;

      speed = (tmpusher->magnitude -
               ((P_AproxDistance(thing->x - sx,thing->y - sy)
                 >>FRACBITS)>>1))<<(FRACBITS-PUSH_FACTOR-1);

      // killough 10/98: make magnitude decrease with square
      // of distance, making it more in line with real nature,
      // so long as it's still in range with original formula.
      //
      // Removes angular distortion, and makes effort required
      // to stay close to source, grow increasingly hard as you
      // get closer, as expected. Still, it doesn't consider z :(

      if (speed > 0 && demo_version >= DV_MBF)
        {
          int x = (thing->x-sx) >> FRACBITS;
          int y = (thing->y-sy) >> FRACBITS;
          speed = (fixed_t)(((int64_t) tmpusher->magnitude << 23) / (x*x+y*y+1));
        }

      // If speed <= 0, you're outside the effective radius. You also have
      // to be able to see the push/pull source point.

      if (speed > 0 && P_CheckSight(thing,tmpusher->source))
        {
          pushangle = R_PointToAngle2(thing->x,thing->y,sx,sy);
          if (tmpusher->source->type == MT_PUSH)
            pushangle += ANG180;    // away
          pushangle >>= ANGLETOFINESHIFT;
          thing->momx += FixedMul(speed,finecosine[pushangle]);
          thing->momy += FixedMul(speed,finesine[pushangle]);
          thing->intflags |= MIF_SCROLLING;
        }
    }
  return true;
}

/////////////////////////////
//
// T_Pusher looks for all objects that are inside the radius of
// the effect.
//

static void T_Pusher(pusher_t *p)
{
  sector_t *sec;
  mobj_t   *thing;
  msecnode_t* node;
  int xspeed,yspeed;
  int xl,xh,yl,yh,bx,by;
  int radius;
  int ht = 0;

  if (!allow_pushers)
    return;

  sec = sectors + p->affectee;

  // Be sure the special sector type is still turned on. If so, proceed.
  // Else, bail out; the sector type has been changed on us.

  if (!(sec->special & PUSH_MASK))
    return;

  // For constant pushers (wind/current) there are 3 situations:
  //
  // 1) Affected Thing is above the floor.
  //
  //    Apply the full force if wind, no force if current.
  //
  // 2) Affected Thing is on the ground.
  //
  //    Apply half force if wind, full force if current.
  //
  // 3) Affected Thing is below the ground (underwater effect).
  //
  //    Apply no force if wind, full force if current.

  if (p->type == p_push)
    {
      // Seek out all pushable things within the force radius of this
      // point pusher. Crosses sectors, so use blockmap.

      tmpusher = p; // MT_PUSH/MT_PULL point source
      radius = p->radius; // where force goes to zero
      tmbbox[BOXTOP]    = p->y + radius;
      tmbbox[BOXBOTTOM] = p->y - radius;
      tmbbox[BOXRIGHT]  = p->x + radius;
      tmbbox[BOXLEFT]   = p->x - radius;

      xl = (tmbbox[BOXLEFT] - bmaporgx - MAXRADIUS)>>MAPBLOCKSHIFT;
      xh = (tmbbox[BOXRIGHT] - bmaporgx + MAXRADIUS)>>MAPBLOCKSHIFT;
      yl = (tmbbox[BOXBOTTOM] - bmaporgy - MAXRADIUS)>>MAPBLOCKSHIFT;
      yh = (tmbbox[BOXTOP] - bmaporgy + MAXRADIUS)>>MAPBLOCKSHIFT;
      for (bx=xl ; bx<=xh ; bx++)
        for (by=yl ; by<=yh ; by++)
          P_BlockThingsIterator(bx, by, PIT_PushThing, true);
      return;
    }

  // constant pushers p_wind and p_current

  if (sec->heightsec != -1) // special water sector?
    ht = sectors[sec->heightsec].floorheight;
  node = sec->touching_thinglist; // things touching this sector
  for ( ; node ; node = node->m_snext)
    {
      thing = node->m_thing;
      if (!thing->player || (thing->flags & (MF_NOGRAVITY | MF_NOCLIP)))
        continue;
      if (p->type == p_wind)
        {
          if (sec->heightsec == -1) // NOT special water sector
            if (thing->z > thing->floorz) // above ground
              {
                xspeed = p->x_mag; // full force
                yspeed = p->y_mag;
              }
            else // on ground
              {
                xspeed = (p->x_mag)>>1; // half force
                yspeed = (p->y_mag)>>1;
              }
          else // special water sector
            {
              if (thing->z > ht) // above ground
                {
                  xspeed = p->x_mag; // full force
                  yspeed = p->y_mag;
                }
              else
                if (thing->player->viewz < ht) // underwater
                  xspeed = yspeed = 0; // no force
                else // wading in water
                  {
                    xspeed = (p->x_mag)>>1; // half force
                    yspeed = (p->y_mag)>>1;
                  }
            }
        }
      else // p_current
        {
          if (sec->heightsec == -1) // NOT special water sector
            if (thing->z > sec->floorheight) // above ground
              xspeed = yspeed = 0; // no force
            else // on ground
              {
                xspeed = p->x_mag; // full force
                yspeed = p->y_mag;
              }
          else // special water sector
            if (thing->z > ht) // above ground
              xspeed = yspeed = 0; // no force
            else // underwater
              {
                xspeed = p->x_mag; // full force
                yspeed = p->y_mag;
              }
        }
      thing->momx += xspeed<<(FRACBITS-PUSH_FACTOR);
      thing->momy += yspeed<<(FRACBITS-PUSH_FACTOR);
      thing->intflags |= MIF_SCROLLING;
    }
}

void T_PusherAdapter(mobj_t *mobj)
{
    T_Pusher((pusher_t *)mobj);
}

/////////////////////////////
//
// P_GetPushThing() returns a pointer to an MT_PUSH or MT_PULL thing,
// NULL otherwise.

mobj_t* P_GetPushThing(int s)
{
  mobj_t* thing;
  sector_t* sec;

  sec = sectors + s;
  thing = sec->thinglist;
  while (thing)
    {
      switch(thing->type)
        {
        case MT_PUSH:
        case MT_PULL:
          return thing;
        default:
          break;
        }
      thing = thing->snext;
    }
  return NULL;
}

/////////////////////////////
//
// Initialize the sectors where pushers are present
//

static void P_SpawnPushers(void)
{
  int i;
  line_t *l = lines;
  register int s;
  mobj_t* thing;

  for (i = 0 ; i < numlines ; i++,l++)
    switch(l->special)
      {
      case 224: // wind
        for (s = -1; (s = P_FindSectorFromLineTag(l,s)) >= 0 ; )
          Add_Pusher(p_wind,l->dx,l->dy,NULL,s);
        break;
      case 225: // current
        for (s = -1; (s = P_FindSectorFromLineTag(l,s)) >= 0 ; )
          Add_Pusher(p_current,l->dx,l->dy,NULL,s);
        break;
      case 226: // push/pull
        for (s = -1; (s = P_FindSectorFromLineTag(l,s)) >= 0 ; )
          {
            thing = P_GetPushThing(s);
            if (thing) // No MT_P* means no effect
              Add_Pusher(p_push,l->dx,l->dy,thing,s);
          }
        break;
      }
}

//
// phares 3/20/98: End of Pusher effects
//
////////////////////////////////////////////////////////////////////////////


//----------------------------------------------------------------------------
//
// $Log: p_spec.c,v $
// Revision 1.56  1998/05/25  10:40:30  killough
// Fix wall scrolling bug
//
// Revision 1.55  1998/05/23  10:23:32  jim
// Fix numeric changer loop corruption
//
// Revision 1.54  1998/05/11  06:52:56  phares
// Documentation
//
// Revision 1.53  1998/05/07  00:51:34  killough
// beautification
//
// Revision 1.52  1998/05/04  11:47:23  killough
// Add #include d_deh.h
//
// Revision 1.51  1998/05/04  02:22:06  jim
// formatted p_specs, moved a coupla routines to p_floor
//
// Revision 1.50  1998/05/03  22:06:30  killough
// Provide minimal required headers at top (no other changes)
//
// Revision 1.49  1998/04/17  18:57:51  killough
// fix comment
//
// Revision 1.48  1998/04/17  18:49:02  killough
// Fix lack of animation in flats
//
// Revision 1.47  1998/04/17  10:24:47  killough
// Add P_FindLineFromLineTag(), add CARRY_CEILING macro
//
// Revision 1.46  1998/04/14  18:49:36  jim
// Added monster only and reverse teleports
//
// Revision 1.45  1998/04/12  02:05:25  killough
// Add ceiling light setting, start ceiling carriers
//
// Revision 1.44  1998/04/06  11:05:23  jim
// Remove LEESFIXES, AMAP bdg->247
//
// Revision 1.43  1998/04/06  04:39:04  killough
// Make scroll carriers carry all things underwater
//
// Revision 1.42  1998/04/01  16:39:11  jim
// Fix keyed door message on gunfire
//
// Revision 1.41  1998/03/29  20:13:35  jim
// Fixed use of 2S flag in Donut linedef
//
// Revision 1.40  1998/03/28  18:13:24  killough
// Fix conveyor bug (carry objects not touching but overhanging)
//
// Revision 1.39  1998/03/28  05:32:48  jim
// Text enabling changes for DEH
//
// Revision 1.38  1998/03/23  18:38:48  jim
// Switch and animation tables now lumps
//
// Revision 1.37  1998/03/23  15:24:41  phares
// Changed pushers to linedef control
//
// Revision 1.36  1998/03/23  03:32:36  killough
// Make "oof" sounds have true mobj origins (for spy mode hearing)
// Make carrying floors carry objects hanging over edges of sectors
//
// Revision 1.35  1998/03/20  14:24:36  jim
// Gen ceiling target now shortest UPPER texture
//
// Revision 1.34  1998/03/20  00:30:21  phares
// Changed friction to linedef control
//
// Revision 1.33  1998/03/18  23:14:02  jim
// Deh text additions
//
// Revision 1.32  1998/03/16  15:43:33  killough
// Add accelerative scrollers, merge Jim's changes
//
// Revision 1.29  1998/03/13  14:05:44  jim
// Fixed arith overflow in some linedef types
//
// Revision 1.28  1998/03/12  21:54:12  jim
// Freed up 12 linedefs for use as vectors
//
// Revision 1.26  1998/03/09  10:57:55  jim
// Allowed Lee's change to 0 tag trigger compatibility
//
// Revision 1.25  1998/03/09  07:23:43  killough
// Add generalized scrollers, renumber some linedefs
//
// Revision 1.24  1998/03/06  12:34:39  jim
// Renumbered 300+ linetypes under 256 for DCK
//
// Revision 1.23  1998/03/05  16:59:10  jim
// Fixed inability of monsters/barrels to use new teleports
//
// Revision 1.22  1998/03/04  07:33:04  killough
// Fix infinite loop caused by multiple carrier references
//
// Revision 1.21  1998/03/02  15:32:57  jim
// fixed errors in numeric model sector search and 0 tag trigger defeats
//
// Revision 1.20  1998/03/02  12:13:57  killough
// Add generalized scrolling flats & walls, carrying floors
//
// Revision 1.19  1998/02/28  01:24:53  jim
// Fixed error in 0 tag trigger fix
//
// Revision 1.17  1998/02/24  08:46:36  phares
// Pushers, recoil, new friction, and over/under work
//
// Revision 1.16  1998/02/23  23:47:05  jim
// Compatibility flagged multiple thinker support
//
// Revision 1.15  1998/02/23  04:52:33  killough
// Allow god mode cheat to work on E1M8 unless compatibility
//
// Revision 1.14  1998/02/23  00:42:02  jim
// Implemented elevators
//
// Revision 1.12  1998/02/17  05:55:06  killough
// Add silent teleporters
// Change RNG calling sequence
// Cosmetic changes
//
// Revision 1.11  1998/02/13  03:28:06  jim
// Fixed W1,G1 linedefs clearing untriggered special, cosmetic changes
//
// Revision 1.10  1998/02/08  05:35:39  jim
// Added generalized linedef types
//
// Revision 1.8  1998/02/02  13:34:26  killough
// Performance tuning, program beautification
//
// Revision 1.7  1998/01/30  14:43:54  jim
// Added gun exits, right scrolling walls and ceiling mover specials
//
// Revision 1.4  1998/01/27  16:19:29  jim
// Fixed subroutines used by linedef triggers and a NULL ref in Donut
//
// Revision 1.3  1998/01/26  19:24:26  phares
// First rev with no ^Ms
//
// Revision 1.2  1998/01/25  20:24:45  jim
// Fixed crusher floor, lowerandChange floor types, and unknown sector special error
//
// Revision 1.1.1.1  1998/01/19  14:03:01  rand
// Lee's Jan 19 sources
//
//----------------------------------------------------------------------------
