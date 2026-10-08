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
#include <string.h>

#include <SDL3/SDL.h>

#include "doomtype.h"
#include "i_printf.h"
#include "i_system.h"
#include "m_io.h"
#include "m_array.h"
#include "m_misc.h"
#include "m_swap.h"
#include "w_internal.h"
#include "w_wad.h"

// [ZDoom PK3] sub-directories of reserved directories are scanned
// automatically, so enumerate a directory tree recursively.
typedef struct
{
    char **files;
    boolean recursive;
} enum_args_t;

static SDL_EnumerationResult EnumerateDirectory(void *userdata,
                                                const char *dirname,
                                                const char *fname)
{
    enum_args_t *args = userdata;
    char *path = M_StringJoin(dirname, fname);

    if (M_DirExists(path))
    {
        if (args->recursive)
        {
            SDL_EnumerateDirectory(path, EnumerateDirectory, args);
        }
        free(path);
    }
    else
    {
        array_push(args->files, path);
    }

    return SDL_ENUM_CONTINUE;
}

static int compare_filenames(const void *a, const void *b)
{
    return strcasecmp(*(const char **)a, *(const char **)b);
}

static FILE **descriptors = NULL;

static boolean W_FILE_AddDir(w_handle_t handle, const char *path,
                             const w_dir_spec_t *spec)
{
    int startlump = numlumps;

    boolean is_root = (path[0] == '.');

    char *dir;

    if (is_root)
    {
        dir = M_StringDuplicate(handle.base_path);
    }
    else
    {
        dir = M_StringJoin(handle.base_path, DIR_SEPARATOR_S, path);
    }

    if (!M_DirExists(dir))
    {
        free(dir);
        return false;
    }

    enum_args_t args = {.files = NULL, .recursive = (spec != NULL)};

    SDL_EnumerateDirectory(dir, EnumerateDirectory, &args);
    free(dir);

    char **files = args.files;

    qsort(files, array_size(files), sizeof(*files), compare_filenames);

    char **wads = NULL; // WADs in the root directory, loaded last

    for (int i = 0; i < array_size(files); ++i)
    {
        const char *filename = files[i];

        if (spec && spec->is_map)
        {
            // [ZDoom PK3] maps/ contains single-level WADs, named
            // after the level they hold
            if (!M_StringCaseEndsWith(filename, ".wad"))
            {
                continue;
            }

            byte *data;
            int length = M_ReadFile(filename, &data);

            if (length <= 0 || data == NULL)
            {
                I_Printf(VB_WARNING, "Error reading %s", filename);
                continue;
            }

            char map_name[9] = {0};
            W_ExtractFileBase(filename, map_name);

            // The WAD image is intentionally not freed: added lumps point
            // into it.
            W_AddWadFromMemory(M_BaseName(filename), data, (size_t)length,
                               map_name);
            continue;
        }

        if (M_StringCaseEndsWith(filename, ".wad"))
        {
            // [ZDoom PK3] WADs in the root directory are added to the
            // lump directory after all other files
            array_push(wads, M_StringDuplicate(filename));
            continue;
        }

        if (startlump == numlumps && spec && spec->start_marker)
        {
            W_AddMarker(spec->start_marker);
        }

        FILE *descriptor = M_fopen(filename, "rb");
        if (descriptor == NULL)
        {
            I_Error("Error opening %s", filename);
        }

        I_Printf(VB_INFO, " adding %s", filename);

        lumpinfo_t item = {0};
        W_ExtractFileBase(filename, item.name);

        if (spec && spec->namespace == ns_sprites)
        {
            W_ConvertSpriteName(item.name);
        }

        item.size = M_FileLength(filename);

        item.module = &w_file_module;
        w_handle_t local_handle = {.descriptor = descriptor,
                                   .priority = handle.priority};
        item.handle = local_handle;

        array_push(descriptors, descriptor);

        // [ZDoom PK3] full path name for long name lookups, relative
        // to the loaded directory, with '/' separators
        char *longname =
            M_StringDuplicate(filename + strlen(handle.base_path) + 1);
        W_ConvertSlashes(longname);
        item.longname = longname;

        array_push(lumpinfo, item);
        numlumps++;
    }

    for (int i = 0; i < array_size(files); ++i)
    {
        free(files[i]);
    }
    array_free(files);

    if (numlumps > startlump && spec && spec->end_marker)
    {
        W_AddMarker(spec->end_marker);
    }

    for (int i = 0; i < array_size(wads); ++i)
    {
        byte *data;
        int length = M_ReadFile(wads[i], &data);

        if (length > 0 && data != NULL)
        {
            // The WAD image is intentionally not freed: added lumps point
            // into it.
            W_AddWadFromMemory(M_BaseName(wads[i]), data, (size_t)length,
                               NULL);
        }
        else
        {
            I_Printf(VB_WARNING, "Error reading %s", wads[i]);
        }
        free(wads[i]);
    }
    array_free(wads);

    return true;
}

