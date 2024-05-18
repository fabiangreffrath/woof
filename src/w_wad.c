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

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

#include "doomtype.h"
#include "i_glob.h"
#include "i_printf.h"
#include "i_system.h"
#include "i_zip.h"
#include "m_array.h"
#include "m_io.h"
#include "m_misc.h"
#include "m_swap.h"
#include "w_wad.h"
#include "z_zone.h"

//
// GLOBALS
//

// Location of each lump on disk.
lumpinfo_t  *lumpinfo = NULL;
int         numlumps;         // killough
static void **lumpcache;      // killough

const char  **wadfiles;

static int W_FileLength(int handle)
{
   struct stat fileinfo;
   if(fstat(handle,&fileinfo) == -1)
      I_Error("W_FileLength: failure in fstat\n");
   return fileinfo.st_size;
}

void ExtractFileBase(const char *path, char *dest)
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

//
// W_AddFile
// All files are optional, but at least one file must be
//  found (PWAD, if all required lumps are present).
// Files with a .wad extension are wadlink files
//  with multiple lumps.
// Other files are single lumps with the base filename
//  for the lump name.
//
// Reload hack removed by Lee Killough
//

static int *handles = NULL;

static void AddFile(const char *path) // killough 1/31/98: static, const
{
    wadinfo_t header;
    filelump_t *fileinfo;
    int handle;
    int length;
    int startlump;

    // open the file and add to directory

    handle = M_open(path, O_RDONLY | O_BINARY);

    if (handle == -1)
    {
        if (M_StringCaseEndsWith(path, ".lmp"))
        {
            return;
        }
        I_Error("Error: couldn't open %s", path); // killough
    }

    I_Printf(VB_INFO, " adding %s", path); // killough 8/8/98

    startlump = numlumps;

    boolean is_single = false;

    // killough:
    if (!M_StringCaseEndsWith(path, ".wad"))
    {
        // single lump file
        fileinfo = calloc(1, sizeof(*fileinfo));
        fileinfo[0].size = LONG(W_FileLength(handle));
        ExtractFileBase(path, fileinfo[0].name);
        numlumps++;
        is_single = true;
    }
    else
    {
        // WAD file
        if (read(handle, &header, sizeof(header)) == 0)
        {
            I_Error("Error reading header from %s (%s)", path, strerror(errno));
        }

        if (strncmp(header.identification, "IWAD", 4)
            && strncmp(header.identification, "PWAD", 4))
        {
            I_Error("Wad file %s doesn't have IWAD or PWAD id", path);
        }

        header.numlumps = LONG(header.numlumps);
        if (header.numlumps == 0)
        {
            I_Printf(VB_WARNING, "Wad file %s is empty", path);
            close(handle);
            return;
        }

        length = header.numlumps * sizeof(filelump_t);
        fileinfo = malloc(length);
        if (fileinfo == NULL)
        {
            I_Error("Failed to allocate file table from %s", path);
        }

        header.infotableofs = LONG(header.infotableofs);
        if (lseek(handle, header.infotableofs, SEEK_SET) == -1)
        {
            I_Printf(VB_WARNING, "Error seeking offset from %s (%s)", path,
                     strerror(errno));
            close(handle);
            free(fileinfo);
            return;
        }

        if (read(handle, fileinfo, length) == 0)
        {
            I_Printf(VB_WARNING, "Error reading lump directory from %s (%s)",
                     path, strerror(errno));
            close(handle);
            free(fileinfo);
            return;
        }

        numlumps += header.numlumps;
    }

    array_push(handles, handle);

    const char *wadname = M_StringDuplicate(M_BaseName(path));
    array_push(wadfiles, wadname);

    // Fill in lumpinfo
    array_grow(lumpinfo, numlumps);

    for (int i = 0; i < numlumps - startlump; i++)
    {
        lumpinfo_t item = {0};
        M_CopyLumpName(item.name, fileinfo[i].name);
        item.size = LONG(fileinfo[i].size);
        item.namespace = ns_global; // killough 4/17/98
        item.handle = handle; //  killough 4/25/98
        item.position = LONG(fileinfo[i].filepos);

        // [FG] WAD file that contains the lump
        item.wad_file = (is_single ? NULL : wadname);
        array_push(lumpinfo, item);
    }

    free(fileinfo);
}

