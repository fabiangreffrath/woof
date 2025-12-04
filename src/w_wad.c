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
//      Handles WAD file header, directory, lump I/O.
//
//-----------------------------------------------------------------------------

#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "doomdef.h"
#include "doomstat.h"
#include "doomtype.h"
#include "i_printf.h"
#include "i_system.h"
#include "m_array.h"
#include "m_misc.h"
#include "w_wad.h"
#include "w_internal.h"
#include "z_zone.h"

//
// GLOBALS
//

// Location of each lump on disk.
lumpinfo_t  *lumpinfo = NULL;
int         numlumps;         // killough
void        **lumpcache;      // killough

const char  **wadfiles;

void W_ExtractFileBase(const char *path, char *dest)
{
  const char *src;
  int length;

  src = M_BaseName(path);

  // copy up to eight characters
  memset(dest,0,8);
  length = 0;

  while (*src && *src != '.')
    if (++length == 9)
    {
      // [FG] remove length check
      I_Printf (VB_DEBUG, "Filename base of %s >8 chars",path);
      break;
    }
    else
      *dest++ = M_ToUpper(*src++);
}

//
// LUMP BASED ROUTINES.
//

void W_AddMarker(const char *name)
{
    lumpinfo_t marker = {0};
    M_CopyLumpName(marker.name, name);
    array_push(lumpinfo, marker);
    numlumps++;
}

boolean W_SkipFile(const char *filename)
{
    static const char *ext[] = { ".wad", ".zip", ".pk3", ".deh", ".exe",
                                 ".bat", ".txt" };

    for (int i = 0; i < arrlen(ext); ++i)
    {
        if (M_StringCaseEndsWith(filename, ext[i]))
        {
            return true;
        }
    }
    return false;
}

static struct
{
    const char *dir;
    const char *start_marker;
    const char *end_marker;
    namespace_t namespace;
} subdirs[] = {
    {"music",     NULL,       NULL,     ns_global   },
    {"graphics",  NULL,       NULL,     ns_global   },
    {"textures",  "TX_START", "TX_END", ns_textures },
    {"sprites",   "S_START",  "S_END",  ns_sprites  },
    {"flats",     "F_START",  "F_END",  ns_flats    },
    {"colormaps", "C_START",  "C_END",  ns_colormaps},
    {"voxels",    "VX_START", "VX_END", ns_voxels   },
};

static struct
{
    const char *dir;
    GameMode_t mode;
    GameMission_t mission;
} filters[] = {
    {"doom.id.doom1",            shareware,    doom     },
    {"doom.id.doom1.registered", registered,   doom     },
    {"doom.id.doom1.ultimate",   retail,       doom     },
    {"doom.id.doom2.commercial", commercial,   doom2    },
    {"doom.id.doom2.plutonia",   commercial,   pack_plut},
    {"doom.id.doom2.tnt",        commercial,   pack_tnt },
};

static w_module_t *modules[] =
{
    &w_zip_module,
    &w_file_module,
};

static void AddDirs(w_module_t *module, w_handle_t handle, const char *base)
{
    if (!module->AddDir(handle, base, NULL, NULL))
    {
        return;
    }

    for (int i = 0; i < arrlen(subdirs); ++i)
    {
        if (base[0] == '.')
        {
            module->AddDir(handle, subdirs[i].dir, subdirs[i].start_marker,
                           subdirs[i].end_marker);
        }
        else
        {
            char *s = M_StringJoin(base, DIR_SEPARATOR_S, subdirs[i].dir);
            module->AddDir(handle, s, subdirs[i].start_marker,
                           subdirs[i].end_marker);
            free(s);
        }
    }
}

