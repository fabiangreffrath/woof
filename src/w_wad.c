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
#include "m_hashmap.h"
#include "m_misc.h"
#include "m_swap.h"
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

void W_ConvertSlashes(char *path)
{
    for (char *p = path; *p; ++p)
    {
        if (*p == '\\')
        {
            *p = '/';
        }
    }
}

void W_ConvertSpriteName(char *name)
{
    for (int i = 0; i < 8; ++i)
    {
        if (name[i] == '^')
        {
            name[i] = '\\';
        }
    }
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

// [ZDoom PK3] map of full path names (e.g. "music/d_runnin.ogg") to
// lump indices, for files loaded from a PK3 archive or a folder.
static hashmap_t *longname_map;

static void HashLongName(const int lumpnum)
{
    if (!longname_map)
    {
        longname_map = hashmap_init_str(64, sizeof(int));
    }

    char *key = M_StringDuplicate(lumpinfo[lumpnum].longname);
    M_StringToLower(key);

    hashmap_put_str(longname_map, key, &lumpnum);

    free(key);
}

int W_CheckNumForLongName(const char *name)
{
    if (!longname_map)
    {
        return -1;
    }

    char *key = M_StringDuplicate(name);
    M_StringToLower(key);

    int *lumpnum = hashmap_get_str(longname_map, key);

    free(key);
    return lumpnum ? *lumpnum : -1;
}

// Map data lumps that may follow a level marker lump.
static boolean IsMapLumpName(const char *name)
{
    static const char *const names[] = {
        "TEXTMAP",  "THINGS",   "LINEDEFS", "SIDEDEFS", "VERTEXES",
        "SEGS",     "SSECTORS", "NODES",    "SECTORS",  "REJECT",
        "BLOCKMAP", "BEHAVIOR", "ZNODES",   "ENDMAP"
    };

    for (int i = 0; i < arrlen(names); ++i)
    {
        if (!strcasecmp(name, names[i]))
        {
            return true;
        }
    }
    return false;
}

void W_AddWadFromMemory(const char *name, const void *data, size_t data_size,
                        const char *map_name)
{
    wadinfo_t header;

    if (sizeof(header) > data_size)
    {
        I_Error("Error reading header from %s", name);
    }

    memcpy(&header, data, sizeof(header));

    if (strncmp(header.identification, "IWAD", 4)
        && strncmp(header.identification, "PWAD", 4))
    {
        I_Error("Wad file %s doesn't have IWAD or PWAD id", name);
    }

    header.numlumps = LONG(header.numlumps);
    header.infotableofs = LONG(header.infotableofs);

    if (header.numlumps == 0)
    {
        I_Printf(VB_WARNING, "Wad file %s is empty", name);
        return;
    }

    if (header.infotableofs + header.numlumps * sizeof(filelump_t) > data_size)
    {
        I_Printf(VB_WARNING, "Error seeking offset from %s", name);
        return;
    }

    filelump_t *fileinfo = (filelump_t *)((const byte *)data
                                         + header.infotableofs);

    const char *wadname = M_StringDuplicate(name);

    I_Printf(VB_INFO, " - adding %s", name);

    if (map_name)
    {
        // [ZDoom PK3] WADs under maps/ hold the data of one single
        // level, and the file name determines the map name. Add the level
        // marker lump renamed after the file, the lumps of the first level,
        // and skip any other data.
        boolean in_map = false;

        for (int i = 0; i < header.numlumps; ++i)
        {
            char lumpname[9] = {0};
            memcpy(lumpname, fileinfo[i].name, 8);

            boolean is_map_lump = IsMapLumpName(lumpname);

            if (!is_map_lump && in_map)
            {
                break; // end of the first level, ignore the rest
            }

            if (!in_map)
            {
                // Add the level marker lump, named after the WAD file.
                in_map = true;

                lumpinfo_t marker = {0};
                M_CopyLumpName(marker.name, map_name);
                marker.size = 0;
                marker.wad_file = wadname;
                array_push(lumpinfo, marker);
                numlumps++;

                if (!is_map_lump)
                {
                    continue; // skip the marker lump's own data
                }
            }

            lumpinfo_t item = {0};
            M_CopyLumpName(item.name, fileinfo[i].name);
            int size = LONG(fileinfo[i].size);
            int position = LONG(fileinfo[i].filepos);
            if (position < 0 || size < 0
                || position + size > (int64_t)data_size)
            {
                I_Error("Error reading lump %d from %s", i, wadname);
            }
            item.size = size;
            item.data = (const byte *)data + position;
            item.wad_file = wadname;
            array_push(lumpinfo, item);
            numlumps++;
        }
    }
    else
    {
        array_push(wadfiles, wadname);

        numlumps += header.numlumps;

        for (int i = 0; i < header.numlumps; ++i)
        {
            lumpinfo_t item = {0};
            M_CopyLumpName(item.name, fileinfo[i].name);
            int size = LONG(fileinfo[i].size);
            int position = LONG(fileinfo[i].filepos);
            if (position < 0 || size < 0
                || position + size > (int64_t)data_size)
            {
                I_Error("Error reading lump %d from %s", i, wadname);
            }
            item.size = size;
            item.data = (const byte *)data + position;
            item.wad_file = wadname;
            array_push(lumpinfo, item);
        }
    }
}

// [ZDoom PK3] sub-directories of a PK3 archive or folder that assign
// files to the WAD namespaces.
// https://zdoom.org/wiki/Using_ZIPs_as_WAD_replacement
static const w_dir_spec_t subdirs[] =
{
    // [Woof!] decorations, normally found between AC_START and AC_END
    {"actors",    "AC_START",  "AC_END",   ns_actors,    false},
    {"colormaps", "C_START",   "C_END",    ns_colormaps, false},
    // [Boom] flats, normally found between F_START and F_END
    {"flats",     "F_START",   "F_END",    ns_flats,     false},
    {"graphics",  NULL,        NULL,       ns_global,    false},
    {"hires",     "HI_START",  "HI_END",   ns_hires,     false},
    {"maps",      NULL,        NULL,       ns_global,    true },
    {"music",     NULL,        NULL,       ns_global,    false},
    {"patches",   NULL,        NULL,       ns_global,    false},
    {"sounds",    NULL,        NULL,       ns_global,    false},
    {"sprites",   "S_START",   "S_END",    ns_sprites,   false},
    {"textures",  "TX_START",  "TX_END",   ns_textures,  false},
    {"voxels",    "VX_START",  "VX_END",   ns_voxels,    false},
};

// [ZDoom PK3] The file list of an archive or folder is sorted, so all
// files of one directory are consecutive. Loading a base directory is a
// single pass that assigns files to the reserved directories on the fly.

const w_dir_spec_t *W_LookupDirSpec(const char *name)
{
    const char *sep = strchr(name, '/');
    size_t length = sep ? (size_t)(sep - name) : strlen(name);

    for (int i = 0; i < arrlen(subdirs); ++i)
    {
        if (strlen(subdirs[i].dir) == length
            && !strncasecmp(name, subdirs[i].dir, length))
        {
            return &subdirs[i];
        }
    }
    return NULL;
}

static const w_dir_spec_t *cur_spec; // reserved directory of the current run
static boolean run_lumps;            // lumps added to the current run

static void CloseDirRun(void)
{
    if (cur_spec && run_lumps && cur_spec->end_marker)
    {
        W_AddMarker(cur_spec->end_marker);
    }
    cur_spec = NULL;
    run_lumps = false;
}

const w_dir_spec_t *W_DirSpecOfFile(const char *relpath)
{
    const w_dir_spec_t *spec =
        strchr(relpath, '/') ? W_LookupDirSpec(relpath) : NULL;

    if (spec != cur_spec)
    {
        CloseDirRun();
        cur_spec = spec;
    }

    return spec;
}

void W_BeginDirLump(const w_dir_spec_t *spec)
{
    if (spec && spec->start_marker && !run_lumps)
    {
        W_AddMarker(spec->start_marker);
    }
    run_lumps = true;
}

void W_FlushDirRun(void)
{
    CloseDirRun();
}

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
    {"chex.chex1",               retail,       pack_chex},
    {"rekkr",                    retail,       pack_rekkr}
};