static void AddWadInZip(void *handle, const char *name, int index,
                        size_t data_size)
{
    byte *data = malloc(data_size);

    I_ZIP_ReadFile(handle, index, data, data_size);

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
    if (header.numlumps == 0)
    {
        I_Printf(VB_WARNING, "Wad file %s is empty", name);
        free(data);
        return;
    }

    header.infotableofs = LONG(header.infotableofs);
    if (header.infotableofs + header.numlumps * sizeof(filelump_t) > data_size)
    {
        I_Printf(VB_WARNING, "Error seeking offset from %s", name);
        free(data);
        return;
    }

    filelump_t *fileinfo = (filelump_t *)(data + header.infotableofs);

    const char *wadname = M_StringDuplicate(name);
    array_push(wadfiles, wadname);

    numlumps += header.numlumps;

    array_grow(lumpinfo, numlumps);

    for (int i = 0; i < header.numlumps; i++)
    {
        lumpinfo_t item = {0};
        M_CopyLumpName(item.name, fileinfo[i].name);
        int size = LONG(fileinfo[i].size);
        int position = LONG(fileinfo[i].filepos);
        if (position + size > data_size)
        {
            I_Error("Error reading lump %d from %s", i, wadname);
        }
        item.size = size;
        item.data = data + position;
        item.namespace = ns_global;
 
        // [FG] WAD file that contains the lump
        item.wad_file = wadname;
        array_push(lumpinfo, item);
    }
}

static void AddMarker(const char *name)
{
    lumpinfo_t marker = {0};
    M_CopyLumpName(marker.name, name);
    array_push(lumpinfo, marker);
    numlumps++;
}

static void AddDir(glob_t *glob, const char *start_marker,
                      const char *end_marker)
{
    int startlump = numlumps;

    while (true)
    {
        const char *filename = I_NextGlob(glob);
        if (filename == NULL)
        {
            break;
        }

        if (startlump == numlumps && start_marker)
        {
            AddMarker(start_marker);
        }

        int handle = M_open(filename, O_RDONLY | O_BINARY);
        if (handle == -1)
        {
            I_Error("Error: couldn't open %s", filename);
        }

        lumpinfo_t item = {0};
        item.handle = handle;
        item.size = LONG(W_FileLength(handle));
        ExtractFileBase(filename, item.name);
        item.namespace = ns_global;
        array_push(lumpinfo, item);
        numlumps++;

        array_push(handles, handle);
    }

    if (numlumps > startlump && end_marker)
    {
        AddMarker(end_marker);
    }
}

static void AddZipDir(glob_zip_t *glob, const char *start_marker,
                      const char *end_marker)
{
    int startlump = numlumps;

    while (true)
    {
        const char *filename = I_ZIP_NextGlob(glob);
        if (filename == NULL)
        {
            break;
        }

        if (startlump == numlumps && start_marker)
        {
            AddMarker(start_marker);
        }

        lumpinfo_t item = {0};
        item.handle = I_ZIP_GetIndex(glob);
        item.size = I_ZIP_GetSize(glob);
        ExtractFileBase(filename, item.name);
        item.zip = I_ZIP_GetHandle(glob);
        item.namespace = ns_global;
        array_push(lumpinfo, item);
        numlumps++;
    }

    if (numlumps > startlump && end_marker)
    {
        AddMarker(end_marker);
    }
}

