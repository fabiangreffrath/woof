//
// Copyright(C) 2024 Roman Fomin
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

#include <fcntl.h>
#include <errno.h>

#include "doomtype.h"
#include "i_glob.h"
#include "i_printf.h"
#include "i_system.h"
#include "m_io.h"
#include "m_array.h"
#include "m_misc.h"
#include "m_swap.h"
#include "w_internal.h"
#include "w_wad.h"

static int FileLength(int descriptor)
{
   struct stat st;

   if (fstat(descriptor, &st) == -1)
   {
      I_Error("failure in fstat\n");
   }

   return st.st_size;
}

static boolean W_FILE_AddDir(w_handle_t handle, const char *path,
                             const char *start_marker, const char *end_marker)
{
    int startlump = numlumps;

    glob_t *glob;

    if (path[0] == '.')
    {
        glob = I_StartGlob(handle.p1.base_path, "*.*",
                           GLOB_FLAG_NOCASE | GLOB_FLAG_SORTED);
    }
    else
    {
        char *s = M_StringJoin(handle.p1.base_path, DIR_SEPARATOR_S, path);
        glob = I_StartGlob(s, "*.*", GLOB_FLAG_NOCASE | GLOB_FLAG_SORTED);
        free(s);
    }

    if (!glob)
    {
        return false;
    }

    while (true)
    {
        const char *filename = I_NextGlob(glob);
        if (filename == NULL)
        {
            break;
        }

        if (W_SkipFile(filename))
        {
            continue;
        }

        if (startlump == numlumps && start_marker)
        {
            W_AddMarker(start_marker);
        }

        int descriptor = M_open(filename, O_RDONLY | O_BINARY);
        if (descriptor == -1)
        {
            I_Error("Error opening %s", filename);
        }

        I_Printf(VB_INFO, " adding %s", filename);

        lumpinfo_t item = {0};
        W_ExtractFileBase(filename, item.name);
        item.size = FileLength(descriptor);

        item.module = &w_file_module;
        w_handle_t local_handle = {.p1.descriptor = descriptor,
                                   .priority = handle.priority};
        item.handle = local_handle;

        array_push(lumpinfo, item);
        numlumps++;
    }

    if (numlumps > startlump && end_marker)
    {
        W_AddMarker(end_marker);
    }

    return true;
}

static int *descriptors = NULL;

static w_type_t W_FILE_Open(const char *path, w_handle_t *handle)
{
    if (M_DirExists(path))
    {
        handle->p1.base_path = M_StringDuplicate(path);
        return W_DIR;
    }

    int descriptor = M_open(path, O_RDONLY | O_BINARY);
    if (descriptor == -1)
    {
        return W_NONE;
    }

    I_Printf(VB_INFO, " adding %s", path); // killough 8/8/98

    w_handle_t local_handle = {.p1.descriptor = descriptor,
                               .priority = handle->priority};

    // open the file and add to directory

    if (!M_StringCaseEndsWith(path, ".wad"))
    {
        array_push(descriptors, descriptor);

        lumpinfo_t item = {0};
        W_ExtractFileBase(path, item.name);
        item.size = FileLength(descriptor);
        item.module = &w_file_module;
        item.handle = local_handle;
        array_push(lumpinfo, item);
        numlumps++;
        return W_FILE;
    }

    // WAD file

    wadinfo_t header;

    if (read(descriptor, &header, sizeof(header)) == 0)
    {
        I_Printf(VB_WARNING, "Error reading header from %s (%s)", path,
                 strerror(errno));
        close(descriptor);
        return W_NONE;
    }

    if (strncmp(header.identification, "IWAD", 4)
        && strncmp(header.identification, "PWAD", 4))
    {
        close(descriptor);
        return W_NONE;
    }

    header.numlumps = LONG(header.numlumps);
    if (header.numlumps == 0)
    {
        I_Printf(VB_WARNING, "Wad file %s is empty", path);
        close(descriptor);
        return W_NONE;
    }

    int length = header.numlumps * sizeof(filelump_t);
    filelump_t *fileinfo = malloc(length);
    if (fileinfo == NULL)
    {
        I_Error("Failed to allocate file table from %s", path);
    }

    header.infotableofs = LONG(header.infotableofs);
    if (lseek(descriptor, header.infotableofs, SEEK_SET) == -1)
    {
        I_Printf(VB_WARNING, "Error seeking offset from %s (%s)", path,
                 strerror(errno));
        close(descriptor);
        free(fileinfo);
        return W_NONE;
    }

    if (read(descriptor, fileinfo, length) == 0)
    {
        I_Printf(VB_WARNING, "Error reading lump directory from %s (%s)", path,
                 strerror(errno));
        close(descriptor);
        free(fileinfo);
        return W_NONE;
    }

    array_push(descriptors, descriptor);

    numlumps += header.numlumps;

    const char *wadname = M_StringDuplicate(M_BaseName(path));
    array_push(wadfiles, wadname);

    for (int i = 0; i < header.numlumps; i++)
    {
        lumpinfo_t item = {0};
        M_CopyLumpName(item.name, fileinfo[i].name);
        item.size = LONG(fileinfo[i].size);

        item.module = &w_file_module;
        local_handle.p2.position = LONG(fileinfo[i].filepos);
        item.handle = local_handle;

        // [FG] WAD file that contains the lump
        item.wad_file = wadname;
        array_push(lumpinfo, item);
    }

    free(fileinfo);
    return W_FILE;
}

static void W_FILE_Read(w_handle_t handle, void *dest, int size)
{
    lseek(handle.p1.descriptor, handle.p2.position, SEEK_SET);
    int bytesread = read(handle.p1.descriptor, dest, size);
    if (bytesread < size)
    {
        I_Error("only read %d of %d", bytesread, size);
    }
}

static void W_FILE_Close(void)
{
    for (int i = 0; i < array_size(descriptors); ++i)
    {
        close(descriptors[i]);
    }
}

w_module_t w_file_module =
{
    W_FILE_AddDir,
    W_FILE_Open,
    W_FILE_Read,
    W_FILE_Close
};
