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

#include "doomtype.h"
#include "i_printf.h"
#include "m_array.h"
#include "m_misc.h"
#include "m_swap.h"
#include "w_wad.h"
#include "w_internal.h"

#include "miniz.h"

static void ConvertSlashes(char *path)
{
    for (char *p = path; *p; ++p)
    {
        if (*p == '\\')
        {
            *p = '/';
        }
    }
}

static void AddWadInMem(w_handle_t handle, const char *name, int index,
                        size_t data_size)
{
    I_Printf(VB_INFO, " - adding %s", name);

    mz_zip_archive *zip = handle.p1.zip;

    byte *data = malloc(data_size);

    if (!mz_zip_reader_extract_to_mem(zip, index, data, data_size, 0))
    {
        I_Error("AddWadInZip: mz_zip_reader_extract_to_mem failed");
    }

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

        item.handle = handle;
 
        // [FG] WAD file that contains the lump
        item.wad_file = wadname;
        array_push(lumpinfo, item);
    }
}

static boolean W_ZIP_AddDir(w_handle_t handle, const char *path,
                            const char *start_marker, const char *end_marker)
{
    mz_zip_archive *zip = handle.p1.zip;

    boolean is_root = (path[0] == '.');

    char *dir = M_StringDuplicate(path);
    ConvertSlashes(dir);

    int startlump = numlumps;

    for (int index = 0 ; index < mz_zip_reader_get_num_files(zip); ++index)
    {
        mz_zip_archive_file_stat stat;
        mz_zip_reader_file_stat(zip, index, &stat);

        if (stat.m_is_directory)
        {
            continue;
        }

        if (is_root && M_StringCaseEndsWith(stat.m_filename, ".wad"))
        {
            AddWadInMem(handle, M_BaseName(stat.m_filename), index,
                        stat.m_uncomp_size);
            continue;
        }

        char *name = M_DirName(stat.m_filename);
        if (strcasecmp(name, dir))
        {
            free(name);
            continue;
        }
        free(name);

        if (W_SkipFile(stat.m_filename))
        {
            continue;
        }

        if (startlump == numlumps && start_marker)
        {
            W_AddMarker(start_marker);
        }

        lumpinfo_t item = {0};

        W_ExtractFileBase(stat.m_filename, item.name);
        item.size = stat.m_uncomp_size;

        item.module = &w_zip_module;
        w_handle_t local_handle = {.p1.zip = zip, .p2.index = index,
                                   .priority = handle.priority};
        item.handle = local_handle;

        array_push(lumpinfo, item);
        numlumps++;
    }

    if (numlumps > startlump && end_marker)
    {
        W_AddMarker(end_marker);
    }

    free(dir);
    return true;
}

static mz_zip_archive **zips = NULL;

static w_type_t W_ZIP_Open(const char *path, w_handle_t *handle)
{
    mz_zip_archive *zip = calloc(1, sizeof(*zip));

    if (!mz_zip_reader_init_file(zip, path, 0))
    {
        free(zip);
        return W_NONE;
    }

    I_Printf(VB_INFO, " adding %s", path);

    handle->p1.zip = zip;
    array_push(zips, zip);
    return W_DIR;
}

static void W_ZIP_Read(w_handle_t handle, void *dest, int size)
{
    boolean result = mz_zip_reader_extract_to_mem(
        handle.p1.zip, handle.p2.index, dest, size, 0);

    if (!result)
    {
        I_Error("W_ZIP_Read: mz_zip_reader_extract_to_mem failed");
    }
}

static void W_ZIP_Close(void)
{
    for (int i = 0; i < array_size(zips); ++i)
    {
        mz_zip_reader_end(zips[i]);
    }
}

w_module_t w_zip_module =
{
    W_ZIP_AddDir,
    W_ZIP_Open,
    W_ZIP_Read,
    W_ZIP_Close
};