boolean W_AddPath(const char *path)
{
    static int priority;

    w_handle_t handle = {0};
    handle.priority = priority++;

    w_module_t *active_module = NULL;

    for (int i = 0; i < arrlen(modules); ++i)
    {
        w_type_t result = modules[i]->Open(path, &handle);

        if (result == W_FILE)
        {
            return true;
        }
        else if (result == W_DIR)
        {
            active_module = modules[i];
            break;
        }
    }

    if (!active_module)
    {
        return false;
    }

    AddDirs(active_module, handle, ".");

    char *dir = NULL;

    for (int i = 0; i < arrlen(filters); ++i)
    {
        if (filters[i].mode == gamemode && filters[i].mission == gamemission)
        {
            dir = M_StringJoin("filter", DIR_SEPARATOR_S, filters[i].dir);
            break;
        }
    }

    if (!dir)
    {
        return true;
    }

    for (char *p = dir; *p; ++p)
    {
        if (*p == '.')
        {
            *p = '\0';
            AddDirs(active_module, handle, dir);
            *p = '.';
        }
    }
    AddDirs(active_module, handle, dir);

    free(dir);

    return true;
}

// jff 1/23/98 Create routines to reorder the master directory
// putting all flats into one marked block, and all sprites into another.
// This will allow loading of sprites and flats from a PWAD with no
// other changes to code, particularly fast hashes of the lumps.
//
// killough 1/24/98 modified routines to be a little faster and smaller

static int IsMarker(const char *marker, const char *name)
{
  return !strncasecmp(name, marker, 8) ||
    (*name == *marker && !strncasecmp(name+1, marker, 7));
}

// killough 4/17/98: add namespace tags

static void W_CoalesceMarkedResource(const char *start_marker,
                                     const char *end_marker, int namespace)
{
  lumpinfo_t *marked = malloc(sizeof(*marked) * numlumps);
  size_t i, num_marked = 0, num_unmarked = 0;
  int is_marked = 0, mark_end = 0;
  lumpinfo_t *lump = lumpinfo;

  for (i=numlumps; i--; lump++)
    if (IsMarker(start_marker, lump->name))       // start marker found
      { // If this is the first start marker, add start marker to marked lumps
        if (!num_marked)
          {
            M_CopyLumpName(marked->name, start_marker);
            marked->size = 0;  // killough 3/20/98: force size to be 0
            marked->namespace = ns_global;        // killough 4/17/98
            num_marked = 1;
          }
        is_marked = 1;                            // start marking lumps
      }
    else
      if (IsMarker(end_marker, lump->name))       // end marker found
        {
          mark_end = 1;                           // add end marker below
          is_marked = 0;                          // stop marking lumps
        }
      else
        if (is_marked)                            // if we are marking lumps,
          {                                       // move lump to marked list
            // sf 26/10/99:
            // ignore sprite lumps smaller than 8 bytes (the smallest possible)
            // in size -- this was used by some dmadds wads
            // as an 'empty' graphics resource
            if(namespace != ns_sprites || lump->size > 8)
            {
            marked[num_marked] = *lump;
            marked[num_marked++].namespace = namespace;  // killough 4/17/98
            }
          }
        else
          lumpinfo[num_unmarked++] = *lump;       // else move down THIS list

  // Append marked list to end of unmarked list
  memcpy(lumpinfo + num_unmarked, marked, num_marked * sizeof(*marked));

  free(marked);                                   // free marked list

  numlumps = num_unmarked + num_marked;           // new total number of lumps

  if (mark_end)                                   // add end marker
    {
      lumpinfo[numlumps].size = 0;  // killough 3/20/98: force size to be 0
      lumpinfo[numlumps].namespace = ns_global;   // killough 4/17/98
      M_CopyLumpName(lumpinfo[numlumps++].name, end_marker);
    }
}

// Hash function used for lump names.
// Must be mod'ed with table size.
// Can be used for any 8-character names.
// by Lee Killough

