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
//      WAD I/O functions.
//
//-----------------------------------------------------------------------------

#ifndef __W_WAD__
#define __W_WAD__

#include "doomtype.h"
#include "z_zone.h"

//
// TYPES
//

// haleyjd 01/21/05: these structs must be packed

#if defined(_MSC_VER)
#pragma pack(push, 1)
#endif

typedef PACKED_PREFIX struct
{
  char identification[4];                  // Should be "IWAD" or "PWAD".
  int  numlumps;
  int  infotableofs;
} PACKED_SUFFIX wadinfo_t;

typedef PACKED_PREFIX struct
{
  int  filepos;
  int  size;
  char name[8];
} PACKED_SUFFIX filelump_t;

#if defined(_MSC_VER)
#pragma pack(pop)
#endif

//
// WADFILE I/O related stuff.
//

typedef struct
{
  // WARNING: order of some fields important (see info.c).

  char  name[8];
  int   size;
  const void *data;     // killough 1/31/98: points to predefined lump data

  // killough 1/31/98: hash table fields, used for ultra-fast hash table lookup
  int index, next;

  // killough 4/17/98: namespace tags, to prevent conflicts between resources
  enum {
    ns_global=0,
    ns_sprites,
    ns_flats,
    ns_colormaps,
    ns_hires // [Woof!] namespace to avoid conflicts with high-resolution textures
  } namespace;

  int handle;
  int position;

  // [FG] WAD file that contains the lump
  const char *wad_file;
} lumpinfo_t;

// killough 1/31/98: predefined lumps
extern const size_t num_predefined_lumps;
extern const lumpinfo_t predefined_lumps[];

extern void       **lumpcache;
extern lumpinfo_t *lumpinfo;
extern int        numlumps;

void W_InitMultipleFiles(void);

// killough 4/17/98: if W_CheckNumForName() called with only
// one argument, pass ns_global as the default namespace

#define W_CheckNumForName(name) (W_CheckNumForName)(name, ns_global)
int     (W_CheckNumForName)(const char* name, int);   // killough 4/17/98
int     W_GetNumForName (const char* name);
int     W_LumpLength (int lump);
void    W_ReadLump (int lump, void *dest);
void    *W_CacheLumpNum(int lump, pu_tag tag);

#define W_CacheLumpName(name,tag) W_CacheLumpNum (W_GetNumForName(name),(tag))

void ExtractFileBase(const char *, char *);       // killough
unsigned W_LumpNameHash(const char *s);           // killough 1/31/98

void I_BeginRead(unsigned int bytes), I_EndRead(void); // killough 10/98

// Function to write all predefined lumps to a PWAD if requested
extern void WriteLumpWad(const char *filename, const lumpinfo_t *lumps, const size_t num_lumps);
extern void WritePredefinedLumpWad(const char *filename); // jff 5/6/98
extern void WriteGeneratedLumpWad(const char *filename);

// [FG] name of the WAD file that contains the lump
const char *W_WadNameForLump (const int lump);
boolean W_IsIWADLump (const int lump);
// check if lump is from WAD
boolean W_IsWADLump (const int lump);
boolean W_LumpExistsWithName(int lump, char *name);
int W_LumpLengthWithName(int lump, char *name);
void W_DemoLumpNameCollision(char **name);

void W_CloseFileDescriptors(void);

#endif

//----------------------------------------------------------------------------
//
// $Log: w_wad.h,v $
// Revision 1.10  1998/05/06  11:32:05  jim
// Moved predefined lump writer info->w_wad
//
// Revision 1.9  1998/05/03  22:43:45  killough
// remove unnecessary #includes
//
// Revision 1.8  1998/05/01  14:55:54  killough
// beautification
//
// Revision 1.7  1998/04/27  02:05:30  killough
// Program beautification
//
// Revision 1.6  1998/04/19  01:14:36  killough
// Reinstate separate namespaces
//
// Revision 1.5  1998/04/17  16:52:21  killough
// back out namespace changes temporarily
//
// Revision 1.4  1998/04/17  10:33:50  killough
// Macroize W_CheckNumForName(), add namespace parameter to functional version
//
// Revision 1.3  1998/02/02  13:35:13  killough
// Improve lump hashing, add predefine lumps
//
// Revision 1.2  1998/01/26  19:28:01  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:03:07  rand
// Lee's Jan 19 sources
//
//
//----------------------------------------------------------------------------
