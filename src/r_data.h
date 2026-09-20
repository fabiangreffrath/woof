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
//  Refresh module, data I/O, caching, retrieval of graphics
//  by name.
//
//-----------------------------------------------------------------------------

#ifndef __R_DATA__
#define __R_DATA__

#include "doomtype.h"

// A single patch from a texture definition, basically
// a rectangular area within the texture rectangle.
typedef struct
{
  int originx, originy;  // Block origin, which has already accounted
  int patch;             // for the internal origin of the patch.
} texpatch_t;

// A maptexturedef_t describes a rectangular texture, which is composed
// of one or more mappatch_t structures that arrange graphic patches.

typedef struct
{
  char  name[8];         // Keep name for switch changing, etc.
  int   next, index;     // killough 1/31/98: used in hashing algorithm
  short width, height;
  short patchcount;      // All the patches[patchcount] are drawn
  texpatch_t patches[1]; // back-to-front into the cached texture.
} texture_t;

extern texture_t **textures;

// Retrieve column data for span blitting.
byte *R_GetColumn(int tex, int col);
byte *R_GetColumnMasked(int tex, int col);

// I/O, setting up the stuff.
void R_InitData (void);
void R_PrecacheLevel (void);

// Retrieval.
// Floor/ceiling opaque texture tiles,
// lookup by name. For animation?
int R_FlatNumForName (const char* name);   // killough -- const added
byte *R_MissingFlat(void);

// Called by P_Ticker for switches and animations,
// returns the texture number for the texture name.
int R_TextureNumForName (const char *name);    // killough -- const added
int R_CheckTextureNumForName (const char *name); 

int R_ColormapNumForName(const char *name);      // killough 4/4/98

void R_InitColormaps(void);   // killough 8/9/98

void R_InvulMode(void);

extern int numflats;

typedef enum
{
  INVUL_VANILLA,
  INVUL_MBF,
  INVUL_GRAY,
} invul_mode_t;

extern invul_mode_t invul_mode;


// [FG] dark/shaded color translation table
extern byte *cr_dark;
extern byte *cr_shaded;
extern byte invul_gray[];

// symbolic indices into color translation table pointer array
typedef enum xlat_index_e
{
    CR_ORIG = -1,
    CR_BRICK,  // 0
    CR_TAN,    // 1
    CR_GRAY,   // 2
    CR_GREEN,  // 3
    CR_BROWN,  // 4
    CR_GOLD,   // 5
    CR_RED,    // 6
    CR_BLUE1,  // 7
    CR_ORANGE, // 8
    CR_YELLOW, // 9
    CR_BLUE2,  // 10
    CR_BLACK,  // 11
    CR_PURPLE, // 12
    CR_WHITE,  // 13
    CR_BRIGHT, // 14
    CR_NONE,   // 15 // [FG] dummy
    CR_LIMIT   // 16 //jff 2/27/98 added for range check
} xlat_index_t;

typedef struct
{
    const char *name;
    const char *str;
    byte *lump;
    byte *table;
} xlat_t;

extern xlat_t xlat[CR_LIMIT];

#define ORIG_S  "\x1b\x2f"
#define BRICK_S "\x1b\x30"
#define TAN_S   "\x1b\x31"
#define GRAY_S  "\x1b\x32"
#define GREEN_S "\x1b\x33"
#define BROWN_S "\x1b\x34"
#define GOLD_S  "\x1b\x35"
#define RED_S   "\x1b\x36"
#define BLUE1_S "\x1b\x37"
#define BLUE2_S "\x1b\x3a"

xlat_index_t R_CRByName(const char *name);

void R_InitColorTranslation(void);

#endif

//----------------------------------------------------------------------------
//
// $Log: r_data.h,v $
// Revision 1.6  1998/05/03  22:55:43  killough
// Add tranmap external declarations
//
// Revision 1.5  1998/04/06  04:48:25  killough
// Add R_ColormapNumForName() prototype
//
// Revision 1.4  1998/03/09  07:26:34  killough
// Add translucency map caching
//
// Revision 1.3  1998/03/02  12:10:05  killough
// Add R_InitTranMap prototype
//
// Revision 1.2  1998/01/26  19:27:34  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:03:08  rand
// Lee's Jan 19 sources
//
//
//----------------------------------------------------------------------------