unsigned W_LumpNameHash(const char *s)
{
  unsigned hash;
  (void) ((hash =        M_ToUpper(s[0]), s[1]) &&
          (hash = hash*3+M_ToUpper(s[1]), s[2]) &&
          (hash = hash*2+M_ToUpper(s[2]), s[3]) &&
          (hash = hash*2+M_ToUpper(s[3]), s[4]) &&
          (hash = hash*2+M_ToUpper(s[4]), s[5]) &&
          (hash = hash*2+M_ToUpper(s[5]), s[6]) &&
          (hash = hash*2+M_ToUpper(s[6]),
           hash = hash*2+M_ToUpper(s[7]))
         );
  return hash;
}

//
// W_CheckNumForName
// Returns -1 if name not found.
//
// Rewritten by Lee Killough to use hash table for performance. Significantly
// cuts down on time -- increases Doom performance over 300%. This is the
// single most important optimization of the original Doom sources, because
// lump name lookup is used so often, and the original Doom used a sequential
// search. For large wads with > 1000 lumps this meant an average of over
// 500 were probed during every search. Now the average is under 2 probes per
// search. There is no significant benefit to packing the names into longwords
// with this new hashing algorithm, because the work to do the packing is
// just as much work as simply doing the string comparisons with the new
// algorithm, which minimizes the expected number of comparisons to under 2.
//
// killough 4/17/98: add namespace parameter to prevent collisions
// between different resources such as flats, sprites, colormaps
//

int (W_CheckNumForName)(register const char *name, register int name_space) // [FG] namespace is reserved in C++
{
  // Hash function maps the name to one of possibly numlump chains.
  // It has been tuned so that the average chain length never exceeds 2.

  register int i = lumpinfo[W_LumpNameHash(name) % (unsigned) numlumps].index;

  // We search along the chain until end, looking for case-insensitive
  // matches which also match a namespace tag. Separate hash tables are
  // not used for each namespace, because the performance benefit is not
  // worth the overhead, considering namespace collisions are rare in
  // Doom wads.

  while (i >= 0 && (strncasecmp(lumpinfo[i].name, name, 8) ||
                    lumpinfo[i].namespace != name_space))
    i = lumpinfo[i].next;

  // Return the matching lump, or -1 if none found.

  return i;
}

//
// killough 1/31/98: Initialize lump hash table
//

static void W_InitLumpHash(void)
{
  int i;

  for (i=0; i<numlumps; i++)
    lumpinfo[i].index = -1;                     // mark slots empty

  // Insert nodes to the beginning of each chain, in first-to-last
  // lump order, so that the last lump of a given name appears first
  // in any chain, observing pwad ordering rules. killough

  for (i=0; i<numlumps; i++)
    {                                           // hash function:
      int j = W_LumpNameHash(lumpinfo[i].name) % (unsigned) numlumps;
      lumpinfo[i].next = lumpinfo[j].index;     // Prepend to list
      lumpinfo[j].index = i;
    }
}

// End of lump hashing -- killough 1/31/98

//
// W_GetNumForName
// Calls W_CheckNumForName, but bombs out if not found.
//

int W_GetNumForName (const char* name)     // killough -- const added
{
  int i = W_CheckNumForName (name);
  if (i == -1)
    I_Error ("%.8s not found!", name); // killough .8 added
  return i;
}

// [Nyan] Widescreen patches
const char *W_CheckWidescreenPatch(const char *lump_main)
{
  static char lump_wide[9] = "W_";
  strncpy(&lump_wide[2], lump_main, 6);

  if (W_CheckNumForName(lump_wide) >= 0)
  {
    return lump_wide;
  }
  return lump_main;
}

//
// W_InitMultipleFiles
// Pass a null terminated list of files to use.
// All files are optional, but at least one file
//  must be found.
// Files with a .wad extension are idlink files
//  with multiple lumps.
// Other files are single lumps with the base filename
//  for the lump name.
// Lump names can appear multiple times.
// The name searcher looks backwards, so a later file
//  does override all earlier ones.
//

static w_handle_t base_handle;