static boolean AddMultipleZipDirs(const char *path)
{
    void *handle = I_ZIP_Open(path);
    if (!handle)
    {
        return false;
    }

    I_Printf(VB_INFO, " adding %s", path);

    glob_zip_t *glob = I_ZIP_StartMultiGlob(
        handle, NULL, GLOB_FLAG_NOCASE | GLOB_FLAG_SORTED, "*.wad", NULL);
    while (true)
    {
        const char *wadname = I_ZIP_NextGlob(glob);
        if (wadname == NULL)
        {
            break;
        }
        AddWadInZip(handle, wadname, I_ZIP_GetIndex(glob), I_ZIP_GetSize(glob));
    }
    I_ZIP_EndGlob(glob);

    glob = I_ZIP_StartMultiGlob(
        handle, NULL, GLOB_FLAG_NOCASE | GLOB_FLAG_SORTED, "*.lmp", "*.wav",
        "*.ogg", "*.flac", "*.mp3", "*.mid", NULL);
    AddZipDir(glob, NULL, NULL);
    I_ZIP_EndGlob(glob);

    glob = I_ZIP_StartMultiGlob(
        handle, "music", GLOB_FLAG_NOCASE | GLOB_FLAG_SORTED, "*.wav", "*.ogg",
        "*.flac", "*.mp3", "*.mid", NULL);
    AddZipDir(glob, NULL, NULL);
    I_ZIP_EndGlob(glob);

    glob = I_ZIP_StartMultiGlob(
        handle, "sprites", GLOB_FLAG_NOCASE | GLOB_FLAG_SORTED, "*.lmp", NULL);
    AddZipDir(glob, "S_START", "S_END");
    I_ZIP_EndGlob(glob);

    glob = I_ZIP_StartMultiGlob(
        handle, "colormaps", GLOB_FLAG_NOCASE | GLOB_FLAG_SORTED, "*.lmp",
        "*.cmp", NULL);
    AddZipDir(glob, "C_START", "C_END");
    I_ZIP_EndGlob(glob);

    glob = I_ZIP_StartMultiGlob(
        handle, "voxels", GLOB_FLAG_NOCASE | GLOB_FLAG_SORTED, "*.kvx", NULL);
    AddZipDir(glob, "VX_START", "VX_END");
    I_ZIP_EndGlob(glob);

    return true;
}

static void AddMultipleDirs(const char *path)
{
    glob_t *glob =
        I_StartGlob(path, "*.wad", GLOB_FLAG_NOCASE | GLOB_FLAG_SORTED);
    while (true)
    {
        const char *filepath = I_NextGlob(glob);
        if (filepath == NULL)
        {
            break;
        }
        AddFile(filepath);
    }
    I_EndGlob(glob);

    glob = I_StartMultiGlob(path, GLOB_FLAG_NOCASE | GLOB_FLAG_SORTED, "*.zip",
                            "*.pk3", NULL);
    while (true)
    {
        const char *filepath = I_NextGlob(glob);
        if (filepath == NULL)
        {
            break;
        }
        AddMultipleZipDirs(filepath);
    }
    I_EndGlob(glob);

    glob = I_StartMultiGlob(path, GLOB_FLAG_NOCASE | GLOB_FLAG_SORTED, "*.lmp",
                            "*.wav", "*.ogg", "*.flac", "*.mp3", "*.mid", NULL);
    AddDir(glob, NULL, NULL);
    I_EndGlob(glob);

    char *s = M_StringJoin(path, DIR_SEPARATOR_S, "music", NULL);
    if (M_DirExists(s))
    {
        glob = I_StartMultiGlob(s, GLOB_FLAG_NOCASE | GLOB_FLAG_SORTED,
                                "*.wav", "*.ogg", "*.flac", "*.mp3", "*.mid", NULL);
        AddDir(glob, NULL, NULL);
        I_EndGlob(glob);
    }

    s = M_StringJoin(path, DIR_SEPARATOR_S, "sprites", NULL);
    if (M_DirExists(s))
    {
        glob = I_StartGlob(s, "*.lmp", GLOB_FLAG_NOCASE | GLOB_FLAG_SORTED);
        AddDir(glob, "S_START", "S_END");
        I_EndGlob(glob);
    }
    free(s);
    s = M_StringJoin(path, DIR_SEPARATOR_S, "colormaps", NULL);
    if (M_DirExists(s))
    {
        glob = I_StartMultiGlob(s, GLOB_FLAG_NOCASE | GLOB_FLAG_SORTED, "*.lmp",
                                "*.cmp", NULL);
        AddDir(glob, "C_START", "C_END");
        I_EndGlob(glob);
    }
    free(s);

    s = M_StringJoin(path, DIR_SEPARATOR_S, "voxels", NULL);
    if (M_DirExists(s))
    {
        glob = I_StartMultiGlob(s, GLOB_FLAG_NOCASE | GLOB_FLAG_SORTED, "*.kvx",
                                NULL);
        AddDir(glob, "VX_START", "VX_END");
        I_EndGlob(glob);
    }
    free(s);
}