static w_module_t *modules[] =
{
    &w_zip_module,
    &w_file_module,
};

static void AddDirs(w_module_t *module, w_handle_t handle, const char *base)
{
    module->AddDir(handle, base);
}

static void Filter(w_module_t *module, w_handle_t handle)
{
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
        return;
    }

    for (char *p = dir; *p; ++p)
    {
        if (*p == '.')
        {
            *p = '\0';
            AddDirs(module, handle, dir);
            *p = '.';
        }
    }
    AddDirs(module, handle, dir);

    free(dir);
}

static void FilterAutoload(w_module_t *module, w_handle_t handle)
{
    if (gamemission < pack_chex)
    {
        AddDirs(module, handle, "filter" DIR_SEPARATOR_S "doom-all");
    }
    if (gamemission == pack_chex || gamemission == pack_chex3v)
    {
        AddDirs(module, handle, "filter" DIR_SEPARATOR_S "chex-all");
    }
    if (gamemission == doom)
    {
        AddDirs(module, handle, "filter" DIR_SEPARATOR_S "doom1-all");
    }
    else if (gamemission >= doom2 && gamemission <= pack_plut)
    {
        AddDirs(module, handle, "filter" DIR_SEPARATOR_S "doom2-all");
    }
    else if (gamemission == pack_freedoom)
    {
        AddDirs(module, handle, "filter" DIR_SEPARATOR_S "freedoom-all");
        if (gamemode == commercial)
        {
            AddDirs(module, handle, "filter" DIR_SEPARATOR_S "freedoom2-all");
        }
        else
        {
            AddDirs(module, handle, "filter" DIR_SEPARATOR_S "freedoom1-all");
        }
    }
    else if (gamemission == pack_rekkr)
    {
        AddDirs(module, handle, "filter" DIR_SEPARATOR_S "rekkr-all");
    }

    for (int i = 0; i < array_size(wadfiles); ++i)
    {
        char *dir = M_StringJoin("filter", DIR_SEPARATOR_S, M_BaseName(wadfiles[i]));
        AddDirs(module, handle, dir);
        free(dir);
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

    Filter(active_module, handle);

    FilterAutoload(active_module, handle);

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
  lumpinfo_t *marked = calloc(numlumps, sizeof(*marked));
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
      // Zero the whole lump: the stale slot it reuses may hold garbage
      // (e.g. a longname pointer from an earlier lump), which would
      // otherwise be picked up by the longname map.
      lumpinfo_t marker = {0};
      marker.namespace = ns_global;   // killough 4/17/98
      M_CopyLumpName(marker.name, end_marker);
      lumpinfo[numlumps++] = marker;
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
        AddDirs(&w_zip_module, base_handle, ".");
        FilterAutoload(&w_zip_module, base_handle);
        return true;
    }

    return false;
}