static w_type_t W_FILE_Open(const char *path, w_handle_t *handle)
{
    if (M_DirExists(path))
    {
        handle->base_path = M_StringDuplicate(path);
        return W_DIR;
    }

    FILE *descriptor = M_fopen(path, "rb");
    if (descriptor == NULL)
    {
        return W_NONE;
    }

    I_Printf(VB_INFO, " adding %s", path); // killough 8/8/98

    w_handle_t local_handle = {.descriptor = descriptor,
                               .priority = handle->priority};

    // open the file and add to directory

    if (!M_StringCaseEndsWith(path, ".wad"))
    {
        array_push(descriptors, descriptor);

        lumpinfo_t item = {0};
        W_ExtractFileBase(path, item.name);
        item.size = M_FileLength(path);
        item.module = &w_file_module;
        item.handle = local_handle;
        array_push(lumpinfo, item);
        numlumps++;
        return W_FILE;
    }

    // WAD file

    wadinfo_t header;

    if (fread(&header, 1, sizeof(header), descriptor) < sizeof(header))
    {
        I_Printf(VB_WARNING, "Error reading header from %s (%s)", path,
                 strerror(errno));
        fclose(descriptor);
        return W_NONE;
    }

    if (strncmp(header.identification, "IWAD", 4)
        && strncmp(header.identification, "PWAD", 4))
    {
        fclose(descriptor);
        return W_NONE;
    }

    header.numlumps = LONG(header.numlumps);
    if (header.numlumps == 0)
    {
        I_Printf(VB_WARNING, "Wad file %s is empty", path);
        fclose(descriptor);
        return W_NONE;
    }

    int length = header.numlumps * sizeof(filelump_t);
    filelump_t *fileinfo = malloc(length);
    if (fileinfo == NULL)
    {
        I_Error("Failed to allocate file table from %s", path);
    }

    header.infotableofs = LONG(header.infotableofs);
    if (fseek(descriptor, header.infotableofs, SEEK_SET) == -1)
    {
        I_Printf(VB_WARNING, "Error seeking offset from %s (%s)", path,
                 strerror(errno));
        fclose(descriptor);
        free(fileinfo);
        return W_NONE;
    }

    if (fread(fileinfo, sizeof(filelump_t), header.numlumps, descriptor) < header.numlumps)
    {
        I_Printf(VB_WARNING, "Error reading lump directory from %s (%s)", path,
                 strerror(errno));
        fclose(descriptor);
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
        local_handle.position = LONG(fileinfo[i].filepos);
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
    fseek(handle.descriptor, handle.position, SEEK_SET);
    int bytesread = fread(dest, 1, size, handle.descriptor);
    if (bytesread < size)
    {
        I_Error("only read %d of %d", bytesread, size);
    }
}

static void W_FILE_Close(void)
{
    for (int i = 0; i < array_size(descriptors); ++i)
    {
        fclose(descriptors[i]);
    }
}

w_module_t w_file_module =
{
    W_FILE_AddDir,
    W_FILE_Open,
    W_FILE_Read,
    W_FILE_Close
};