void W_AddPath(const char *path)
{
    if (M_DirExists(path))
    {
        AddMultipleDirs(path);
    }
    else
    {
        if (AddMultipleZipDirs(path))
        {
            return;
        }
        AddFile(path);
    }
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
    I_Error ("W_GetNumForName: %.8s not found!", name); // killough .8 added
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

void W_InitPredefineLumps(void)
{
  // killough 1/31/98: add predefined lumps first

  numlumps = num_predefined_lumps;

  // lumpinfo will be realloced as lumps are added
  array_grow(lumpinfo, numlumps);

  memcpy(lumpinfo, predefined_lumps, numlumps*sizeof(*lumpinfo));

  array_ptr(lumpinfo)->size += numlumps;
}

void W_InitMultipleFiles(void)
{
  if (!numlumps)
    I_Error ("W_InitFiles: no files found");

  //jff 1/23/98
  // get all the sprites and flats into one marked block each
  // killough 1/24/98: change interface to use M_START/M_END explicitly
  // killough 4/17/98: Add namespace tags to each entry

  W_CoalesceMarkedResource("S_START", "S_END", ns_sprites);
  W_CoalesceMarkedResource("F_START", "F_END", ns_flats);

  // killough 4/4/98: add colormap markers
  W_CoalesceMarkedResource("C_START", "C_END", ns_colormaps);

  W_CoalesceMarkedResource("VX_START", "VX_END", ns_voxels);

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
    I_Error ("W_LumpLength: %i >= numlumps",lump);
  return lumpinfo[lump].size;
}

//
// W_ReadLump
// Loads the lump into the given buffer,
//  which must be >= W_LumpLength().
//

void W_ReadLump(int lump, void *dest)
{
    lumpinfo_t *l = lumpinfo + lump;

#ifdef RANGECHECK
    if (lump >= numlumps)
    {
        I_Error("W_ReadLump: %i >= numlumps", lump);
    }
#endif

    if (l->data) // killough 1/31/98: predefined lump data
    {
        memcpy(dest, l->data, l->size);
        return;
    }

    if (!l->size)
    {
        return;
    }

    I_BeginRead(l->size);

    if (l->zip)
    {
        I_ZIP_ReadFile(l->zip, l->handle, dest, l->size);
    }
    else
    {
        int c;

        // killough 1/31/98: Reload hack (-wart) removed
        // killough 10/98: Add flashing disk indicator

        lseek(l->handle, l->position, SEEK_SET);
        c = read(l->handle, dest, l->size);
        if (c < l->size)
        {
            I_Error("W_ReadLump: only read %i of %i on lump %i", c, l->size,
                    lump);
        }
    }

    I_EndRead();
}

//
// W_CacheLumpNum
//
// killough 4/25/98: simplified

void *W_CacheLumpNum(int lump, pu_tag tag)
{
#ifdef RANGECHECK
  if ((unsigned)lump >= numlumps)
    I_Error ("W_CacheLumpNum: %i >= numlumps",lump);
#endif

  if (!lumpcache[lump])      // read the lump in
    W_ReadLump(lump, Z_Malloc(W_LumpLength(lump), tag, &lumpcache[lump]));
  else
    Z_ChangeTag(lumpcache[lump],tag);

  return lumpcache[lump];
}

// W_CacheLumpName macroized in w_wad.h -- killough

// WritePredefinedLumpWad
// Args: Filename - string with filename to write to
// Returns: void
//
// If the user puts a -dumplumps switch on the command line, we will
// write all those predefined lumps above out into a pwad.  User
// supplies the pwad name.
//
// killough 4/22/98: make endian-independent, remove tab chars
// haleyjd 01/21/05: rewritten to use stdio
//
void WriteLumpWad(const char *filename, const lumpinfo_t *lumps, const size_t num_lumps)
{
   FILE *file;
   char *fn;
   
   if(!filename || !*filename)  // check for null pointer or empty name
      return;  // early return

   fn = malloc(strlen(filename) + 5);  // we may have to add ".wad" to the name they pass

   AddDefaultExtension(strcpy(fn, filename), ".wad");

   // The following code writes a PWAD from the predefined lumps array
   // How to write a PWAD will not be explained here.
   if((file = M_fopen(fn, "wb")))
   {
      wadinfo_t header = { "PWAD" };
      size_t filepos = sizeof(wadinfo_t) + num_lumps * sizeof(filelump_t);
      int i;
      
      header.numlumps     = LONG(num_lumps);
      header.infotableofs = LONG(sizeof(header));
      
      // write header
      fwrite(&header, 1, sizeof(header), file);
      
      // write directory
      for(i = 0; i < num_lumps; i++)
      {
         filelump_t fileinfo = { 0 };
         
         fileinfo.filepos = LONG(filepos);
         fileinfo.size    = LONG(lumps[i].size);
         M_CopyLumpName(fileinfo.name, lumps[i].name);
         
         fwrite(&fileinfo, 1, sizeof(fileinfo), file);

         filepos += lumps[i].size;
      }
      
      // write lumps
      for(i = 0; i < num_lumps; i++)
         fwrite(lumps[i].data, 1, lumps[i].size, file);
      
      fclose(file);
      I_Success("Internal lumps wad, %s written, exiting\n", filename);
   }
   I_Error("Cannot open internal lumps wad %s for output\n", filename);
   free(fn);
}

void WritePredefinedLumpWad(const char *filename)
{
    WriteLumpWad(filename, predefined_lumps, num_predefined_lumps);
}

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

// [FG] avoid demo lump name collisions
void W_DemoLumpNameCollision(char **name)
{
  const char *const safename = "DEMO1";
  char basename[9];
  int i, lump;

  ExtractFileBase(*name, basename);

  // [FG] lumps called DEMO* are considered safe
  if (!strncasecmp(basename, safename, 4))
  {
    return;
  }

  lump = W_CheckNumForName(basename);

  if (lump >= 0)
  {
    for (i = lump - 1; i >= 0; i--)
    {
      if (!strncasecmp(lumpinfo[i].name, basename, 8))
      {
        break;
      }
    }

    if (i >= 0)
    {
      I_Printf(VB_WARNING, "Demo lump name collision detected with lump \'%.8s\' from %s.",
              lumpinfo[i].name, W_WadNameForLump(i));

      // [FG] the DEMO1 lump is almost certainly always a demo lump
      M_StringCopy(lumpinfo[lump].name, safename, 8);
      *name = lumpinfo[lump].name;

      W_InitLumpHash();
    }
  }
}

void W_CloseFileDescriptors(void)
{
    for (int i = 0; i < array_size(handles); ++i)
    {
       close(handles[i]);
    }

    I_ZIP_CloseFiles();
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