boolean W_InitBaseFile(const char *path)
{
    char *filename =
        M_StringJoin(path, DIR_SEPARATOR_S, PROJECT_SHORTNAME ".pk3");

    w_type_t result = w_zip_module.Open(filename, &base_handle);

    free(filename);

    if (result == W_DIR)
    {
        AddDirs(&w_zip_module, base_handle, "all-all");
        return true;
    }

    return false;
}

void W_AddBaseDir(const char *path)
{
    AddDirs(&w_zip_module, base_handle, path);
}

void W_InitMultipleFiles(void)
{
  if (!numlumps)
    I_Error ("no files found");

  //jff 1/23/98
  // get all the sprites and flats into one marked block each
  // killough 1/24/98: change interface to use M_START/M_END explicitly
  // killough 4/4/98: add colormap markers
  // killough 4/17/98: Add namespace tags to each entry

  for (int i = 0; i < arrlen(subdirs); ++i)
  {
    if (subdirs[i].namespace != ns_global)
    {
      W_CoalesceMarkedResource(subdirs[i].start_marker, subdirs[i].end_marker,
                               subdirs[i].namespace);
    }
  }

  // [Woof!] namespace to avoid conflicts with high-resolution textures
  W_CoalesceMarkedResource("HI_START", "HI_END", ns_hires);

  // set up caching
  lumpcache = Z_Calloc(sizeof *lumpcache, numlumps, PU_STATIC, 0); // killough

  if (!lumpcache)
    I_Error ("Couldn't allocate lumpcache");

  // killough 1/31/98: initialize lump hash table
  W_InitLumpHash();
}

//
// W_LumpLength
// Returns the buffer size needed to load the given lump.
//
int W_LumpLength (int lump)
{
  if (lump >= numlumps)
    I_Error ("%i >= numlumps",lump);
  return lumpinfo[lump].size;
}

//
// W_ReadLump
// Loads the lump into the given buffer,
//  which must be >= W_LumpLength().
//

void W_ReadLumpSize(int lump, void *dest, int size)
{
    lumpinfo_t *info = lumpinfo + lump;

#ifdef RANGECHECK
    if (lump >= numlumps)
    {
        I_Error("%i >= numlumps", lump);
    }
#endif

    if (!size || !info->size)
    {
        return;
    }

    if (size < 0)
    {
        size = info->size;
    }

    if (info->data) // killough 1/31/98: predefined lump data
    {
        memcpy(dest, info->data, size);
        return;
    }

    I_BeginRead(size);

    info->module->Read(info->handle, dest, size);

    I_EndRead();
}

void W_ReadLump(int lump, void *dest)
{
    W_ReadLumpSize(lump, dest, -1);
}

//
// W_CacheLumpNum
//
// killough 4/25/98: simplified

void *W_CacheLumpNum(int lump, pu_tag tag)
{
#ifdef RANGECHECK
  if ((unsigned)lump >= numlumps)
    I_Error ("%i >= numlumps",lump);
#endif

  if (!lumpcache[lump])      // read the lump in
    W_ReadLump(lump, Z_Malloc(W_LumpLength(lump), tag, &lumpcache[lump]));
  else
    Z_ChangeTag(lumpcache[lump],tag);

  return lumpcache[lump];
}

// W_CacheLumpName macroized in w_wad.h -- killough

// [FG] name of the WAD file that contains the lump
const char *W_WadNameForLump (const int lump)
{
  if (lump < 0 || lump >= numlumps)
    return "invalid";
  else
  {
    const char *wad_file = lumpinfo[lump].wad_file;

    if (wad_file)
      return M_BaseName(wad_file);
    else
      return "lump";
  }
}

boolean W_IsIWADLump (const int lump)
{
	return lump >= 0 && lump < numlumps &&
	       lumpinfo[lump].wad_file == wadfiles[0];
}

// check if lump is from WAD
boolean W_IsWADLump (const int lump)
{
	return lump >= 0 && lump < numlumps && lumpinfo[lump].wad_file;
}

