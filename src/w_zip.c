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

#include <stdlib.h>
#include <string.h>

#include "doomtype.h"
#include "i_printf.h"
#include "m_array.h"
#include "m_misc.h"
#include "w_wad.h"
#include "w_internal.h"

#include "miniz.h"

typedef struct
{
    int index;
    const char *filename;
} record_t;

struct archive_s
{
    mz_zip_archive *zip;
    record_t *directory;
};

static archive_t **archives;

static void AddWad(w_handle_t handle, int index, boolean is_map)
{
    mz_zip_archive *zip = handle.archive->zip;

    mz_zip_archive_file_stat stat;
    if (!mz_zip_reader_file_stat(zip, index, &stat))
    {
        I_Error("mz_zip_reader_file_stat failed");
    }

    byte *data = malloc(stat.m_uncomp_size);

    if (!data || !mz_zip_reader_extract_to_mem(zip, index, data, stat.m_uncomp_size, 0))
    {
        I_Error("mz_zip_reader_extract_to_mem failed");
    }

    char map_name[9] = {0};

    if (is_map)
    {
        // [ZDoom PK3] the WAD file name determines the map name
        W_ExtractFileBase(stat.m_filename, map_name);
    }

    // The WAD image is intentionally not freed: added lumps point into it.
    W_AddWadFromMemory(M_BaseName(stat.m_filename), data, stat.m_uncomp_size,
                       is_map ? map_name : NULL);
}

// [ZDoom PK3] Load a base directory in a single pass over the sorted
// archive directory, assigning files to the reserved directories on the
// fly: all entries of one directory are consecutive.
static boolean W_ZIP_AddDir(w_handle_t handle, const char *base)
{
    archive_t *archive = handle.archive;

    mz_zip_archive *zip = archive->zip;

    char *dir = M_StringDuplicate(base);
    W_ConvertSlashes(dir);

    boolean is_root = (dir[0] == '.');
    size_t dirlen = strlen(dir);

    int *wads = NULL; // WADs in the base directory, loaded last

    for (int i = 0; i < mz_zip_reader_get_num_files(zip); ++i)
    {
        const record_t record = archive->directory[i];

        mz_zip_archive_file_stat stat;
        mz_zip_reader_file_stat(zip, record.index, &stat);

        if (stat.m_is_directory)
        {
            continue;
        }

        char *name = M_StringDuplicate(record.filename);
        W_ConvertSlashes(name);

        // Entries outside the base directory belong to other AddDir
        // passes (other base directories of the same archive).
        if (!is_root
            && (strncasecmp(name, dir, dirlen) || name[dirlen] != '/'))
        {
            free(name);
            continue;
        }

        // [ZDoom PK3] file path relative to the base directory
        const char *relpath = is_root ? name : name + dirlen + 1;

        const w_dir_spec_t *spec = W_DirSpecOfFile(relpath);

        if (!spec && strchr(relpath, '/'))
        {
            free(name); // file in a non-reserved directory
            continue;
        }

        if (spec && spec->is_map)
        {
            // [ZDoom PK3] maps/ contains single-level WADs, named
            // after the level they hold
            if (M_StringCaseEndsWith(record.filename, ".wad"))
            {
                AddWad(handle, record.index, true);
            }
            free(name);
            continue;
        }

        if (!spec && M_StringCaseEndsWith(record.filename, ".wad"))
        {
            // [ZDoom PK3] WADs in the base directory are added to the
            // lump directory after all other files
            array_push(wads, record.index);
            free(name);
            continue;
        }

        W_BeginDirLump(spec);

        lumpinfo_t item = {0};

        W_ExtractFileBase(stat.m_filename, item.name);

        if (spec && spec->namespace == ns_sprites)
        {
            W_ConvertSpriteName(item.name);
        }

        item.size = stat.m_uncomp_size;

        // [ZDoom PK3] full path name for long name lookups, relative
        // to the base directory
        item.longname = M_StringDuplicate(relpath);

        item.module = &w_zip_module;
        w_handle_t local_handle = {.archive = archive,
                                   .index = record.index,
                                   .priority = handle.priority};
        item.handle = local_handle;

        array_push(lumpinfo, item);
        numlumps++;

        free(name);
    }

    W_FlushDirRun();

    for (int i = 0; i < array_size(wads); ++i)
    {
        AddWad(handle, wads[i], false);
    }
    array_free(wads);

    free(dir);
    return true;
}

static int compare_records(const void *a, const void *b)
{
    const record_t *arg1 = a;
    const record_t *arg2 = b;

    return strcasecmp(arg1->filename, arg2->filename);
}

static w_type_t W_ZIP_Open(const char *path, w_handle_t *handle)
{
    mz_zip_archive *zip = calloc(1, sizeof(*zip));

    if (!mz_zip_reader_init_file(zip, path, MZ_ZIP_FLAG_DO_NOT_SORT_CENTRAL_DIRECTORY))
    {
        free(zip);
        return W_NONE;
    }

    const int num_files = mz_zip_reader_get_num_files(zip);
    record_t *directory = malloc(num_files * sizeof(*directory));
    for (int i = 0; i < num_files; ++i)
    {
        directory[i].index = i;
        int size = mz_zip_reader_get_filename(zip, i, NULL, 0);
        char *filename = malloc(size);
        mz_zip_reader_get_filename(zip, i, filename, size);
        directory[i].filename = filename;
    }
    qsort(directory, num_files, sizeof(*directory), compare_records);

    I_Printf(VB_INFO, " adding %s", path);

    archive_t *archive = malloc(sizeof(*archive));
    archive->zip = zip;
    archive->directory = directory;

    array_push(archives, archive);
    handle->archive = archive;

    return W_DIR;
}

static void W_ZIP_Read(w_handle_t handle, void *dest, int size)
{
    boolean result = mz_zip_reader_extract_to_mem(
        handle.archive->zip, handle.index, dest, size, 0);

    if (!result)
    {
        I_Error("mz_zip_reader_extract_to_mem failed");
    }
}

static void W_ZIP_Close(void)
{
    for (int i = 0; i < array_size(archives); ++i)
    {
        mz_zip_reader_end(archives[i]->zip);
    }
}

w_module_t w_zip_module =
{
    W_ZIP_AddDir,
    W_ZIP_Open,
    W_ZIP_Read,
    W_ZIP_Close
};