void W_AddBaseDir(const char *path)
{
    char *base = M_StringJoin("filter", DIR_SEPARATOR_S, path);
    AddDirs(&w_zip_module, base_handle, base);
    free(base);
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

  // set up caching
  lumpcache = Z_Calloc(numlumps, sizeof(*lumpcache), PU_STATIC, 0); // killough

  if (!lumpcache)
    I_Error ("Couldn't allocate lumpcache");

  // killough 1/31/98: initialize lump hash table
  W_InitLumpHash();

  for (int i = 0; i < numlumps; ++i)
  {
    if (lumpinfo[i].longname)
    {
        HashLongName(i);
    }
  }
}

//
// W_LumpLength
// Returns the buffer size needed to load the given lump.
//
static inline int LumpLength(int lump)
{
  return lumpinfo[lump].size;
}

int W_LumpLength (int lump)
{
#ifdef RANGECHECK
  if (lump >= numlumps)
    I_Error ("%i >= numlumps",lump);
#endif

  return LumpLength(lump);
}

//
// W_ReadLump
// Loads the lump into the given buffer,
//  which must be >= W_LumpLength().
//

static inline void ReadLumpSize(int lump, void *dest, int size)
{
    lumpinfo_t *info = lumpinfo + lump;

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

void W_ReadLumpSize(int lump, void *dest, int size)
{
#ifdef RANGECHECK
    if (lump >= numlumps)
    {
        I_Error("%i >= numlumps", lump);
    }
#endif

    ReadLumpSize(lump, dest, size);
}

static void ReadLump(int lump, void *dest)
{
    ReadLumpSize(lump, dest, -1);
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
    ReadLump(lump, Z_Malloc(LumpLength(lump), tag, &lumpcache[lump]));
  else
    Z_ChangeTag(lumpcache[lump],tag);

  return lumpcache[lump];
}

// W_CacheLumpName macroized in w_wad.h -- killough

// [FG] name of the WAD file that contains the lump
const char *W_WadNameForLump(const int lump)
{
    if (!W_LumpExists(lump))
    {
        return "invalid";
    }
    else
    {
        const char *wad_file = lumpinfo[lump].wad_file;

        if (wad_file)
        {
            return M_BaseName(wad_file);
        }
        else
        {
            return "lump";
        }
    }
}

boolean W_LumpExists(const int lump)
{
    return 0 <= lump && lump < numlumps;
}

boolean W_IsIWADLump(const int lump)
{
    return W_LumpExists(lump) && lumpinfo[lump].wad_file == wadfiles[0];
}

// check if lump is from WAD
boolean W_IsWADLump(const int lump)
{
    return W_LumpExists(lump) && lumpinfo[lump].wad_file;
}

boolean W_LumpExistsWithName(int lump, char *name)
{
    if (!W_LumpExists(lump))
    {
        return false;
    }

    if (name && strncasecmp(lumpinfo[lump].name, name, 8))
    {
        return false;
    }

    return true;
}

int W_LumpLengthWithName(int lump, char *name)
{
    if (!W_LumpExistsWithName(lump, name))
    {
        return 0;
    }

    return LumpLength(lump);
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