boolean W_LumpExistsWithName(int lump, char *name)
{
  if (lump < 0 || lump >= numlumps)
    return false;

  if (name && strncasecmp(lumpinfo[lump].name, name, 8))
    return false;

  return true;
}

int W_LumpLengthWithName(int lump, char *name)
{
  if (!W_LumpExistsWithName(lump, name))
    return 0;

  return W_LumpLength(lump);
}

// killough 10/98: support .deh from wads
//
// A lump named DEHACKED is treated as plaintext of a .deh file embedded in
// a wad (more portable than reading/writing info.c data directly in a wad).
//
// If there are multiple instances of "DEHACKED", we process each, in first
// to last order (we must reverse the order since they will be stored in
// last to first order in the chain). Passing NULL as first argument to
// ProcessDehFile() indicates that the data comes from the lump number
// indicated by the third argument, instead of from a file.

static void ProcessInWad(int i, const char *name, void (*process)(int lumpnum),
                         process_wad_t flag)
{
    if (i >= 0)
    {
        ProcessInWad(lumpinfo[i].next, name, process, flag);

        int condition = 0;
        if (flag & PROCESS_IWAD)
        {
            condition |= lumpinfo[i].wad_file == wadfiles[0];
        }
        if (flag & PROCESS_PWAD)
        {
            condition |= lumpinfo[i].wad_file != wadfiles[0];
        }

        if (!strncasecmp(lumpinfo[i].name, name, 8)
            && lumpinfo[i].namespace == ns_global && condition)
        {
            process(i);
        }
    }
}

void W_ProcessInWads(const char *name, void (*process)(int lumpnum),
                     process_wad_t flags)
{
    ProcessInWad(lumpinfo[W_LumpNameHash(name) % (unsigned)numlumps].index,
                 name, process, flags);
}

void W_Close(void)
{
    for (int i = 0; i < arrlen(modules); ++i)
    {
        modules[i]->Close();
    }
}

//----------------------------------------------------------------------------
//
// $Log: w_wad.c,v $
// Revision 1.20  1998/05/06  11:32:00  jim
// Moved predefined lump writer info->w_wad
//
// Revision 1.19  1998/05/03  22:43:09  killough
// beautification, header #includes
//
// Revision 1.18  1998/05/01  14:53:59  killough
// beautification
//
// Revision 1.17  1998/04/27  02:06:41  killough
// Program beautification
//
// Revision 1.16  1998/04/17  10:34:53  killough
// Tag lumps with namespace tags to resolve collisions
//
// Revision 1.15  1998/04/06  04:43:59  killough
// Add C_START/C_END support, remove non-standard C code
//
// Revision 1.14  1998/03/23  03:42:59  killough
// Fix drive-letter bug and force marker lumps to 0-size
//
// Revision 1.12  1998/02/23  04:59:18  killough
// Move TRANMAP init code to r_data.c
//
// Revision 1.11  1998/02/20  23:32:30  phares
// Added external tranmap
//
// Revision 1.10  1998/02/20  22:53:25  phares
// Moved TRANMAP initialization to w_wad.c
//
// Revision 1.9  1998/02/17  06:25:07  killough
// Make numlumps static add #ifdef RANGECHECK for perf
//
// Revision 1.8  1998/02/09  03:20:16  killough
// Fix garbage printed in lump error message
//
// Revision 1.7  1998/02/02  13:21:04  killough
// improve hashing, add predef lumps, fix err handling
//
// Revision 1.6  1998/01/26  19:25:10  phares
// First rev with no ^Ms
//
// Revision 1.5  1998/01/26  06:30:50  killough
// Rewrite merge routine to use simpler, robust algorithm
//
// Revision 1.3  1998/01/23  20:28:11  jim
// Basic sprite/flat functionality in PWAD added
//
// Revision 1.2  1998/01/22  05:55:58  killough
// Improve hashing algorithm
//
//----------------------------------------------------------------------------
